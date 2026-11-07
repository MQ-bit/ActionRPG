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
#include "Core/ARTeam.h"
#include "GameplayEffect.h"
#include "GAS/ARGameplayTags.h"
#include "GenericTeamAgentInterface.h"

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

	const int32 PlaybackId = ComboState.BeginStep(StepIndex, ComboMontages.Num());

	FOnMontageBlendingOutStarted BlendingOut;
	BlendingOut.BindUObject(this, &UARGA_BasicAttack::OnMontageBlendingOut, PlaybackId);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendingOut, Montage);

	FOnMontageEnded Ended;
	Ended.BindUObject(this, &UARGA_BasicAttack::OnMontageEnded, PlaybackId);
	AnimInstance->Montage_SetEndDelegate(Ended, Montage);
}

void UARGA_BasicAttack::ApplyComboAction(FARComboState::EAction Action)
{
	switch (Action)
	{
	case FARComboState::EAction::PlayNextStep:
		PlayStep(ComboState.GetNextStepIndex());
		break;
	case FARComboState::EAction::End:
		// The ability (and State.Attacking) lasts until the last played montage has fully ended.
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		break;
	case FARComboState::EAction::Cancel:
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		break;
	case FARComboState::EAction::None:
		break;
	}
}

void UARGA_BasicAttack::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	ApplyComboAction(ComboState.OnPressed());
}

void UARGA_BasicAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Montage events still queued from this activation must not touch a later one.
	ComboState.Invalidate();

	// A cancelled ability must not leave its montage playing.
	if (bWasCancelled && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		ActorInfo->AbilitySystemComponent->CurrentMontageStop();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UARGA_BasicAttack::OnMontageBlendingOut(UAnimMontage* /*Montage*/, bool bInterrupted, int32 PlaybackId)
{
	ApplyComboAction(ComboState.OnBlendingOut(PlaybackId, bInterrupted));
}

void UARGA_BasicAttack::OnMontageEnded(UAnimMontage* /*Montage*/, bool bInterrupted, int32 PlaybackId)
{
	ApplyComboAction(ComboState.OnEnded(PlaybackId, bInterrupted));
}

void UARGA_BasicAttack::OnHitEvent(FGameplayEventData Payload)
{
	// The hit frame opens the input window for the next step.
	ComboState.OnHit();

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

	const IGenericTeamAgentInterface* SourceTeam = Cast<IGenericTeamAgentInterface>(Avatar);

	TSet<AActor*> Damaged;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || Damaged.Contains(Target))
		{
			continue;
		}
		Damaged.Add(Target);

		// Only hostile teams are hit, so enemies do not hurt each other.
		const IGenericTeamAgentInterface* TargetTeam = Cast<IGenericTeamAgentInterface>(Target);
		if (SourceTeam && TargetTeam
			&& !ARTeam::IsHostile(SourceTeam->GetGenericTeamId().GetId(), TargetTeam->GetGenericTeamId().GetId()))
		{
			continue;
		}

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
