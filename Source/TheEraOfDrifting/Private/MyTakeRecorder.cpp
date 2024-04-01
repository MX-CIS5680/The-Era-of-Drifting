// Fill out your copyright notice in the Description page of Project Settings.


#include "MyTakeRecorder.h"

#include "Recorder/TakeRecorder.h"

#include "Algo/Accumulate.h"
#include "AnimatedRange.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Recorder/TakeRecorderBlueprintLibrary.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ISequencer.h"
#include "LevelSequence.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "MovieSceneTimeHelpers.h"
#include "ObjectTools.h"
#include "TakeMetaData.h"
#include "TakeRecorderSource.h"
#include "TakeRecorderSources.h"
#include "TakesUtils.h"
#include "Tickable.h"
#include "Stats/Stats.h"
#include "SequencerSettings.h"

//#include "TrackRecorders/MovieSceneAnimationTrackRecorder.h"

// Engine includes
#include "GameFramework/WorldSettings.h"
#include "UObject/SavePackage.h"

// UnrealEd includes
#include "Editor.h"

#include "UObject/GCObjectScopeGuard.h"

// Slate includes
#include "Framework/Notifications/NotificationManager.h"

// LevelEditor includes

#include "TRSourcesUnencapsulation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MyTakeRecorder)

#define LOCTEXT_NAMESPACE "MyTakeRecorder"

DEFINE_LOG_CATEGORY(ManifestSerialization);

class FMyTickableTakeRecorder : public FTickableGameObject
{
public:

	TWeakObjectPtr<UMyTakeRecorder> WeakRecorder;

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FMyTickableTakeRecorder, STATGROUP_Tickables);
	}

	//Make sure it always ticks, otherwise we can miss recording, in particularly when time code is always increasing throughout the system.
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }

	virtual bool IsTickableInEditor() const override
	{
		return true;
	}

	virtual UWorld* GetTickableGameObjectWorld() const override
	{
		UMyTakeRecorder* Recorder = WeakRecorder.Get();
		return Recorder ? Recorder->GetWorld() : nullptr;
	}

	virtual void Tick(float DeltaTime) override
	{
		if (UMyTakeRecorder* Recorder = WeakRecorder.Get())
		{
			Recorder->Tick(DeltaTime);
		}
	}
};

FMyTickableTakeRecorder TickableTakeRecorder;

// Static members of UTakeRecorder
static TStrongObjectPtr<UMyTakeRecorder>& GetCurrentRecorder()
{
	static TStrongObjectPtr<UMyTakeRecorder> CurrentRecorder;
	return CurrentRecorder;
}

void FMyTakeRecorderParameterOverride::RegisterHandler(FName OverrideName, FMyTakeRecorderParameterDelegate Delegate)
{
	Delegates.FindOrAdd(MoveTemp(OverrideName), MoveTemp(Delegate));
}

void  FMyTakeRecorderParameterOverride::UnregisterHandler(FName OverrideName)
{
	Delegates.Remove(MoveTemp(OverrideName));
}

// Static functions for UMyTakeRecorder
UMyTakeRecorder* UMyTakeRecorder::GetActiveRecorder()
{
	return GetCurrentRecorder().Get();
}

FMyTakeRecorderParameterOverride& UMyTakeRecorder::TakeInitializeParameterOverride()
{
	static FMyTakeRecorderParameterOverride Overrides;
	return Overrides;
}

bool UMyTakeRecorder::SetActiveRecorder(UMyTakeRecorder* NewActiveRecorder)
{
	if (GetCurrentRecorder().IsValid())
	{
		return false;
	}

	GetCurrentRecorder().Reset(NewActiveRecorder);
	TickableTakeRecorder.WeakRecorder = GetCurrentRecorder().Get();
	return true;
}

// Non-static api for UMyTakeRecorder

UMyTakeRecorder::UMyTakeRecorder(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	CountdownSeconds = 0.f;
	SequenceAsset = nullptr;
	OverlayWidget = nullptr;
}

void UMyTakeRecorder::SetDisableSaveTick(bool InValue)
{
	Parameters.bDisableRecordingAndSave = InValue;
}

namespace TakeInitHelper
{
	FTakeRecorderParameters AccumulateParamsOverride(const FTakeRecorderParameters& InParam)
	{
		TMap<FName, FMyTakeRecorderParameterDelegate>& TheDelegates = UMyTakeRecorder::TakeInitializeParameterOverride().Delegates;
		auto Op = [](FTakeRecorderParameters InParameters, const TPair<FName, FMyTakeRecorderParameterDelegate>& Pair)
			{
				return Pair.Value.Execute(MoveTemp(InParameters));
			};
		return Algo::Accumulate(TheDelegates, InParam, MoveTemp(Op));
	}
}

