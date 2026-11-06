// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "ARGameplayAbility.generated.h"

/** Ability base. InputTag links an input action to this ability through the ASC. */
UCLASS(Abstract)
class ACTIONROGUELIKE_API UARGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	const FGameplayTag& GetInputTag() const { return InputTag; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FGameplayTag InputTag;
};
