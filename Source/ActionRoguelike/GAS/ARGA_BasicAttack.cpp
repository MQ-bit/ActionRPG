// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ARGA_BasicAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GAS/ARGameplayTags.h"

UARGA_BasicAttack::UARGA_BasicAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	InputTag = TAG_InputTag_Attack.GetTag();
	SetAssetTags(FGameplayTagContainer(TAG_Ability_Attack.GetTag()));
	// Granted to the owner while the ability is active; the character reads it to block movement.
	ActivationOwnedTags.AddTag(TAG_State_Attacking.GetTag());
}

void UARGA_BasicAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!AttackMontage || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_PlayMontageAndWait* Task =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage);
	// OnBlendOut is skipped on purpose: the ability (and State.Attacking) lasts until the montage fully ends.
	Task->OnCompleted.AddDynamic(this, &UARGA_BasicAttack::OnMontageEnded);
	Task->OnInterrupted.AddDynamic(this, &UARGA_BasicAttack::OnMontageEnded);
	Task->OnCancelled.AddDynamic(this, &UARGA_BasicAttack::OnMontageEnded);
	Task->ReadyForActivation();
}

void UARGA_BasicAttack::OnMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
