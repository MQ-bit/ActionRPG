// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/ARCharacterBase.h"
#include "AREnemyCharacter.generated.h"

/** Minimal concrete enemy: shares the ASC and attributes from the base. AI is added in stage 3 of the task board. */
UCLASS()
class ACTIONROGUELIKE_API AAREnemyCharacter : public AARCharacterBase
{
	GENERATED_BODY()
};
