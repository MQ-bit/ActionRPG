// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/ARGameplayAbility.h"
#include "ARGA_HitReact.generated.h"

class UAnimMontage;

/**
 * Short reaction to a hit, for the player and the enemies. Triggered by Event.Damaged (sent to the target of a hit) and
 * Event.Parried (sent to an attacker whose hit was parried). It cancels a running attack and blocks movement while active.
 * A hit during a reaction only deals damage (no retrigger), and a new reaction waits ReactCooldown after the last one ended,
 * so a combo or several attackers cannot lock the character.
 */
UCLASS()
class ACTIONROGUELIKE_API UARGA_HitReact : public UARGameplayAbility
{
	GENERATED_BODY()

public:
	UARGA_HitReact();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UFUNCTION()
	void OnReactionFinished();

	/** Hit from the front. */
	UPROPERTY(EditDefaultsOnly, Category = "HitReact")
	TObjectPtr<UAnimMontage> FrontMontage;

	/** Hit from behind. Empty = FrontMontage. */
	UPROPERTY(EditDefaultsOnly, Category = "HitReact")
	TObjectPtr<UAnimMontage> BackMontage;

	/** Longer stagger after being parried. Empty = FrontMontage. */
	UPROPERTY(EditDefaultsOnly, Category = "HitReact")
	TObjectPtr<UAnimMontage> ParriedMontage;

	/** Reaction length when there is no montage or no anim instance (tests, a missing asset). */
	UPROPERTY(EditDefaultsOnly, Category = "HitReact", meta = (ClampMin = "0"))
	float NoMontageDuration = 0.4f;

	/** Seconds after a reaction ended before the next one can start. */
	UPROPERTY(EditDefaultsOnly, Category = "HitReact", meta = (ClampMin = "0"))
	float ReactCooldown = 0.4f;

private:
	double LastReactEndTime = -1000.0;
};
