// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ARAttributeSet.generated.h"

#define AR_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** Broadcast after Health changed: old value, new value, current MaxHealth. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FAROnHealthChanged, float /*OldValue*/, float /*NewValue*/, float /*MaxHealth*/);

/** Core combat attributes. Modify them only through Gameplay Effects. */
UCLASS()
class ACTIONROGUELIKE_API UARAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UARAttributeSet();

	// Final (current) value clamp.
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	// Base value clamp: without it, overheal/overkill from instant effects accumulates in the base value.
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	/** Used by death handling now and by the HUD later. Unbind in EndPlay. */
	FAROnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Health;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MaxHealth;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSet, MaxHealth)
};
