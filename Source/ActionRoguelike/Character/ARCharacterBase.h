// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ARCharacterBase.generated.h"

/** Shared base for player and enemy characters. The AbilitySystemComponent is added here in stage 2 (GAS). */
UCLASS(Abstract)
class ACTIONROGUELIKE_API AARCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AARCharacterBase();
};
