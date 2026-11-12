// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ARParkour.h"
#include "GAS/ARGameplayAbility.h"
#include "ARGA_Parkour.generated.h"

/**
 * Vault over a low obstacle or mantle onto a ledge in front of the character. CanActivate runs the probe, so the
 * ability is only available when ARParkour::Classify finds a move; the jump input tries it first and jumps otherwise.
 * Holds State.Parkouring while moving (blocks and is blocked by State.Attacking). The motion is UARAbilityTask_ParkourMove
 * (lift, cross, settle, swept); a montage with motion warping can replace it later.
 */
UCLASS()
class ACTIONROGUELIKE_API UARGA_Parkour : public UARGameplayAbility
{
	GENERATED_BODY()

public:
	UARGA_Parkour();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** What the probe decided: the move, where the capsule ends up, and which way to face. */
	struct FProbeResult
	{
		ARParkour::EMove Move = ARParkour::EMove::None;
		FVector Target = FVector::ZeroVector;
		/** Height the capsule centre rises to before crossing; clears the obstacle top. */
		float ApexZ = 0.f;
		FRotator Facing = FRotator::ZeroRotator;
	};

	/** Traces in front of the avatar and classifies what it finds. False when there is no move. */
	bool Probe(const FGameplayAbilityActorInfo* ActorInfo, FProbeResult& Out) const;

	UFUNCTION()
	void OnMoveFinished();

	UFUNCTION()
	void OnMoveBlocked();

	UPROPERTY(EditDefaultsOnly, Category = "Parkour")
	float VaultDuration = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Parkour")
	float MantleDuration = 0.7f;

	/** How far in front of the capsule an obstacle is looked for (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Parkour")
	float DetectDistance = 120.f;
};
