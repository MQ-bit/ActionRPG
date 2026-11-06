// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ARAttributeSet.h"

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
