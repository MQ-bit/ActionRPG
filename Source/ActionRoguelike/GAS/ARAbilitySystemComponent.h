// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "ARAbilitySystemComponent.generated.h"

class UARGameplayAbility;

/** Project ASC. Abilities are bound to input by tag (UARGameplayAbility::InputTag). */
UCLASS()
class ACTIONROGUELIKE_API UARAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	/** Grants the ability and remembers its input tag on the spec. */
	void GiveStartupAbility(TSubclassOf<UARGameplayAbility> AbilityClass);

	/** Activates every granted ability bound to this input tag, or forwards the press to it if it is already active. */
	void AbilityInputTagPressed(const FGameplayTag& InputTag);
};
