// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "CheckPoint.generated.h"

UCLASS()
class THEERAOFDRIFTING_API ACheckPoint : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACheckPoint();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	FORCEINLINE bool Active() const { return IsActive; }

	UFUNCTION(BlueprintCallable)
	FORCEINLINE void Disable() { IsActive = false; DisableEffect(); }

	UFUNCTION(BlueprintImplementableEvent)
	void DisableEffect();

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UBoxComponent> BoxCollider;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool IsActive;
};
