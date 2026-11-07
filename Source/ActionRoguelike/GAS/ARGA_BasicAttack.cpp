// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ARGA_BasicAttack.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
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
	if (ComboMontages.IsEmpty() || !ComboMontages[0] || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Not "only once": every combo step sends its own hit event.
	UAbilityTask_WaitGameplayEvent* HitTask =
		UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TAG_Event_Attack_Hit.GetTag(), nullptr, false, true);
	HitTask->EventReceived.AddDynamic(this, &UARGA_BasicAttack::OnHitEvent);
	HitTask->ReadyForActivation();

	PlayStep(0);
}

void UARGA_BasicAttack::PlayStep(int32 StepIndex)
{
	UAnimMontage* Montage = ComboMontages.IsValidIndex(StepIndex) ? ComboMontages[StepIndex].Get() : nullptr;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	if (!Montage || !ASC || !AnimInstance || ASC->PlayMontage(this, CurrentActivationInfo, Montage, 1.f) <= 0.f)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	ComboIndex = StepIndex;
	bComboWindowOpen = false;
	bNextStepQueued = false;

	FOnMontageBlendingOutStarted BlendingOut;
	BlendingOut.BindUObject(this, &UARGA_BasicAttack::OnMontageBlendingOut);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendingOut, Montage);

	FOnMontageEnded Ended;
	Ended.BindUObject(this, &UARGA_BasicAttack::OnMontageEnded);
	AnimInstance->Montage_SetEndDelegate(Ended, Montage);
}

void UARGA_BasicAttack::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	// One queued step at most, and only after the current step's hit frame.
	if (bComboWindowOpen && ComboMontages.IsValidIndex(ComboIndex + 1))
	{
		bNextStepQueued = true;
	}
}

void UARGA_BasicAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// A cancelled ability must not leave its montage playing.
	if (bWasCancelled && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		ActorInfo->AbilitySystemComponent->CurrentMontageStop();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UARGA_BasicAttack::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (!ComboMontages.IsValidIndex(ComboIndex) || Montage != ComboMontages[ComboIndex])
	{
		return;
	}

	if (bInterrupted)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (bNextStepQueued)
	{
		PlayStep(ComboIndex + 1);
	}
}

void UARGA_BasicAttack::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// The ability (and State.Attacking) lasts until the last played montage has fully ended.
	if (!ComboMontages.IsValidIndex(ComboIndex) || Montage != ComboMontages[ComboIndex])
	{
		return;
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bInterrupted);
}

void UARGA_BasicAttack::OnHitEvent(FGameplayEventData Payload)
{
	// The hit frame opens the input window for the next step.
	bComboWindowOpen = true;

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
