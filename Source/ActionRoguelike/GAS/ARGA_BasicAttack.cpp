// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ARGA_BasicAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
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

	UAbilityTask_PlayMontageAndWait* MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage);
	// OnBlendOut is skipped on purpose: the ability (and State.Attacking) lasts until the montage fully ends.
	MontageTask->OnCompleted.AddDynamic(this, &UARGA_BasicAttack::OnMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &UARGA_BasicAttack::OnMontageEnded);
	MontageTask->OnCancelled.AddDynamic(this, &UARGA_BasicAttack::OnMontageEnded);
	MontageTask->ReadyForActivation();

	// One hit per activation, even if the montage has several notifies.
	UAbilityTask_WaitGameplayEvent* HitTask =
		UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TAG_Event_Attack_Hit.GetTag(), nullptr, true, true);
	HitTask->EventReceived.AddDynamic(this, &UARGA_BasicAttack::OnHitEvent);
	HitTask->ReadyForActivation();
}

void UARGA_BasicAttack::OnMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UARGA_BasicAttack::OnHitEvent(FGameplayEventData Payload)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World || !SourceASC || !DamageEffectClass)
	{
		return;
	}

	const FVector Center = Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * HitRange;

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ARBasicAttackHit), false, Avatar);
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(HitRadius), QueryParams);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugHit)
	{
		DrawDebugSphere(World, Center, HitRadius, 16, FColor::Red, false, 1.f);
	}
#endif

	TSet<AActor*> Damaged;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || Damaged.Contains(Target))
		{
			continue;
		}
		Damaged.Add(Target);

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC)
		{
			continue;
		}

		FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(TAG_Data_Damage.GetTag(), -Damage);
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		}
	}
}
