// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceActor.h"
#include "MyTakeRecorder.h"
#include "MovieScene.h"
#include "LevelSequencePlayer.h"
#include "RewindPlayer.h"
#include "Recorder/TakeRecorderPanel.h"
#include "RewindRecorder.generated.h"

UCLASS()
class THEERAOFDRIFTING_API ARewindRecorder : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ARewindRecorder();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	bool StartRecord();

	UFUNCTION(BlueprintCallable)
	void StopRecord();

	UFUNCTION(BlueprintCallable)
	void PlayRewind();

	UFUNCTION(BlueprintCallable)
	void ResetForRecord();

	UFUNCTION(BlueprintCallable)
	ULevelSequence* GetRecordedLevelSequence();

	UFUNCTION(BlueprintImplementableEvent)
	void OnRecordStartFailed(FText const& text);

	UFUNCTION(BlueprintImplementableEvent)
	void OnRecordStartEvent();

	UFUNCTION(BlueprintImplementableEvent)
	void OnRecordStopEvent();

	UFUNCTION(BlueprintCallable)
	FORCEINLINE bool HasRecorded() const { return RecordedSequence != nullptr; }

private:
	UTakeRecorderSources* InitSources() const;

public:
	FString SubSequencePath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UMyTakeRecorder> TakeRecorder;

	TObjectPtr<UTakeMetaData> TransientTakeMetaData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<ARewindPlayer> BP_RewindPlayer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<ULevelSequence> SaveToSequence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<ULevelSequence> RecordedSequence;

	bool has_Source;
};
