// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "ISequencer.h"
#include "Recorder/TakeRecorderParameters.h"
#include "Serializers/MovieSceneManifestSerialization.h"
#include "MyTakeRecorder.generated.h"

/**
 * 
 */

class ISequencer;
class UTakePreset;
class UTakeMetaData;
class UTakeRecorder;
class ULevelSequence;
class SNotificationItem;
class UTakeRecorderSources;
class UTakeRecorderOverlayWidget;
class UMovieSceneSequence;

UENUM(BlueprintType)
enum class EMyTakeRecorderState : uint8
{
	CountingDown,
	PreRecord,
	TickingAfterPre,
	Started,
	Stopped,
	Cancelled,
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMyTakeRecordingPreInitialize, UMyTakeRecorder*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMyTakeRecordingInitialized, UMyTakeRecorder*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMyTakeRecordingStarted, UMyTakeRecorder*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMyTakeRecordingStopped, UMyTakeRecorder*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMyTakeRecordingFinished, UMyTakeRecorder*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMyTakeRecordingCancelled, UMyTakeRecorder*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMyStartPlayFrameModified, UMyTakeRecorder*, const FFrameNumber& StartFrame);

DECLARE_DELEGATE_RetVal_OneParam(FTakeRecorderParameters, FMyTakeRecorderParameterDelegate, const FTakeRecorderParameters&);

struct THEERAOFDRIFTING_API FMyTakeRecorderParameterOverride
{
	void RegisterHandler(FName OverrideName, FMyTakeRecorderParameterDelegate Delegate);
	void UnregisterHandler(FName OverrideName);

	TMap<FName, FMyTakeRecorderParameterDelegate> Delegates;
};

UCLASS(BlueprintType)
class THEERAOFDRIFTING_API UMyTakeRecorder : public UObject
{
public:

	GENERATED_BODY()

	UMyTakeRecorder(const FObjectInitializer& ObjInit);

public:

	/**
	 * Retrieve the currently active take recorder instance
	 */
	static UMyTakeRecorder* GetActiveRecorder();

	/**
	 * Retrieve a multi-cast delegate that is triggered when a new recording begins
	 */
	//static FOnTakeRecordingInitialized& OnRecordingInitialized();

	/**
	 * On take initialization overrides provide a mechanism to adjust parameter values when
	 * a new take is initialized.
	 */
	static FMyTakeRecorderParameterOverride& TakeInitializeParameterOverride();

public:

	/**
	 * Access the number of seconds remaining before this recording will start
	 */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	float GetCountdownSeconds() const
	{
		return CountdownSeconds;
	}

	/**
	 * Access the sequence asset that this recorder is recording into
	 */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	ULevelSequence* GetSequence() const
	{
		return SequenceAsset;
	}

	/**
	 * Get the current state of this recorder
	 */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	EMyTakeRecorderState GetState() const
	{
		return State;
	}

	/**
	 * Disable saving data on tick event. This make the TakeRecord a no-op.
	 */
	void SetDisableSaveTick(bool);

public:

	/**
	 * Initialize a new recording with the specified parameters. Fails if another recording is currently in progress.
	 *
	 * @param LevelSequenceBase   A level sequence to use as a base set-up for the take
	 * @param Sources             The sources to record from
	 * @param MetaData            Meta data to store on the recorded level sequence asset for this take
	 * @param Parameters          Configurable parameters for this instance of the recorder
	 * @param OutError            Error string to receive for context
	 * @return True if the recording process was successfully initialized, false otherwise
	 */
	bool Initialize(ULevelSequence* LevelSequenceBase, UTakeRecorderSources* Sources, UTakeMetaData* MetaData, const FTakeRecorderParameters& InParameters, FText* OutError = nullptr);

	/**
	 * Called to stop the recording
	 */
	void Stop();

	/**
	 * Called to cancel the recording
	 */
	void Cancel();

	/**
	 * Retrieve a multi-cast delegate that is triggered before initialization occurs (ie. when the recording button is pressed and before the countdown starts)
	 */
	FOnMyTakeRecordingPreInitialize& OnRecordingPreInitialize();

	/**
	 * Retrieve a multi-cast delegate that is triggered when this recording starts
	 */
	FOnMyTakeRecordingStarted& OnRecordingStarted();

	/**
	 * Retrieve a multi-cast delegate that is triggered when this recording is stopped
	 */
	FOnMyTakeRecordingStopped& OnRecordingStopped();

	/**
	 * Retrieve a multi-cast delegate that is triggered when this recording finishes
	 */
	FOnMyTakeRecordingFinished& OnRecordingFinished();

	/**
	 * Retrieve a multi-cast delegate that is triggered when this recording is cancelled
	 */
	FOnMyTakeRecordingCancelled& OnRecordingCancelled();

