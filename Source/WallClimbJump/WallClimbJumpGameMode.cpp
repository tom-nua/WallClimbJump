// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallClimbJumpGameMode.h"
#include "WallClimbJumpCharacter.h"
#include "UObject/ConstructorHelpers.h"

AWallClimbJumpGameMode::AWallClimbJumpGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
