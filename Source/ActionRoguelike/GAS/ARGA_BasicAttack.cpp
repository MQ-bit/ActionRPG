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
#include "Character/AREquipmentComponent.h"
#include "Combat/ARWeaponData.h"
#include "Camera/CameraShakeBase.h"
#include "Character/ARCharacterBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Core/ARCombat.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Core/ARTeam.h"
#include "GAS/ARGA_Block.h"
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
	// No attacking in the middle of a vault or mantle, a hit reaction or a block.
	ActivationBlockedTags.AddTag(TAG_State_Parkouring.GetTag());
	ActivationBlockedTags.AddTag(TAG_State_HitReact.GetTag());
	ActivationBlockedTags.AddTag(TAG_State_Blocking.GetTag());
}

void UARGA_BasicAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// The player's equipped weapon (enemies have no equipment component and keep the values of their Blueprint).
	FARAttackValues Unarmed;
	Unarmed.Montages = ComboMontages;
	Unarmed.Damage = Damage;
	Unarmed.HitRange = HitRange;
	Unarmed.HitRadius = HitRadius;
	Unarmed.PlayRate = MontagePlayRate;
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAREquipmentComponent* Equipment = Avatar ? Avatar->FindComponentByClass<UAREquipmentComponent>() : nullptr;
	ActiveEquipment = Equipment;
	ActiveWeapon = Equipment ? Equipment->GetEquippedWeapon() : nullptr;
	const FARAttackValues Values = ARWeapon::ResolveAttackValues(Unarmed, ActiveWeapon, Equipment && Equipment->IsBroken());
	ActiveMontages = Values.Montages;
	ActiveDamage = Values.Damage;
	ActiveHitRange = Values.HitRange;
	ActiveHitRadius = Values.HitRadius;
	ActivePlayRate = FMath::Max(0.1f, Values.PlayRate);

	if (ActiveMontages.IsEmpty() || !ActiveMontages[0] || !CommitAbility(Handle, ActorInfo, ActivationInfo))
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
	UAnimMontage* Montage = ActiveMontages.IsValidIndex(StepIndex) ? ActiveMontages[StepIndex].Get() : nullptr;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	if (!Montage || !ASC || !AnimInstance || ASC->PlayMontage(this, CurrentActivationInfo, Montage, ActivePlayRate) <= 0.f)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const int32 PlaybackId = ComboState.BeginStep(StepIndex, ActiveMontages.Num());
	bDurabilityChargedThisStep = false;

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

void UARGA_BasicAttack::PlayImpactFeel(AActor* Attacker, AActor* Target) const
{
	UWorld* World = Attacker ? Attacker->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	// Hit stop: both bodies freeze for a moment (the character keeps one freeze, so overlapping hits extend it).
	if (HitStopSeconds > 0.f)
	{
		for (AActor* Actor : { Attacker, Target })
		{
			if (AARCharacterBase* Character = Cast<AARCharacterBase>(Actor))
			{
				Character->ApplyHitStop(HitStopSeconds, HitStopTimeDilation);
			}
		}
	}

	if (HitCameraShake)
	{
		for (const AActor* Actor : { Attacker, Target })
		{
			const APawn* Pawn = Cast<APawn>(Actor);
			const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
			if (PC && PC->IsLocalController() && PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->StartCameraShake(HitCameraShake);
				break;
			}
		}
	}
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

	const FVector Center = Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * ActiveHitRange;

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ARBasicAttackHit), false, Avatar);
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(ActiveHitRadius), QueryParams);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugHit)
	{
		DrawDebugSphere(World, Center, ActiveHitRadius, 16, FColor::Red, false, 1.f);
	}
#endif

	const IGenericTeamAgentInterface* SourceTeam = Cast<IGenericTeamAgentInterface>(Avatar);

	// A weapon that broke earlier in this combo hits with its broken damage from then on (montages and range stay).
	UAREquipmentComponent* Equipment = ActiveEquipment.Get();
	const float HitDamage = ActiveWeapon && Equipment && Equipment->GetEquippedWeapon() == ActiveWeapon && Equipment->IsBroken()
		? ActiveWeapon->Damage * ActiveWeapon->BrokenDamageScale
		: ActiveDamage;

	bool bDamagedAnEnemy = false;
	TWeakObjectPtr<AActor> ParriedBy;
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

		// A blocking target in front takes part of the damage; a parry takes none and staggers this attacker.
		ARCombat::EDefense Defense = ARCombat::EDefense::None;
		float BlockScale = 1.f;
		if (const UARGA_Block* Block = UARGA_Block::FindActiveBlock(TargetASC))
		{
			Defense = ARCombat::ResolveDefense(true, static_cast<float>(World->GetTimeSeconds() - Block->GetBlockStartTime()), Block->GetParryWindow(),
				Target->GetActorForwardVector(), Avatar->GetActorLocation() - Target->GetActorLocation(), Block->GetFrontHalfAngle());
			BlockScale = Block->GetBlockDamageScale();
		}
		if (Defense == ARCombat::EDefense::Parried)
		{
			ParriedBy = Target;
			continue;
		}

		const float Amount = Defense == ARCombat::EDefense::Blocked ? HitDamage * BlockScale : HitDamage;
		FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(TAG_Data_Damage.GetTag(), -Amount);
			const FActiveGameplayEffectHandle Applied = SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			const bool bApplied = Amount > 0.f && Applied.WasSuccessfullyApplied();
			bDamagedAnEnemy |= bApplied;
			if (bApplied)
			{
				PlayImpactFeel(Avatar, Target);
			}
			// A blocked hit makes no hit reaction; a real hit does.
			if (bApplied && Defense == ARCombat::EDefense::None)
			{
				FGameplayEventData Event;
				Event.EventTag = TAG_Event_Damaged.GetTag();
				Event.Instigator = Avatar;
				Event.Target = Target;
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Target, TAG_Event_Damaged.GetTag(), Event);
			}
		}
	}

	// A swing that damaged at least one enemy wears the weapon once, however many it hit.
	if (bDamagedAnEnemy && !bDurabilityChargedThisStep)
	{
		bDurabilityChargedThisStep = true;
		if (Equipment)
		{
			Equipment->OnWeaponHit();
		}
	}

	// Last: the parried attacker's hit reaction cancels this ability, so nothing of it may run after this.
	if (AActor* Parrier = ParriedBy.Get())
	{
		FGameplayEventData Event;
		Event.EventTag = TAG_Event_Parried.GetTag();
		Event.Instigator = Parrier;
		Event.Target = Avatar;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Avatar, TAG_Event_Parried.GetTag(), Event);
	}
}