bool UMyTakeRecorder::Initialize(ULevelSequence* LevelSequenceBase, UTakeRecorderSources* Sources, UTakeMetaData* MetaData, const FTakeRecorderParameters& InParameters, FText* OutError)
{
	FGCObjectScopeGuard GCGuard(this);

	if (GetActiveRecorder())
	{
		if (OutError)
		{
			*OutError = LOCTEXT("RecordingInProgressError", "A recording is currently in progress.");
		}
		return false;
	}

	if (MetaData->GetSlate().IsEmpty())
	{
		if (OutError)
		{
			*OutError = LOCTEXT("NoSlateSpecifiedError", "No slate specified.");
		}
		return false;
	}

	//OnRecordingPreInitializeEvent.Broadcast(this);

	UTakeRecorderBlueprintLibrary::OnTakeRecorderPreInitialize();

	FTakeRecorderParameters FinalParameters = TakeInitHelper::AccumulateParamsOverride(InParameters);
	if (FinalParameters.TakeRecorderMode == ETakeRecorderMode::RecordNewSequence)
	{
		if (!CreateDestinationAsset(*FinalParameters.Project.GetTakeAssetPath(), LevelSequenceBase, Sources, MetaData, OutError))
		{
			return false;
		}
	}
	else
	{
		if (!SetupDestinationAsset(FinalParameters, LevelSequenceBase, Sources, MetaData, OutError))
		{
			return false;
		}
	}

	// -----------------------------------------------------------
	// Anything after this point assumes successful initialization
	// -----------------------------------------------------------

	AddToRoot();

	Parameters = FinalParameters;
	State = EMyTakeRecorderState::CountingDown;

	// Override parameters for recording into a current sequence
	if (Parameters.TakeRecorderMode == ETakeRecorderMode::RecordIntoSequence)
	{
		Parameters.Project.bStartAtCurrentTimecode = false;
		Parameters.User.bStopAtPlaybackEnd = true;
		Parameters.User.bAutoLock = false;
	}

	if (Parameters.Project.RecordingClockSource == EUpdateClockSource::Timecode)
	{
		Parameters.Project.bStartAtCurrentTimecode = true;
		UE_LOG(LogTakesCore, Warning, TEXT("Overriding Start at Current Timecode to True since the clock source is synced to Timecode."));
	}

	// Perform any other parameter-configurable initialization. Must have a valid world at this point.
	InitializeFromParameters();

	// Figure out which world we're recording from
	DiscoverSourceWorld();

	// Open a recording notification

	if (ShouldShowNotifications())
	{
		/*TSharedRef<STakeRecorderNotification> Content = SNew(STakeRecorderNotification, this);

		FNotificationInfo Info(Content);
		Info.bFireAndForget = false;
		Info.ExpireDuration = 5.f;

		TSharedPtr<SNotificationItem> PendingNotification = FSlateNotificationManager::Get().AddNotification(Info);
		Content->SetOwner(PendingNotification);*/
	}

	ensure(SetActiveRecorder(this));

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		// If a start frame was specified, adjust the playback range before rewinding to the beginning of the playback range
		UMovieScene* MovieScene = SequenceAsset->GetMovieScene();
		MovieScene->SetPlaybackRange(TRange<FFrameNumber>(Parameters.StartFrame, MovieScene->GetPlaybackRange().GetUpperBoundValue()));

		// Always start the recording at the beginning of the playback range
		Sequencer->SetLocalTime(MovieScene->GetPlaybackRange().GetLowerBoundValue());

		if (Parameters.TakeRecorderMode == ETakeRecorderMode::RecordNewSequence)
		{
			USequencerSettings* SequencerSettings = USequencerSettingsContainer::GetOrCreate<USequencerSettings>(TEXT("TakeRecorderSequenceEditor"));

			CachedAllowEditsMode = SequencerSettings->GetAllowEditsMode();
			CachedAutoChangeMode = SequencerSettings->GetAutoChangeMode();

			//When we start recording we don't want to track anymore.  It will be restored when stopping recording.
			SequencerSettings->SetAllowEditsMode(EAllowEditsMode::AllEdits);
			SequencerSettings->SetAutoChangeMode(EAutoChangeMode::None);

			Sequencer->SetSequencerSettings(SequencerSettings);
		}

		// Center the view range around the current time about to be captured
		FAnimatedRange Range = Sequencer->GetViewRange();
		FTimecode CurrentTime = FApp::GetTimecode();
		FFrameRate FrameRate = MovieScene->GetDisplayRate();
		FFrameNumber ViewRangeStart = CurrentTime.ToFrameNumber(FrameRate);
		double ViewRangeStartSeconds = Parameters.Project.bStartAtCurrentTimecode ? FrameRate.AsSeconds(ViewRangeStart) : MovieScene->GetPlaybackRange().GetLowerBoundValue() / MovieScene->GetTickResolution();
		FAnimatedRange NewRange(ViewRangeStartSeconds - 0.5f, ViewRangeStartSeconds + (Range.GetUpperBoundValue() - Range.GetLowerBoundValue()) + 0.5f);
		Sequencer->SetViewRange(NewRange, EViewRangeInterpolation::Immediate);
		Sequencer->SetClampRange(Sequencer->GetViewRange());
	}

	return true;
}

