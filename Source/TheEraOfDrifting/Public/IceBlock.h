// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Resetable.h"
#include "IceBlock.generated.h"

UCLASS()
class THEERAOFDRIFTING_API AIceBlock : public AActor, public IResetable
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Ice Block")
	FTransform Init_Transform;

	UPROPERTY(EditAnywhere, Category = "Ice Block")
	FVector Init_Velocity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice Block")
	UStaticMeshComponent* StaticMeshComponent;

public:	
	// Sets default values for this actor's properties
	AIceBlock();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void Reset_Implementation() override;

private:
	UPrimitiveComponent* m_PrimitiveComponent;
};
