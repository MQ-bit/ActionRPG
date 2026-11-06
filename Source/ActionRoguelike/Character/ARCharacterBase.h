// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "ARCharacterBase.generated.h"

class UARAbilitySystemComponent;
class UARAttributeSet;

/** Shared base for player and enemy characters. Owns the ASC and attributes (single-player: no PlayerState ownership). */
UCLASS(Abstract)
class ACTIONROGUELIKE_API AARCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AARCharacterBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UARAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UARAttributeSet> AttributeSet;
};
