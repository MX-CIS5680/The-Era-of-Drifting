// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"

#include "WhiteGhostPlayer.generated.h"

/**
 * 
 */
UCLASS()
class THEERAOFDRIFTING_API AWhiteGhostPlayer : public ALevelSequenceActor
{
	GENERATED_BODY()
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "EraOfDrifting")
	void LateInit();

	UFUNCTION(BlueprintCallable, Category = "EraOfDrifting")
	FORCEINLINE bool IsLateInit() const { return b_LateInit; }

	//UFUNCTION(BlueprintCallable, Category = "EraOfDrifting")
	//FORCEINLINE float GetRemainTime() const { 
	//	return SequencePlayer->GetSequence()-> - SequencePlayer->GetPlaybackPosition();
	//}

public:
	UPROPERTY(EditAnywhere, Category = "EraOfDrifting")
	TSubclassOf<class AActor> BP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EraOfDrifting")
	TObjectPtr<AActor> BP_Actor;

protected:
	bool b_LateInit;
};
