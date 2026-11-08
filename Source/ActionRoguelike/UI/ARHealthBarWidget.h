// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ARHealthBarWidget.generated.h"

class AARCharacterBase;
class UARAttributeSet;
class UProgressBar;

/**
 * Health bar for one character. All logic is here; the UMG Blueprint only lays out a ProgressBar named HealthBar.
 * Used for the player HUD and, through a WidgetComponent, for the bar above an enemy.
 */
UCLASS(Abstract)
class ACTIONROGUELIKE_API UARHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Fill of the bar, 0..1. A missing or zero MaxHealth reads as empty. */
	static float ComputeHealthPercent(float Health, float MaxHealth);

	/** Shows the character's current Health and follows its changes. Rebinding replaces the previous character. */
	void BindToCharacter(AARCharacterBase* Character);

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

private:
	void HandleHealthChanged(float OldValue, float NewValue, float MaxHealth);
	void SetPercent(float Percent);
	void Unbind();

	TWeakObjectPtr<UARAttributeSet> BoundSet;
};
