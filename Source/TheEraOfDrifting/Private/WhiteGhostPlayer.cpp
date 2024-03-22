// Fill out your copyright notice in the Description page of Project Settings.


#include "WhiteGhostPlayer.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneObjectBindingID.h"

void AWhiteGhostPlayer::BeginPlay()
{
	Super::BeginPlay();

    b_LateInit = false;

    // Spawn actors
    UWorld* World = GetWorld();
    if (World && BP)
    {
        BP_Actor = World->SpawnActor<AActor>(BP);
    }
}

void AWhiteGhostPlayer::Tick(float DeltaTime)
{
    if (!b_LateInit)
    {
        LateInit();
        b_LateInit = true;
    }
}

void AWhiteGhostPlayer::LateInit()
{
    //TArray<AActor*> actors{ BP_Actor };

    UMovieScene* movie_scene = LevelSequenceAsset->GetMovieScene();

    FGuid id = movie_scene->GetSpawnable(0).GetGuid();
    AddBinding(FMovieSceneObjectBindingID(UE::MovieScene::FRelativeObjectBindingID(id)), BP_Actor);
}
