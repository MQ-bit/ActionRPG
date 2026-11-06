// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/ARGameplayAbility.h"
#include "ARGA_BasicAttack.generated.h"

class UAnimMontage;

/** Plays the attack montage and holds State.Attacking until it ends. The montage is set in a data-only Blueprint. */
UCLASS()
class ACTIONROGUELIKE_API UARGA_BasicAttack : public UARGameplayAbility
{
	GENERATED_BODY()

public:
	UARGA_BasicAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UFUNCTION()
	void OnMontageEnded();

	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> AttackMontage;
};
