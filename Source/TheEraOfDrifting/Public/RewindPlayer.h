// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceActor.h"
#include "RewindPlayer.generated.h"

/**
 * 
 */
UCLASS()
class THEERAOFDRIFTING_API ARewindPlayer : public ALevelSequenceActor
{
	GENERATED_BODY()
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	// Called every frame
	virtual void Tick(float DeltaTime) override;

public:
	UFUNCTION(BlueprintCallable)
	void PlayRewind(ULevelSequence* LevelSequence, float const& expectedFinishedTime);

	UFUNCTION(BlueprintCallable)
	void EndRewind();

	UFUNCTION(BlueprintCallable)
	void ClearLevelSequence();

	UFUNCTION(BlueprintImplementableEvent)
	void OnRewindStart();

	UFUNCTION(BlueprintImplementableEvent)
	void OnRewindPlaying();

	UFUNCTION(BlueprintCallable)
	void UnbindActor();

	UFUNCTION(BlueprintCallable)
	void BindActor();

protected:
	UFUNCTION(BlueprintCallable)
	float ComputePlayRate(float const& expectedFinishedTime) const;

	UFUNCTION(BlueprintCallable)
	float GetRewindPercent() const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EraOfDrifting")
	TObjectPtr<AActor> BP_Actor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Animation)
	class TSubclassOf<UAnimInstance> AnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool UsePlayer;
};