void UMyTakeRecorder::DiscoverSourceWorld()
{
	WeakWorld = TakesUtils::DiscoverSourceWorld();

	bool bPlayInGame = WeakWorld->WorldType == EWorldType::PIE || WeakWorld->WorldType == EWorldType::Game;
	// If recording via PIE, be sure to stop recording cleanly when PIE ends
	if (bPlayInGame)
	{
		// If CountdownSeconds is zero and the framerate is high, then we can create the overlay but it
		// never ends up being visible. However, when the framerate is low (e.g. in debug) it does show
		// for a single frame, which is undesirable, so only make it if it's going to last some time.
		if (CountdownSeconds > 0)
		{
			//UClass* Class = StaticLoadClass(UTakeRecorderOverlayWidget::StaticClass(), nullptr, TEXT("/Takes/UMG/DefaultRecordingOverlay.DefaultRecordingOverlay_C"));
			//if (Class)
			//{
			//	OverlayWidget = CreateWidget<UTakeRecorderOverlayWidget>(WeakWorld.Get(), Class);
			//	OverlayWidget->SetFlags(RF_Transient);
			//	//OverlayWidget->SetRecorder(this);
			//	OverlayWidget->AddToViewport();
			//}
		}
		//FEditorDelegates::EndPIE.AddUObject(this, &UMyTakeRecorder::HandlePIE);
	}
	// If not recording via PIE, be sure to stop recording if PIE Starts
	if (!bPlayInGame)
	{
		//FEditorDelegates::BeginPIE.AddUObject(this, &UMyTakeRecorder::HandlePIE);//reuse same function
	}
}

bool UMyTakeRecorder::CreateDestinationAsset(const TCHAR* AssetPathFormat, ULevelSequence* LevelSequenceBase, UTakeRecorderSources* Sources, UTakeMetaData* MetaData, FText* OutError)
{
	check(LevelSequenceBase && Sources && MetaData);

	FString   PackageName = MetaData->GenerateAssetPath(AssetPathFormat);

	// Initialize a new package, ensuring that it has a unique name
	if (!TakesUtils::CreateNewAssetPackage<ULevelSequence>(PackageName, SequenceAsset, OutError, LevelSequenceBase))
	{
		return false;
	}

	// Copy the sources into the level sequence for future reference (and potentially mutation throughout recording)
	SequenceAsset->CopyMetaData(Sources);

	UMovieScene* MovieScene = SequenceAsset->GetMovieScene();
	UTakeMetaData* AssetMetaData = SequenceAsset->CopyMetaData(MetaData);

	// Ensure the asset meta-data is unlocked for the recording (it is later Locked when the recording finishes)
	AssetMetaData->Unlock();
	AssetMetaData->ClearFlags(RF_Transient);

	FDateTime UtcNow = FDateTime::UtcNow();
	AssetMetaData->SetTimestamp(UtcNow);

	// @todo: duration / tick resolution / sample rate / frame rate needs some clarification between sync clocks, template sequences and meta data
	if (AssetMetaData->GetDuration() > 0)
	{
		TRange<FFrameNumber> PlaybackRange = TRange<FFrameNumber>::Inclusive(0, ConvertFrameTime(AssetMetaData->GetDuration(), AssetMetaData->GetFrameRate(), MovieScene->GetTickResolution()).CeilToFrame());
		MovieScene->SetPlaybackRange(PlaybackRange);
	}
	MovieScene->SetDisplayRate(AssetMetaData->GetFrameRate());

	SequenceAsset->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(SequenceAsset);

	WeakSequencer = TakesUtils::OpenSequencer(SequenceAsset, OutError);
	return WeakSequencer.IsValid();
}

bool UMyTakeRecorder::SetupDestinationAsset(const FTakeRecorderParameters& InParameters, ULevelSequence* LevelSequenceBase, UTakeRecorderSources* Sources, UTakeMetaData* MetaData, FText* OutError)
{
	check(LevelSequenceBase && Sources && MetaData);

	//WeakSequencer = TakesUtils::OpenSequencer(LevelSequenceBase, OutError);
	//if (!WeakSequencer.IsValid())
	//{
	//	return false;
	//}

	// The SequenceAsset is either LevelSequenceBase or the currently focused sequence
	SequenceAsset = LevelSequenceBase;// Cast<ULevelSequence>(WeakSequencer.Pin()->GetFocusedMovieSceneSequence());

	// Copy the sources into the level sequence for future reference (and potentially mutation throughout recording)
	SequenceAsset->CopyMetaData(Sources);

	UMovieScene* MovieScene = SequenceAsset->GetMovieScene();
	UTakeMetaData* AssetMetaData = SequenceAsset->CopyMetaData(MetaData);

	// Ensure the asset meta-data is unlocked for the recording (it is later Locked when the recording finishes)
	AssetMetaData->Unlock();
	AssetMetaData->ClearFlags(RF_Transient);

	FDateTime UtcNow = FDateTime::UtcNow();
	AssetMetaData->SetTimestamp(UtcNow);

	// When recording into an existing level sequence, set the asset metadata to the sequence's display rate
	if (InParameters.TakeRecorderMode == ETakeRecorderMode::RecordIntoSequence)
	{
		AssetMetaData->SetFrameRate(MovieScene->GetDisplayRate());
	}

	SequenceAsset->MarkPackageDirty();

	return true;
}

