// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/ARCharacterBase.h"

#include "GAS/ARAbilitySystemComponent.h"
#include "GAS/ARAttributeSet.h"

AARCharacterBase::AARCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<UARAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UARAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AARCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AARCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// Owner and avatar are the same actor.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	for (const TSubclassOf<UARGameplayAbility>& AbilityClass : StartupAbilities)
	{
		AbilitySystemComponent->GiveStartupAbility(AbilityClass);
	}
}
