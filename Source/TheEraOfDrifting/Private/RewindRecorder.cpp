// Fill out your copyright notice in the Description page of Project Settings.


#include "RewindRecorder.h"

#include "Recorder/TakeRecorderPanel.h"
#include "Recorder/TakeRecorderBlueprintLibrary.h"
#include "Recorder/TakeRecorderParameters.h"
#include "TakeRecorderSettings.h"
#include "TakeRecorderSource.h"
#include "TakeMetaData.h"
#include "Kismet/GameplayStatics.h"
#include "TakesCoreBlueprintLibrary.h"
#include "TakeRecorderSourceHelpers.h"
#include "Misc/Paths.h"
#include <filesystem>

#define LOCTEXT_NAMESPACE "MyReindRecorder"

// Sets default values
ARewindRecorder::ARewindRecorder()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ARewindRecorder::BeginPlay()
{
	Super::BeginPlay();

	has_Source = false;

	if (SaveToSequence)
	{
		InitSources();
		has_Source = true;
	}
}

void ARewindRecorder::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	UMyTakeRecorder* CurrentRecording = UMyTakeRecorder::GetActiveRecorder();
	if (CurrentRecording)
	{
		CurrentRecording->Stop();
		ResetForRecord();
	}
	if (has_Source)
	{
		UTakeRecorderSources* sources = SaveToSequence->FindMetaData<UTakeRecorderSources>();
		TakeRecorderSourceHelpers::RemoveAllActorSources(sources);
	}

	std::string const std_path = std::string(TCHAR_TO_UTF8(*SubSequencePath));
	if (std::filesystem::is_directory(std_path))
	{
		std::filesystem::remove_all(std_path);
	}
}

// Called every frame
void ARewindRecorder::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool ARewindRecorder::StartRecord()
{
	/*UTakeRecorderBlueprintLibrary::OpenTakeRecorderPanel();
	recordPanel = UTakeRecorderBlueprintLibrary::GetTakeRecorderPanel();
	if (recordPanel)
	{
		printf("test");
	}*/
	if (!SaveToSequence) return false;

	UTakeRecorderSources* sources = SaveToSequence->FindMetaData<UTakeRecorderSources>();
	//TArray<AActor*> actors{ UGameplayStatics::GetPlayerPawn(GetWorld(), 0) };
	//TakeRecorderSourceHelpers::AddActorSources(sources, actors);

	//UTakeMetaData* meta_data = nullptr;
	if (!sources)
	{
		sources = InitSources();
	}

	SaveToSequence->RemoveMetaData<UTakeMetaData>();

	//if (!meta_data)
	{
		if (TransientTakeMetaData)
		{
			TransientTakeMetaData->MarkAsGarbage();
			TransientTakeMetaData = nullptr;
		}
			
		//if (!TransientTakeMetaData)
		{
			TransientTakeMetaData = UTakeMetaData::CreateFromDefaults(GetTransientPackage(), NAME_None);
			TransientTakeMetaData->SetFlags(RF_Transactional | RF_Transient);

			FString DefaultSlate = GetDefault<UTakeRecorderProjectSettings>()->Settings.DefaultSlate;
			if (TransientTakeMetaData->GetSlate() != DefaultSlate)
			{
				TransientTakeMetaData->SetSlate(DefaultSlate, false);
			}

			// Compute the correct starting take number
			int32 NextTakeNumber = UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(TransientTakeMetaData->GetSlate());
			if (TransientTakeMetaData->GetTakeNumber() != NextTakeNumber)
			{
				TransientTakeMetaData->SetTakeNumber(NextTakeNumber, false);
			}
		}

		//meta_data = TransientTakeMetaData;
	}
	UTakeMetaData* meta_data = TransientTakeMetaData;

	FTakeRecorderParameters Parameters;
	Parameters.User = GetDefault<UTakeRecorderUserSettings>()->Settings;
	Parameters.User.bSaveRecordedAssets = false;
	Parameters.Project = GetDefault<UTakeRecorderProjectSettings>()->Settings;
	Parameters.TakeRecorderMode = ETakeRecorderMode::RecordIntoSequence;
	Parameters.StartFrame = SaveToSequence->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue();

	FText ErrorText = LOCTEXT("UnknownError", "An unknown error occurred when trying to start recording");

	UMyTakeRecorder* NewRecorder = NewObject<UMyTakeRecorder>(GetTransientPackage(), NAME_None, RF_Transient);

	if (!NewRecorder->Initialize(SaveToSequence, sources, meta_data, Parameters, &ErrorText))
	{
		if (ensure(!ErrorText.IsEmpty()))
		{
			OnRecordStartFailed(ErrorText);
		}
		return false;
	}
	return true;
}

void ARewindRecorder::StopRecord()
{
	UMyTakeRecorder* CurrentRecording = UMyTakeRecorder::GetActiveRecorder();

	if (CurrentRecording)
	{
		CurrentRecording->Stop();
		//RecordedSequence = CurrentRecording->RecordedSequence;
		CurrentRecording->SequenceName;

		static FString const temp = "/Game/TimeRewind/{0}_Subscenes/{1}{2}";
		static FString const temp_1 = "/Content/TimeRewind/{0}_Subscenes/";

		FString const project_path = FPaths::GetProjectFilePath();
		FString const project_folder = FPaths::GetPath(project_path);

		SubSequencePath = (project_folder + FString::Format(*temp_1, { SaveToSequence->GetName() }));

		//AnimationName = FString::Format(*temp, { SaveToSequence->GetName(), "", ""});
		
		FString const path = FString::Format(*temp, { SaveToSequence->GetName(), "", CurrentRecording->SequenceName });
		RecordedSequence = LoadObject<ULevelSequence>(nullptr, *path);

		OnRecordStopEvent();
	}
}

void ARewindRecorder::PauseRecord()
{
	UMyTakeRecorder* CurrentRecording = UMyTakeRecorder::GetActiveRecorder();

	if (!IsPause && CurrentRecording)
	{
		CurrentRecording->State = EMyTakeRecorderState::Pause;
		IsPause = true;
	}
}

void ARewindRecorder::ResumeRecord()
{
	UMyTakeRecorder* CurrentRecording = UMyTakeRecorder::GetActiveRecorder();

	if (IsPause && CurrentRecording)
	{
		CurrentRecording->State = EMyTakeRecorderState::Started;
		IsPause = false;
	}
}

void ARewindRecorder::PlayRewind()
{
	BP_RewindPlayer->PlayRewind(GetRecordedLevelSequence(), 3.f);

	ResetForRecord();
}

void ARewindRecorder::ResetForRecord()
{
	if (RecordedSequence)
	{
		RecordedSequence = nullptr;
	}
}

ULevelSequence* ARewindRecorder::GetRecordedLevelSequence()
{
	return RecordedSequence;
}

UTakeRecorderSources* ARewindRecorder::InitSources() const
{
	UTakeRecorderSources* sources = SaveToSequence->FindOrAddMetaData<UTakeRecorderSources>();
	TArray<AActor*> actors{ UGameplayStatics::GetPlayerPawn(GetWorld(), 0) };
	TakeRecorderSourceHelpers::AddActorSources(sources, actors);

	return sources;
}

