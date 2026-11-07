// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ARAttributeSet.h"

#include "ActionRoguelike.h"

UARAttributeSet::UARAttributeSet()
{
	InitMaxHealth(100.f);
	InitHealth(100.f);
}

void UARAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
}

void UARAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
}

void UARAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		UE_LOG(LogAR, Display, TEXT("%s Health %.0f -> %.0f"), *GetNameSafe(GetOwningActor()), OldValue, NewValue);
		OnHealthChanged.Broadcast(OldValue, NewValue, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
	{
		// Keep Health within the new upper bound (GAS does not reconcile separate attributes).
		SetHealth(NewValue);
	}
}
