// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GAS/ARComboState.h"
#include "GAS/ARGameplayAbility.h"
#include "ARGA_BasicAttack.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * Light attack combo. Plays ComboMontages one after another and holds State.Attacking until the combo ends.
 * Each montage sends Event.Attack.Hit (UARAnimNotify_AttackHit) at its hit frame: actors with an ASC in front of
 * the avatar take damage and the input window for the next step opens. Pressing attack inside the window queues
 * the next step, which starts as the current montage blends out. Values come from a data-only Blueprint.
 */
UCLASS()
class ACTIONROGUELIKE_API UARGA_BasicAttack : public UARGameplayAbility
{
	GENERATED_BODY()

public:
	UARGA_BasicAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Called by the ASC when attack is pressed while this ability is already active. */
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	void PlayStep(int32 StepIndex);
	void ApplyComboAction(FARComboState::EAction Action);

	// Montage delegates carry the step's playback id, so callbacks of a replaced step or ended ability are ignored.
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, int32 PlaybackId);
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 PlaybackId);

	UFUNCTION()
	void OnHitEvent(FGameplayEventData Payload);

	/** One montage per combo step, in order. Each needs an AR Attack Hit notify at its hit frame. */
	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	TArray<TObjectPtr<UAnimMontage>> ComboMontages;

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

private:
	FARComboState ComboState;
};
