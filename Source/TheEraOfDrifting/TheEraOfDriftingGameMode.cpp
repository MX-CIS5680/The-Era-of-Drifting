// Copyright Epic Games, Inc. All Rights Reserved.

#include "TheEraOfDriftingGameMode.h"
#include "TheEraOfDriftingCharacter.h"
#include "UObject/ConstructorHelpers.h"

ATheEraOfDriftingGameMode::ATheEraOfDriftingGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
