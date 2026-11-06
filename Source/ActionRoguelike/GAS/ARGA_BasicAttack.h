// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GAS/ARGameplayAbility.h"
#include "ARGA_BasicAttack.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * Plays the attack montage and holds State.Attacking until it ends. When the montage sends Event.Attack.Hit
 * (UARAnimNotify_AttackHit), actors with an ASC in front of the avatar take damage. Values come from a data-only Blueprint.
 */
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

	UFUNCTION()
	void OnHitEvent(FGameplayEventData Payload);

	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Applied to every target hit. Uses SetByCaller Data.Damage. */
	UPROPERTY(EditDefaultsOnly, Category = "Attack|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Attack|Damage")
	float Damage = 20.f;

	/** Distance from the avatar to the center of the hit sphere (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Attack|Hit")
	float HitRange = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Attack|Hit")
	float HitRadius = 100.f;

	/** Draws the hit sphere for a moment. For tuning only. */
	UPROPERTY(EditDefaultsOnly, Category = "Attack|Hit")
	bool bDrawDebugHit = false;
};
