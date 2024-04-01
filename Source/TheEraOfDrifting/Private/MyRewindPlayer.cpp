// Fill out your copyright notice in the Description page of Project Settings.


#include "RewindPlayer.h"

#include "MovieScene.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "LevelSequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include <filesystem>
//namespace fs = std::filesystem;

void ARewindPlayer::BeginPlay()
{
	Super::BeginPlay();

	if (UsePlayer)
	{
		BP_Actor = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	}
}

void ARewindPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (LevelSequenceAsset && SequencePlayer && SequencePlayer->IsPlaying())
	{
		OnRewindPlaying();
	}
}

void ARewindPlayer::PlayRewind(ULevelSequence* LevelSequence, float const& expectedFinishedTime)
{
	if (LevelSequence)
	{
		SetSequence(LevelSequence);
		
		BindActor();
		
		SequencePlayer->SetPlayRate(ComputePlayRate(expectedFinishedTime));

		SequencePlayer->PlayReverse();

		OnRewindStart();
	}
}

void ARewindPlayer::EndRewind()
{
	if (UsePlayer)
	{
		ACharacter* charac = reinterpret_cast<ACharacter*>(BP_Actor.Get());
		USkeletalMeshComponent* mesh = charac->GetMesh();
		mesh->SetAnimClass(AnimClass);
	}
	
	ClearLevelSequence();
}

void ARewindPlayer::ClearLevelSequence()
{
	SetSequence(nullptr);
}

void ARewindPlayer::UnbindActor()
{
	if (LevelSequenceAsset && SequencePlayer)
	{
		UMovieScene* movie_scene = LevelSequenceAsset->GetMovieScene();

		FGuid id = movie_scene->GetSpawnable(0).GetGuid();
		RemoveBinding(UE::MovieScene::FRelativeObjectBindingID(id), BP_Actor);
	}
}

void ARewindPlayer::BindActor()
{
	if (LevelSequenceAsset && SequencePlayer)
	{
		UMovieScene* movie_scene = LevelSequenceAsset->GetMovieScene();

		FGuid id = movie_scene->GetSpawnable(0).GetGuid();

		AddBinding(FMovieSceneObjectBindingID(UE::MovieScene::FRelativeObjectBindingID(id)), BP_Actor);
	}
}

float ARewindPlayer::ComputePlayRate(float const& expectedFinishedTime) const
{
	if (SequencePlayer)
	{
		auto const duration = SequencePlayer->GetDuration();
		return 5.f;// duration.AsSeconds() / expectedFinishedTime;
	}
	return 1.f;
}

float ARewindPlayer::GetRewindPercent() const
{
	if (SequencePlayer && SequencePlayer->IsPlaying())
	{
		auto const duration = SequencePlayer->GetDuration();
		auto const start = SequencePlayer->GetStartTime();
		auto const current = SequencePlayer->GetCurrentTime();

		return 1.f - static_cast<float>(current.Time.FrameNumber.Value - start.Time.FrameNumber.Value) / static_cast<float>(duration.Time.FrameNumber.Value);
	}
	return 0.f;
}
