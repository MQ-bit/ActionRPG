// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ARGameplayAbility.h"

#include "GAS/ARGameplayTags.h"

UARGameplayAbility::UARGameplayAbility()
{
	// A dead character cannot use abilities.
	ActivationBlockedTags.AddTag(TAG_State_Dead.GetTag());
}
