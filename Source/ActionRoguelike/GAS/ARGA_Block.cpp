// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ARGA_Block.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/ARGameplayTags.h"

UARGA_Block::UARGA_Block()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	InputTag = TAG_InputTag_Block.GetTag();
	SetAssetTags(FGameplayTagContainer(TAG_Ability_Block.GetTag()));
	ActivationOwnedTags.AddTag(TAG_State_Blocking.GetTag());
	ActivationBlockedTags.AddTag(TAG_State_Parkouring.GetTag());
	ActivationBlockedTags.AddTag(TAG_State_HitReact.GetTag());
	// Blocking cuts a running attack (the "attack cancel" combat rule).
	CancelAbilitiesWithTag.AddTag(TAG_Ability_Attack.GetTag());
}

const UARGA_Block* UARGA_Block::FindActiveBlock(const UAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return nullptr;
	}
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const UARGA_Block* Block = Spec.IsActive() ? Cast<UARGA_Block>(Spec.GetPrimaryInstance()) : nullptr;
		if (Block && Block->IsActive())
		{
			return Block;
		}
	}
	return nullptr;
}

bool UARGA_Block::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (World && World->GetTimeSeconds() - LastBlockEndTime < ReblockDelay)
	{
		return false;
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UARGA_Block::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	BlockStartTime = Avatar && Avatar->GetWorld() ? Avatar->GetWorld()->GetTimeSeconds() : 0.0;

	if (const ACharacter* Character = Cast<ACharacter>(Avatar))
	{
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		SavedMaxWalkSpeed = Movement->MaxWalkSpeed;
		Movement->MaxWalkSpeed = FMath::Min(Movement->MaxWalkSpeed, BlockMoveSpeed);
	}
}

void UARGA_Block::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UARGA_Block::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsActive())
	{
		const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
		LastBlockEndTime = Avatar && Avatar->GetWorld() ? Avatar->GetWorld()->GetTimeSeconds() : LastBlockEndTime;
		if (const ACharacter* Character = Cast<ACharacter>(Avatar); Character && SavedMaxWalkSpeed >= 0.f)
		{
			Character->GetCharacterMovement()->MaxWalkSpeed = SavedMaxWalkSpeed;
		}
		SavedMaxWalkSpeed = -1.f;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
