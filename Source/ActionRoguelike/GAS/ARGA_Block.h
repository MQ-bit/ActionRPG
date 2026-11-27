// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/ARGameplayAbility.h"
#include "ARGA_Block.generated.h"

class UAbilitySystemComponent;

/**
 * Held block (player). Active while the input is held: hits from the front deal BlockDamageScale of their damage, and a hit
 * in the first ParryWindow seconds of the block is a parry (no damage, the attacker staggers). Pressing block cancels a
 * running attack. The attack's hit code asks FindActiveBlock() on the target; the rules are ARCombat::ResolveDefense.
 */
UCLASS()
class ACTIONROGUELIKE_API UARGA_Block : public UARGameplayAbility
{
	GENERATED_BODY()

public:
	UARGA_Block();

	/** The running block of this ASC, or null. */
	static const UARGA_Block* FindActiveBlock(const UAbilitySystemComponent* ASC);

	double GetBlockStartTime() const { return BlockStartTime; }
	float GetParryWindow() const { return ParryWindow; }
	float GetBlockDamageScale() const { return BlockDamageScale; }
	float GetFrontHalfAngle() const { return FrontHalfAngle; }

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Seconds at the start of the block in which a hit is parried. */
	UPROPERTY(EditDefaultsOnly, Category = "Block", meta = (ClampMin = "0"))
	float ParryWindow = 0.2f;

	/** Damage multiplier for a blocked hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Block", meta = (ClampMin = "0", ClampMax = "1"))
	float BlockDamageScale = 0.2f;

	/** Hits within this angle of the facing direction are blocked (degrees). */
	UPROPERTY(EditDefaultsOnly, Category = "Block", meta = (ClampMin = "0", ClampMax = "180"))
	float FrontHalfAngle = 70.f;

	/** Seconds after a block ended before a new one (and its parry window) can start; stops parry spamming. */
	UPROPERTY(EditDefaultsOnly, Category = "Block", meta = (ClampMin = "0"))
	float ReblockDelay = 0.3f;

private:
	double BlockStartTime = 0.0;
	double LastBlockEndTime = -1000.0;
};