void UMyTakeRecorder::InitializeFromParameters()
{
	// Initialize the countdown delay
	// I want to start record immediately
	CountdownSeconds = 0.1f;// Parameters.User.CountdownSeconds;

	// Set the end recording frame if enabled
	//StopRecordingFrame = Parameters.User.bStopAtPlaybackEnd ? SequenceAsset->GetMovieScene()->GetPlaybackRange().GetUpperBoundValue() : TOptional<FFrameNumber>();
	
	// We want it to stop until we trigger stop
	StopRecordingFrame = TOptional<FFrameNumber>();

	// Apply immersive mode if the parameters demand it
	if (Parameters.User.bMaximizeViewport)
	{
		//TSharedPtr<IAssetViewport> ActiveLevelViewport = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor").GetFirstActiveViewport();

		// If it's already immersive we just leave it alone
		//if (ActiveLevelViewport.IsValid() && !ActiveLevelViewport->IsImmersive())
		//{
		//	ActiveLevelViewport->MakeImmersive(true/*bWantImmersive*/, false/*bAllowAnimation*/);

		//	// Restore it when we're done
		//	auto RestoreImmersiveMode = [WeakViewport = TWeakPtr<IAssetViewport>(ActiveLevelViewport)]
		//		{
		//			if (TSharedPtr<IAssetViewport> CleaupViewport = WeakViewport.Pin())
		//			{
		//				CleaupViewport->MakeImmersive(false/*bWantImmersive*/, false/*bAllowAnimation*/);
		//			}
		//		};
		//	OnStopCleanup.Add(RestoreImmersiveMode);
		//}
	}
}

bool UMyTakeRecorder::ShouldShowNotifications()
{
	// -TAKERECORDERISHEADLESS in the command line can force headless behavior and disable the notifications.
	static const bool bCmdLineTakeRecorderIsHeadless = FParse::Param(FCommandLine::Get(), TEXT("TAKERECORDERISHEADLESS"));

	return Parameters.Project.bShowNotifications
		&& !bCmdLineTakeRecorderIsHeadless
		&& !FApp::IsUnattended()
		&& !GIsRunningUnattendedScript;
}

UWorld* UMyTakeRecorder::GetWorld() const
{
	return WeakWorld.Get();
}

void UMyTakeRecorder::Tick(float DeltaTime)
{
	if (State == EMyTakeRecorderState::CountingDown)
	{
		NumberOfTicksAfterPre = 0;
		CountdownSeconds = FMath::Max(0.f, CountdownSeconds - DeltaTime);
		if (CountdownSeconds > 0.f)
		{
			return;
		}
		PreRecord();
	}
	else if (State == EMyTakeRecorderState::PreRecord)
	{
		if (++NumberOfTicksAfterPre == 2) //seems we need 2 ticks to make sure things are settled
		{
			State = EMyTakeRecorderState::TickingAfterPre;
		}
	}
	else if (State == EMyTakeRecorderState::TickingAfterPre)
	{
		NumberOfTicksAfterPre = 0;
		Start();
		InternalTick(0.0f);
	}
	else if (State == EMyTakeRecorderState::Started)
	{
		InternalTick(DeltaTime);
	}
}

FQualifiedFrameTime UMyTakeRecorder::GetRecordTime() const
{
	return TakesUtils::GetRecordTime(WeakSequencer.Pin(), SequenceAsset, TimecodeAtStart, Parameters.Project.bStartAtCurrentTimecode);
}

