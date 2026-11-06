// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ARGE_Damage.h"

#include "GAS/ARAttributeSet.h"
#include "GAS/ARGameplayTags.h"

UARGE_Damage::UARGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = TAG_Data_Damage.GetTag();

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UARAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(Modifier);
}
