// Fill out your copyright notice in the Description page of Project Settings.


#include "IceBlock.h"

// Sets default values
AIceBlock::AIceBlock()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	RootComponent = StaticMeshComponent;
}

// Called when the game starts or when spawned
void AIceBlock::BeginPlay()
{
	Super::BeginPlay();

	Init_Transform = GetActorTransform();
	//m_PrimitiveComponent = nullptr;
	//m_PrimitiveComponent = FindComponentByClass<UPrimitiveComponent>();
	
	UE_LOG(LogTemp, Warning, TEXT("Ice Block Begin Play!\n"));
	StaticMeshComponent->SetPhysicsLinearVelocity(Init_Velocity);
}

// Called every frame
void AIceBlock::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	//double dis2 = FVector::Distance(Init_Transform.GetLocation(), GetActorTransform().GetLocation());
	//if (dis2 > 1000.0)
	//{
	//	Reset_Implementation();
	//}
}

void AIceBlock::Reset_Implementation()
{
	if (StaticMeshComponent)
	{
		// Set the velocity to zero
		StaticMeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
		StaticMeshComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	// TODO: make reset a progress
	SetActorTransform(Init_Transform);

	StaticMeshComponent->SetPhysicsLinearVelocity(Init_Velocity);
}

