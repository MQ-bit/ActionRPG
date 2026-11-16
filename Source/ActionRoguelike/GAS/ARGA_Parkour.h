// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ARParkour.h"
#include "GAS/ARGameplayAbility.h"
#include "GAS/ARParkourClip.h"
#include "TimerManager.h"
#include "ARGA_Parkour.generated.h"

class UMotionWarpingComponent;

/**
 * Vault over a low obstacle or mantle onto a ledge in front of the character. CanActivate runs the probe, so the
 * ability is only available when ARParkour::Classify finds a move; the jump input tries it first and jumps otherwise.
 * Holds State.Parkouring while moving (blocks and is blocked by State.Attacking).
 *
 * The motion is a montage with Motion Warping when Clips has an entry for the move, obstacle height and speed and the
 * avatar has a UMotionWarpingComponent (the warp target "ParkourTarget" is the capsule's feet position). Otherwise, or
 * when the montage cannot be played, it is UARAbilityTask_ParkourMove (lift, cross, settle, swept).
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

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Montages by move, obstacle height and speed. Empty: the interpolated move is used. Set in the data-only Blueprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parkour|Montage")
	TArray<FARParkourClip> Clips;

protected:
	/** What the probe decided: the move, where the capsule ends up, and which way to face. */
	struct FProbeResult
	{
		ARParkour::EMove Move = ARParkour::EMove::None;
		FVector Target = FVector::ZeroVector;
		/** Height the capsule centre rises to before crossing; clears the obstacle top. */
		float ApexZ = 0.f;
		FRotator Facing = FRotator::ZeroRotator;
		/** Height of the obstacle top above the feet, used to pick the clip. */
		float Height = 0.f;
		float HalfHeight = 0.f;
		TWeakObjectPtr<AActor> Obstacle;
	};

	/** Traces in front of the avatar and classifies what it finds. False when there is no move. */
	bool Probe(const FGameplayAbilityActorInfo* ActorInfo, FProbeResult& Out) const;

	/** Plays the matching montage with a warp target; false (and nothing changed) when there is none or it cannot play. */
	bool TryStartMontage(const FProbeResult& Result);

	void StartInterpolatedMove(const FProbeResult& Result);

	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 PlaybackId);

	/** Arrival check and clean-up, one tick after the montage ended. */
	void FinishMontageMove(int32 PlaybackId);

	/** Remembers the last capsule position that overlapped nothing, as the spot to return to when the montage is cut short. */
	void SampleSafeLocation();

	bool CapsuleBlockedAt(const FVector& Location) const;

	/** Undoes everything TryStartMontage set up. Safe to call twice. */
	void CleanUpMontageMove(bool bArrived);

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

	/** The montage counts as arrived when the capsule ends within this distance of the target (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Parkour|Montage")
	float ArrivalTolerance = 15.f;

private:
	bool bMontageMove = false;
	int32 MontagePlaybackId = 0;
	FVector MontageTarget = FVector::ZeroVector;
	FVector SafeLocation = FVector::ZeroVector;
	TWeakObjectPtr<AActor> IgnoredObstacle;
	TWeakObjectPtr<UMotionWarpingComponent> WarpComponent;
	FTimerHandle SafeLocationTimer;
};