void UMyTakeRecorder::InternalTick(float DeltaTime)
{
	UE::MovieScene::FScopedSignedObjectModifyDefer FlushOnTick(true);

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	FQualifiedFrameTime RecordTime = GetRecordTime();

	UTakeRecorderSources* Sources = SequenceAsset->FindOrAddMetaData<UTakeRecorderSources>();
	if (!Parameters.bDisableRecordingAndSave)
	{
		CurrentFrameTime = Sources->TickRecording(SequenceAsset, RecordTime, DeltaTime);
	}
	else
	{
		CurrentFrameTime = Sources->AdvanceTime(RecordTime, DeltaTime);
	}

	if (Sequencer.IsValid())
	{
		FAnimatedRange Range = Sequencer->GetViewRange();
		UMovieScene* MovieScene = SequenceAsset->GetMovieScene();
		if (MovieScene)
		{
			FFrameRate FrameRate = MovieScene->GetTickResolution();
			double CurrentTimeSeconds = FrameRate.AsSeconds(CurrentFrameTime) + 0.5f;
			CurrentTimeSeconds = CurrentTimeSeconds > Range.GetUpperBoundValue() ? CurrentTimeSeconds : Range.GetUpperBoundValue();
			TRange<double> NewRange(Range.GetLowerBoundValue(), CurrentTimeSeconds);
			Sequencer->SetViewRange(NewRange, EViewRangeInterpolation::Immediate);
			Sequencer->SetClampRange(Sequencer->GetViewRange());
		}
	}

	if (StopRecordingFrame.IsSet() && CurrentFrameTime.FrameNumber >= StopRecordingFrame.GetValue())
	{
		Stop();
	}
}

void UMyTakeRecorder::PreRecord()
{
	State = EMyTakeRecorderState::PreRecord;

	UTakeRecorderSources* Sources = SequenceAsset->FindMetaData<UTakeRecorderSources>();
	check(Sources);
	UTakeMetaData* AssetMetaData = SequenceAsset->FindMetaData<UTakeMetaData>();

	//Set the flag to specify if we should auto save the serialized data or not when recording.

	MovieSceneSerializationNamespace::bAutoSerialize = Parameters.User.bAutoSerialize;
	if (Parameters.User.bAutoSerialize)
	{
		FString AssetName = AssetMetaData->GenerateAssetPath(Parameters.Project.GetTakeAssetPath());
		FString AssetPath = FPaths::ProjectSavedDir() + AssetName;
		FPaths::RemoveDuplicateSlashes(AssetPath);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.DirectoryExists(*AssetPath))
		{
			PlatformFile.CreateDirectoryTree(*AssetPath);
		}

		ManifestSerializer.SetLocalCaptureDir(AssetPath);
		FName SerializedType("Sequence");
		FString Name = SequenceAsset->GetName();
		FManifestFileHeader Header(Name, SerializedType, FGuid());
		FText Error;
		FString FileName = FString::Printf(TEXT("%s_%s"), *(SerializedType.ToString()), *(Name));

		if (!ManifestSerializer.OpenForWrite(FileName, Header, Error))
		{
			UE_LOG(ManifestSerialization, Warning, TEXT("Error Opening Sequence Sequencer File: Subject '%s' Error '%s'"), *(Name), *(Error.ToString()));
		}
	}

	FTakeRecorderSourcesSettings TakeRecorderSourcesSettings;
	TakeRecorderSourcesSettings.bStartAtCurrentTimecode = Parameters.Project.bStartAtCurrentTimecode;
	TakeRecorderSourcesSettings.bRecordSourcesIntoSubSequences = Parameters.Project.bRecordSourcesIntoSubSequences;
	TakeRecorderSourcesSettings.bRecordToPossessable = Parameters.Project.bRecordToPossessable;
	TakeRecorderSourcesSettings.bSaveRecordedAssets = (!Parameters.bDisableRecordingAndSave && Parameters.User.bSaveRecordedAssets);// || GEditor == nullptr;
	TakeRecorderSourcesSettings.bRemoveRedundantTracks = Parameters.User.bRemoveRedundantTracks;
	TakeRecorderSourcesSettings.bAutoLock = Parameters.User.bAutoLock;

	Sources->SetSettings(TakeRecorderSourcesSettings);

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!Parameters.bDisableRecordingAndSave)
	{
		UMovieScene* MovieScene = SequenceAsset->GetMovieScene();
		FQualifiedFrameTime SequencerTime;
		if (MovieScene)
		{
			FTimecode Timecode = FApp::GetTimecode();
			FFrameRate FrameRate = MovieScene->GetDisplayRate();
			FFrameRate TickResolution = MovieScene->GetTickResolution();
			FFrameNumber PlaybackStartFrame = Parameters.Project.bStartAtCurrentTimecode ? FFrameRate::TransformTime(FFrameTime(Timecode.ToFrameNumber(FrameRate)), FrameRate, TickResolution).FloorToFrame() : MovieScene->GetPlaybackRange().GetLowerBoundValue();
			SequencerTime = FQualifiedFrameTime(PlaybackStartFrame, TickResolution);
		}

		Sources->PreRecording(SequenceAsset, SequencerTime, Parameters.User.bAutoSerialize ? &ManifestSerializer : nullptr);
	}
	else
	{
		Sources->SetCachedAssets(SequenceAsset, Parameters.User.bAutoSerialize ? &ManifestSerializer : nullptr);
	}

	// Refresh sequencer in case the movie scene data has mutated (ie. existing object bindings removed because they will be recorded again)
	if (Sequencer.IsValid())
	{
		Sequencer->RefreshTree();
	}

	// Apply engine Time Dilation after the countdown, otherwise the countdown will be dilated as well!
	UWorld* RecordingWorld = GetWorld();
	check(RecordingWorld);
	if (AWorldSettings* WorldSettings = RecordingWorld->GetWorldSettings())
	{
		const float ExistingCinematicTimeDilation = WorldSettings->CinematicTimeDilation;

		const bool bInvalidTimeDilation = Parameters.User.EngineTimeDilation == 0.f;

		if (bInvalidTimeDilation)
		{
			UE_LOG(LogTakesCore, Warning, TEXT("Time dilation cannot be 0. Ignoring time dilation for this recording."));
		}

		if (Parameters.User.EngineTimeDilation != ExistingCinematicTimeDilation && !bInvalidTimeDilation)
		{
			WorldSettings->CinematicTimeDilation = Parameters.User.EngineTimeDilation;

			// Restore it when we're done
			auto RestoreTimeDilation = [ExistingCinematicTimeDilation, WeakWorldSettings = MakeWeakObjectPtr(WorldSettings)]
				{
					if (AWorldSettings* CleaupWorldSettings = WeakWorldSettings.Get())
					{
						CleaupWorldSettings->CinematicTimeDilation = ExistingCinematicTimeDilation;
					}
				};
			OnStopCleanup.Add(RestoreTimeDilation);
		}
	}
}

