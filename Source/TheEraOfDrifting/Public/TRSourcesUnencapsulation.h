// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Templates/SubclassOf.h"
#include "Tickable.h"
#include "Misc/FrameRate.h"
#include "Misc/QualifiedFrameTime.h"
#include "Serializers/MovieSceneManifestSerialization.h"
#include "TakeRecorderSources.h"
#include "LevelSequence.h"
#include "TRSourcesUnencapsulation.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable)
class THEERAOFDRIFTING_API UTRSourcesUnencapsulation : public UObject
{
public:
	GENERATED_BODY()

private:
	/** A list of handlers to invoke when the sources list changes */
	DECLARE_MULTICAST_DELEGATE(FOnSourcesChanged);
	FOnSourcesChanged OnSourcesChangedEvent;

public:

	/** The array of all sources contained within this list */
	UPROPERTY(Instanced)
	TArray<TObjectPtr<UTakeRecorderSource>> Sources;

	/** Maps each source to the level sequence that was created for that source, or to the root source if a subsequence was not created. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UTakeRecorderSource>, TObjectPtr<ULevelSequence>> SourceSubSequenceMap;

	/** List of sub-sections that we're recording into. Needed to ensure they're all the right size at the end without re-adjusting every sub-section in a sequence. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMovieSceneSubSection>> ActiveSubSections;

	/** Are we currently in a recording pass and should be ticking our Sources? */
	bool bIsRecording;

	/** What Tick Resolution is the target level sequence we're recording into? Used to convert seconds into FrameNumbers. */
	FFrameRate TargetLevelSequenceTickResolution;

	/** What Display Rate is the target level sequence we're recording into? Used to convert seconds into FrameNumbers. */
	FFrameRate TargetLevelSequenceDisplayRate;

	/** Non-serialized serial number that is used for updating UI when the source list changes */
	uint32 SourcesSerialNumber;

	/** Sources settings */
	FTakeRecorderSourcesSettings Settings;

	/** Manifest Serializer that we are recording into. */
	FManifestSerializer* CachedManifestSerializer;

	/** Level Sequence that we are recording into. Cached so that new sources added mid-recording get placed in the right sequence. */
	ULevelSequence* CachedLevelSequence;

	/** Array of Allocated Serializers created for each sub sequence.  Deleted at the end of the recording so memory is freed. */
	TArray<TSharedPtr<FManifestSerializer>> CreatedManifestSerializers;

	/** All sources after PreRecord */
	TArray<UTakeRecorderSource*> PreRecordedSources;

	/** The last frame time during tick recording */
	FQualifiedFrameTime CachedFrameTime;
};