	/**
	 * Retrieve a multi-cast delegate that is triggered when a delta time has been applied to the Movie Scene
	 */
	FOnMyStartPlayFrameModified& OnStartPlayFrameModified();

public:

	/**
	 * Called after the countdown to PreRecord
	 */
	void PreRecord();

	/**
	 * Called after PreRecord To Start
	 */
	void Start();

	/*
	 * Stop or cancel
	 */
	void StopInternal(const bool bCancelled);

	/**
	 * Ticked by a tickable game object to performe any necessary time-sliced logic
	 */
	void Tick(float DeltaTime);

	/**
	 * Called if we're currently recording a PIE world that has been shut down or if we start PIE in a non-PIE world. Bound in Initialize, and unbound in Stop.
	 */
	void HandlePIE(bool bIsSimulating);

	/**
	 * Create a new destination asset to record into based on the parameters
	 */
	bool CreateDestinationAsset(const TCHAR* AssetPathFormat, ULevelSequence* LevelSequenceBase, UTakeRecorderSources* Sources, UTakeMetaData* MetaData, FText* OutError);

	/**
	 * Setup a existing asset to record into based on the parameters
	 */
	bool SetupDestinationAsset(const FTakeRecorderParameters& InParameters, ULevelSequence* LevelSequenceBase, UTakeRecorderSources* Sources, UTakeMetaData* MetaData, FText* OutError);

	/**
	 * Discovers the source world to record from, and initializes it for recording
	 */
	void DiscoverSourceWorld();

	/**
	 * Called to perform any initialization based on the user-provided parameters
	 */
	void InitializeFromParameters();

	/**
	 * Returns true if notification widgets should be shown when recording.
	 * It takes into account TakeRecorder project settings, the command line, and global unattended settings.
	 */
	bool ShouldShowNotifications();

public:

	/* The time at which to record. Taken from the Sequencer global time, otherwise based on timecode */
	FQualifiedFrameTime GetRecordTime() const;

	/** Called by Tick and Start to make sure we record at start */
	void InternalTick(float DeltaTime);

	virtual UWorld* GetWorld() const override;

public:

	/** The number of seconds remaining before Start() should be called */
	float CountdownSeconds;

	/** The state of this recorder instance */
	EMyTakeRecorderState State;

	/** FFrameTime in MovieScene Resolution we are at*/
	FFrameTime CurrentFrameTime;

	/** Timecode at the start of recording */
	FTimecode TimecodeAtStart;

	/** Optional frame to stop recording at*/
	TOptional<FFrameNumber> StopRecordingFrame;

	/** The asset that we should output recorded data into */
	//UPROPERTY(transient)
	TObjectPtr<ULevelSequence> SequenceAsset;

	/** The overlay widget for this recording */
	//UPROPERTY(transient)
	TObjectPtr<UTakeRecorderOverlayWidget> OverlayWidget;

	/** The world that we are recording within */
	//UPROPERTY(transient)
	TWeakObjectPtr<UWorld> WeakWorld;

	/** Parameters for the recorder - marked up as a uproperty to support reference collection */
	//UPROPERTY()
	FTakeRecorderParameters Parameters;

	/** Anonymous array of cleanup functions to perform when a recording has finished */
	TArray<TFunction<void()>> OnStopCleanup;

	/** Triggered before the recorder is initialized */
	FOnMyTakeRecordingPreInitialize OnRecordingPreInitializeEvent;

	/** Triggered when this recorder starts */
	FOnMyTakeRecordingStarted OnRecordingStartedEvent;

	/** Triggered when this recorder is stopped */
	FOnMyTakeRecordingStopped OnRecordingStoppedEvent;

	/** Triggered when this recorder finishes */
	FOnMyTakeRecordingFinished OnRecordingFinishedEvent;

	/** Triggered when this recorder is cancelled */
	FOnMyTakeRecordingCancelled OnRecordingCancelledEvent;

	/** Triggered when the movie scene is adjusted. */
	FOnMyStartPlayFrameModified OnFrameModifiedEvent;

	/** Sequencer ptr that controls playback of the desination asset during the recording */
	TWeakPtr<ISequencer> WeakSequencer;

	/** Due a few ticks after the pre so we are set up with asset creation */
	int32 NumberOfTicksAfterPre;

	friend class FTickableTakeRecorder;

public:

	/**
	 * Set the currently active take recorder instance
	 */
	static bool SetActiveRecorder(UMyTakeRecorder* NewActiveRecorder);

public:

	FManifestSerializer ManifestSerializer;

	EAllowEditsMode CachedAllowEditsMode;
	EAutoChangeMode CachedAutoChangeMode;
	EUpdateClockSource CachedClockSource;

	TRange<FFrameNumber> CachedPlaybackRange;

	FString SequenceName;
	ULevelSequence* RecordedSequence;
};