void UMyTakeRecorder::Start()
{
	FTimecode Timecode = FApp::GetTimecode();

	State = EMyTakeRecorderState::Started;

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

	CurrentFrameTime = FFrameTime(0);
	TimecodeAtStart = Timecode;

	// Discard any entity tokens we have so that restore state does not take effect when we delete any sections that recording will be replacing.
	if (Sequencer.IsValid())
	{
		Sequencer->PreAnimatedState.DiscardEntityTokens();
	}

	UMovieScene* MovieScene = SequenceAsset->GetMovieScene();
	if (MovieScene)
	{
		CachedPlaybackRange = MovieScene->GetPlaybackRange();
		CachedClockSource = MovieScene->GetClockSource();
		MovieScene->SetClockSource(Parameters.Project.RecordingClockSource);
		if (Sequencer.IsValid())
		{
			Sequencer->ResetTimeController();
		}

		FFrameRate FrameRate = MovieScene->GetDisplayRate();
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FFrameNumber PlaybackStartFrame = Parameters.Project.bStartAtCurrentTimecode ? FFrameRate::TransformTime(FFrameTime(Timecode.ToFrameNumber(FrameRate)), FrameRate, TickResolution).FloorToFrame() : MovieScene->GetPlaybackRange().GetLowerBoundValue();

		// Transform all the sections to start the playback start frame
		FFrameNumber DeltaFrame = PlaybackStartFrame - MovieScene->GetPlaybackRange().GetLowerBoundValue();
		if (DeltaFrame != 0)
		{
			for (UMovieSceneSection* Section : MovieScene->GetAllSections())
			{
				Section->MoveSection(DeltaFrame);
			}
		}

		//OnFrameModifiedEvent.Broadcast(this, PlaybackStartFrame);

		if (StopRecordingFrame.IsSet())
		{
			StopRecordingFrame = StopRecordingFrame.GetValue() + DeltaFrame;
		}

		// Set infinite playback range when starting recording. Playback range will be clamped to the bounds of the sections at the completion of the recording
		MovieScene->SetPlaybackRange(TRange<FFrameNumber>(PlaybackStartFrame, TNumericLimits<int32>::Max() - 1), false);
		if (Sequencer.IsValid())
		{
			Sequencer->SetGlobalTime(PlaybackStartFrame);
			Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Playing);
		}
	}
	UTakeRecorderSources* Sources = SequenceAsset->FindMetaData<UTakeRecorderSources>();
	check(Sources);

	UTakeMetaData* AssetMetaData = SequenceAsset->FindMetaData<UTakeMetaData>();
	FDateTime UtcNow = FDateTime::UtcNow();
	AssetMetaData->SetTimestamp(UtcNow);
	AssetMetaData->SetTimecodeIn(Timecode);

	if (!Parameters.bDisableRecordingAndSave)
	{
		FQualifiedFrameTime RecordTime = GetRecordTime();

		Sources->StartRecording(SequenceAsset, RecordTime, Parameters.User.bAutoSerialize ? &ManifestSerializer : nullptr);

		// Record immediately so that there's a key on the first frame of recording
		Sources->TickRecording(SequenceAsset, RecordTime, 0.1f);
	}

	if (!ShouldShowNotifications())
	{
		// Log in lieu of the notification widget
		UE_LOG(LogTakesCore, Log, TEXT("Started recording"));
	}

	//OnRecordingStartedEvent.Broadcast(this);

	UTakeRecorderBlueprintLibrary::OnTakeRecorderStarted();
}

