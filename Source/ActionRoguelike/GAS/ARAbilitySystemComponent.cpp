// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ARAbilitySystemComponent.h"

#include "GAS/ARGameplayAbility.h"

void UARAbilitySystemComponent::GiveStartupAbility(TSubclassOf<UARGameplayAbility> AbilityClass)
{
	if (!AbilityClass)
	{
		return;
	}

	FGameplayAbilitySpec Spec(AbilityClass, 1);
	Spec.GetDynamicSpecSourceTags().AddTag(AbilityClass.GetDefaultObject()->GetInputTag());
	GiveAbility(Spec);
}

void UARAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	// Collect first: activation can change the activatable list.
	TArray<FGameplayAbilitySpecHandle> Handles;
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			Handles.Add(Spec.Handle);
		}
	}

	for (const FGameplayAbilitySpecHandle& Handle : Handles)
	{
		FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
		if (!Spec)
		{
			continue;
		}

		if (Spec->IsActive())
		{
			// Already running (e.g. a combo): let the ability decide what a repeated press means.
			AbilitySpecInputPressed(*Spec);
		}
		else
		{
			TryActivateAbility(Handle);
		}
	}
}
