// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ARHealthBarWidget.h"

#include "Character/ARCharacterBase.h"
#include "Components/ProgressBar.h"
#include "GAS/ARAttributeSet.h"

float UARHealthBarWidget::ComputeHealthPercent(float Health, float MaxHealth)
{
	return MaxHealth > 0.f ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f;
}

void UARHealthBarWidget::BindToCharacter(AARCharacterBase* Character)
{
	Unbind();

	UARAttributeSet* Set = Character ? Character->GetAttributeSet() : nullptr;
	if (!Set)
	{
		return;
	}

	BoundSet = Set;
	Set->OnHealthChanged.AddUObject(this, &UARHealthBarWidget::HandleHealthChanged);
	SetPercent(ComputeHealthPercent(Set->GetHealth(), Set->GetMaxHealth()));
}

void UARHealthBarWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UARHealthBarWidget::HandleHealthChanged(float OldValue, float NewValue, float MaxHealth)
{
	SetPercent(ComputeHealthPercent(NewValue, MaxHealth));
}

void UARHealthBarWidget::SetPercent(float Percent)
{
	if (HealthBar)
	{
		HealthBar->SetPercent(Percent);
	}
}

void UARHealthBarWidget::Unbind()
{
	if (UARAttributeSet* Set = BoundSet.Get())
	{
		Set->OnHealthChanged.RemoveAll(this);
	}
	BoundSet.Reset();
}