void UMyTakeRecorder::Stop()
{
	const bool bCancelled = false;
	StopInternal(bCancelled);
}

void UMyTakeRecorder::Cancel()
{
	const bool bCancelled = true;
	StopInternal(bCancelled);
}

void UMyTakeRecorder::StopInternal(const bool bCancelled)
{
	static bool bStoppedRecording = false;

	if (bStoppedRecording)
	{
		return;
	}

	double StartTime = FPlatformTime::Seconds();

	TGuardValue<bool> ReentrantGuard(bStoppedRecording, true);

	if (Parameters.TakeRecorderMode == ETakeRecorderMode::RecordNewSequence)
	{
		USequencerSettings* SequencerSettings = USequencerSettingsContainer::GetOrCreate<USequencerSettings>(TEXT("TakeRecorderSequenceEditor"));

		SequencerSettings->SetAllowEditsMode(CachedAllowEditsMode);
		SequencerSettings->SetAutoChangeMode(CachedAutoChangeMode);
	}

	ManifestSerializer.Close();

	//FEditorDelegates::EndPIE.RemoveAll(this);
	//FEditorDelegates::BeginPIE.RemoveAll(this);

	const bool bDidEverStartRecording = State == EMyTakeRecorderState::Started;
	const bool bRecordingFinished = !bCancelled && bDidEverStartRecording;
	State = bRecordingFinished ? EMyTakeRecorderState::Stopped : EMyTakeRecorderState::Cancelled;

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
	}

	UMovieScene* MovieScene = SequenceAsset->GetMovieScene();
	if (MovieScene)
	{
		MovieScene->SetClockSource(CachedClockSource);

		if (Sequencer.IsValid())
		{
			Sequencer->ResetTimeController();
		}
	}

	if (bDidEverStartRecording)
	{
		if (!ShouldShowNotifications())
		{
			// Log in lieu of the notification widget
			if (bRecordingFinished)
			{
				UE_LOG(LogTakesCore, Log, TEXT("Stopped recording"));
			}
			else
			{
				UE_LOG(LogTakesCore, Log, TEXT("Recording cancelled"));
			}
		}

		//OnRecordingStoppedEvent.Broadcast(this);
		UTakeRecorderBlueprintLibrary::OnTakeRecorderStopped();

		if (MovieScene)
		{
			TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();

			//Set Range to what we recorded instead of that large number, this let's us reliably set camera cut times.
			FFrameNumber PlaybackEndFrame = CurrentFrameTime.FrameNumber;

			if (StopRecordingFrame.IsSet())
			{
				PlaybackEndFrame = StopRecordingFrame.GetValue();
			}

			MovieScene->SetPlaybackRange(TRange<FFrameNumber>(Range.GetLowerBoundValue(), PlaybackEndFrame));

			if (Sequencer)
			{
				Sequencer->ResetTimeController();
			}
		}
		//SourceSubSequenceMap;

		if (!Parameters.bDisableRecordingAndSave)
		{
			UTakeRecorderSources* Sources = SequenceAsset->FindMetaData<UTakeRecorderSources>();
			check(Sources);

			FTakeRecorderSourcesSettings settings = Sources->GetSettings();
			settings.bSaveRecordedAssets = false;
			Sources->SetSettings(settings);
			UTRSourcesUnencapsulation* unencapsulation_sources = reinterpret_cast<UTRSourcesUnencapsulation*>(Sources);
			
			TMap<TObjectPtr<UTakeRecorderSource>, TObjectPtr<ULevelSequence>>& source_map = unencapsulation_sources->SourceSubSequenceMap;
			
			for (auto Source : unencapsulation_sources->Sources)
			{
				RecordedSequence = source_map[Source];
				SequenceName = source_map[Source]->GetName();
			}
			Sources->StopRecording(SequenceAsset, bCancelled);
		}

		// Restore the playback range to what it was before recording.
		if (Parameters.TakeRecorderMode == ETakeRecorderMode::RecordIntoSequence)
		{
			if (MovieScene)
			{
				MovieScene->SetPlaybackRange(CachedPlaybackRange);
			}
		}

		if (bRecordingFinished)
		{
			// Lock the sequence so that it can't be changed without implicitly unlocking it now
			if (Parameters.User.bAutoLock)
			{
				SequenceAsset->GetMovieScene()->SetReadOnly(true);
			}

			UTakeMetaData* AssetMetaData = SequenceAsset->FindMetaData<UTakeMetaData>();
			check(AssetMetaData);

			if (MovieScene)
			{
				FFrameRate DisplayRate = MovieScene->GetDisplayRate();
				FFrameRate TickResolution = MovieScene->GetTickResolution();
				FTimecode Timecode = FTimecode::FromFrameNumber(FFrameRate::TransformTime(CurrentFrameTime, TickResolution, DisplayRate).FloorToFrame(), DisplayRate);

				AssetMetaData->SetTimecodeOut(Timecode);
			}

			//if (GEditor && GEditor->GetEditorWorldContext().World())
			//{
			//	AssetMetaData->SetLevelOrigin(GEditor->GetEditorWorldContext().World()->PersistentLevel);
			//}

			// Lock the meta data so it can't be changed without implicitly unlocking it now
			AssetMetaData->Lock();

			if (Parameters.User.bSaveRecordedAssets)
			{
				//TakesUtils::SaveAsset(SequenceAsset);
			}

			// Rebuild sequencer because subsequences could have been added or bindings removed
			if (Sequencer)
			{
				Sequencer->RefreshTree();
			}
		}
	}

	// Perform any other cleanup that has been defined for this recording
	for (const TFunction<void()>& Cleanup : OnStopCleanup)
	{
		Cleanup();
	}
	OnStopCleanup.Reset();

	// reset the current recorder and stop us from being ticked
	if (GetCurrentRecorder().Get() == this)
	{
		GetCurrentRecorder().Reset();
		TickableTakeRecorder.WeakRecorder = nullptr;

		if (bRecordingFinished)
		{
			//OnRecordingFinishedEvent.Broadcast(this);
			UTakeRecorderBlueprintLibrary::OnTakeRecorderFinished(SequenceAsset);
		}
		else
		{
			//OnRecordingCancelledEvent.Broadcast(this);
			UTakeRecorderBlueprintLibrary::OnTakeRecorderCancelled();
		}
	}

	// Delete the asset after OnTakeRecorderFinished and OnTakeRecorderCancelled because the ScopedSequencerPanel 
	// will still have the current sequence before it is returned to the Pending Take.
	if (Parameters.TakeRecorderMode == ETakeRecorderMode::RecordNewSequence && !bRecordingFinished)
	{
		if (GIsEditor)
		{
			// Recording was canceled before it started, so delete the asset. Note we can only do this on editor
			// nodes. On -game nodes, this cannot be performed. This can only happen with Mult-user and -game node
			// recording.
			//
			FAssetRegistryModule::AssetDeleted(SequenceAsset);
		}

		// Move the asset to the transient package so that new takes with the same number can be created in its place
		FName DeletedPackageName = MakeUniqueObjectName(nullptr, UPackage::StaticClass(), *(FString(TEXT("/Temp/") + SequenceAsset->GetName() + TEXT("_Cancelled"))));
		SequenceAsset->GetOutermost()->Rename(*DeletedPackageName.ToString());

		SequenceAsset->ClearFlags(RF_Standalone | RF_Public);
		SequenceAsset->RemoveFromRoot();
		SequenceAsset->MarkAsGarbage();
		SequenceAsset = nullptr;
	}

	//if (OverlayWidget)
	//{
	//	OverlayWidget->RemoveFromParent();
	//	OverlayWidget->RemoveFromRoot();
	//	OverlayWidget->MarkAsGarbage();
	//	OverlayWidget = nullptr;
	//}

	if (SequenceAsset)
	{
		UPackage* const Package = SequenceAsset->GetOutermost();
		FString const PackageName = Package->GetName();

		double ElapsedTime = FPlatformTime::Seconds() - StartTime;
		UE_LOG(LogTakesCore, Log, TEXT("Finished processing %s in %0.2f seconds"), *PackageName, ElapsedTime);
	}

	RemoveFromRoot();
}

FOnMyTakeRecordingPreInitialize& UMyTakeRecorder::OnRecordingPreInitialize()
{
	return OnRecordingPreInitializeEvent;
}

FOnMyTakeRecordingStarted& UMyTakeRecorder::OnRecordingStarted()
{
	return OnRecordingStartedEvent;
}

FOnMyTakeRecordingStopped& UMyTakeRecorder::OnRecordingStopped()
{
	return OnRecordingStoppedEvent;
}

FOnMyTakeRecordingFinished& UMyTakeRecorder::OnRecordingFinished()
{
	return OnRecordingFinishedEvent;
}

FOnMyTakeRecordingCancelled& UMyTakeRecorder::OnRecordingCancelled()
{
	return OnRecordingCancelledEvent;
}

FOnMyStartPlayFrameModified& UMyTakeRecorder::OnStartPlayFrameModified()
{
	return OnFrameModifiedEvent;
}

void UMyTakeRecorder::HandlePIE(bool bIsSimulating)
{
	ULevelSequence* FinishedAsset = GetSequence();

	if (ShouldShowNotifications())
	{
	}

	Stop();

	if (FinishedAsset)
	{
		//GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(FinishedAsset);
	}
}

#undef LOCTEXT_NAMESPACE