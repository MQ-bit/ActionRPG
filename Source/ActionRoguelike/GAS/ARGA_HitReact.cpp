// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ARGA_HitReact.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "Core/ARCombat.h"
#include "GameFramework/Actor.h"
#include "GAS/ARGameplayTags.h"

UARGA_HitReact::UARGA_HitReact()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	SetAssetTags(FGameplayTagContainer(TAG_Ability_HitReact.GetTag()));
	ActivationOwnedTags.AddTag(TAG_State_HitReact.GetTag());
	// A vault or mantle is not interrupted (damage still applies).
	ActivationBlockedTags.AddTag(TAG_State_Parkouring.GetTag());
	CancelAbilitiesWithTag.AddTag(TAG_Ability_Attack.GetTag());
	CancelAbilitiesWithTag.AddTag(TAG_Ability_Block.GetTag());

	for (const FGameplayTag& Event : { TAG_Event_Damaged.GetTag(), TAG_Event_Parried.GetTag() })
	{
		FAbilityTriggerData Trigger;
		Trigger.TriggerTag = Event;
		Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(Trigger);
	}
}

bool UARGA_HitReact::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (World && World->GetTimeSeconds() - LastReactEndTime < ReactCooldown)
	{
		return false;
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UARGA_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAnimMontage* Montage = FrontMontage;
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const AActor* Instigator = TriggerEventData ? TriggerEventData->Instigator.Get() : nullptr;
	if (TriggerEventData && TriggerEventData->EventTag == TAG_Event_Parried.GetTag())
	{
		Montage = ParriedMontage ? ParriedMontage.Get() : FrontMontage.Get();
	}
	else if (Avatar && Instigator && BackMontage
		&& ARCombat::HitDirection(Avatar->GetActorForwardVector(), Instigator->GetActorLocation() - Avatar->GetActorLocation()) == ARCombat::EHitDirection::Back)
	{
		Montage = BackMontage;
	}

	if (Montage && ActorInfo && ActorInfo->GetAnimInstance())
	{
		// The task is removed with the ability, so a finished reaction's callbacks never reach a later one.
		UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage);
		Task->OnCompleted.AddDynamic(this, &UARGA_HitReact::OnReactionFinished);
		Task->OnBlendOut.AddDynamic(this, &UARGA_HitReact::OnReactionFinished);
		Task->OnInterrupted.AddDynamic(this, &UARGA_HitReact::OnReactionFinished);
		Task->OnCancelled.AddDynamic(this, &UARGA_HitReact::OnReactionFinished);
		Task->ReadyForActivation();
		return;
	}

	UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, NoMontageDuration);
	Wait->OnFinish.AddDynamic(this, &UARGA_HitReact::OnReactionFinished);
	Wait->ReadyForActivation();
}

void UARGA_HitReact::OnReactionFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UARGA_HitReact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsActive())
	{
		const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
		LastReactEndTime = Avatar && Avatar->GetWorld() ? Avatar->GetWorld()->GetTimeSeconds() : LastReactEndTime;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
