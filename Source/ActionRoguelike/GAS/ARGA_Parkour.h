// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ARParkour.h"
#include "GAS/ARGameplayAbility.h"
#include "GAS/ARParkourClip.h"
#include "TimerManager.h"
#include "ARGA_Parkour.generated.h"

class UARParkourIKComponent;
class UMotionWarpingComponent;
class UPrimitiveComponent;
class USpringArmComponent;

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

public:
	/**
	 * For the jump input buffer: a running character pressed jump with a wall ahead that is still too far for the clip. The caller
	 * keeps the input and tries to activate every frame until the character has run up to the clip's distance.
	 */
	bool ShouldBufferJump() const;

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
		/** Distance from the capsule centre to the wall surface, along the move direction. */
		float WallDistance = 0.f;
		TWeakObjectPtr<AActor> Obstacle;
		/** The obstacle's front top edge at the point the probe hit, the direction into it, and the component that has the top. */
		FVector FrontEdgePoint = FVector::ZeroVector;
		FVector Inward = FVector::ForwardVector;
		TWeakObjectPtr<UPrimitiveComponent> ObstacleComponent;
	};

	/** Traces in front of the avatar and classifies what it finds. False when there is no move. */
	bool Probe(const FGameplayAbilityActorInfo* ActorInfo, FProbeResult& Out) const;

	/** The clip the move would play, or INDEX_NONE. */
	int32 PickClipIndex(const FProbeResult& Result, const ACharacter* Character) const;

	/** The wall is more than the approach warp can absorb beyond the clip's reference distance. */
	bool IsTooFar(const FProbeResult& Result, const FGameplayAbilityActorInfo* ActorInfo) const;

	/**
	 * Pose matching, small: the start time of the clip's approach (before ApproachEnd) whose remaining root travel and root speed fit the wall
	 * distance and the speed now best (ARParkour::ChooseStart), with the reference distance for that start. False when the clip has no approach.
	 */
	bool MatchStart(const FARParkourClip& Clip, UAnimMontage* Montage, const FProbeResult& Result, const ACharacter* Character,
		float& OutStart, float& OutReference, float& OutCost, float& OutFootError) const;

	/** The clip of the same move and obstacle height (any speed variant) with a matched start whose match is cheapest; Index when none is better. */
	int32 BestMatchedClip(const FProbeResult& Result, const ACharacter* Character, int32 Index) const;

	/** Plays the matching montage with a warp target; false (and nothing changed) when there is none or it cannot play. */
	bool TryStartMontage(const FProbeResult& Result);

	/** Plays clip Index with its warp targets set for this wall. False (and nothing changed) when the montage cannot play. */
	bool PlayClip(const FProbeResult& Result, int32 Index);


	void StartInterpolatedMove(const FProbeResult& Result);

	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 PlaybackId);

	/**
	 * The montage has played to its last frame and starts to blend out (its blend-out trigger time is 0): the warp has put the capsule on
	 * the target. While the character is still Flying the anim blueprint shows the in-air pose under the fading montage, and the feet
	 * drop to the ground in one frame when the movement mode finally changes: switch to Walking now, finish the move at the montage end.
	 */
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, int32 PlaybackId);

	/** Arrival check and clean-up, one tick after the montage ended. */
	void FinishMontageMove(int32 PlaybackId);

	/** Remembers the last capsule position that overlapped nothing, as the spot to return to when the montage is cut short. */
	void SampleSafeLocation();

	/** Inset shrinks the capsule (cm), so touching a surface does not count as overlapping it. */
	bool CapsuleBlockedAt(const FVector& Location, float Inset = 0.f) const;

	/** Undoes everything TryStartMontage set up. Safe to call twice. */
	void CleanUpMontageMove(bool bArrived);

	/** Mantle: puts a validated target on the ledge edge for each hand that has a window in the clip (hand IK). */
	void BeginHandIK(const FProbeResult& Result, const FARParkourClip& Clip, UAnimMontage* Montage, ACharacter* Character);
	void EndHandIK();

	/** Camera boom without collision and with lag for the length of the move; EndCameraOverride restores the old values. */
	void BeginCameraOverride(AActor* Avatar);
	void EndCameraOverride();
	void TryRestoreCameraCollision();

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
	float DetectDistance = 300.f;

	/** The montage counts as arrived when the capsule ends within this distance of the target (cm); it is then snapped onto it. */
	UPROPERTY(EditDefaultsOnly, Category = "Parkour|Montage")
	float ArrivalTolerance = 25.f;

	/** The arrival overlap test uses a capsule this much smaller (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Parkour|Montage")
	float ArrivalInset = 4.f;

	/** A hand target lies on the ledge edge, this far onto the top and to the side of the body centre line (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Parkour|Hand IK")
	float HandInwardCm = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Parkour|Hand IK")
	float HandLateralCm = 20.f;

	/** The hand bone sits this far above the top surface (cm): about the thickness of the hand, so the palm rests on it instead of sinking in. */
	UPROPERTY(EditDefaultsOnly, Category = "Parkour|Hand IK")
	float HandHeightCm = 4.f;

	/** While parkouring, the camera boom does not collide (it would be pulled into the character by walls) and follows with this lag. */
	UPROPERTY(EditDefaultsOnly, Category = "Parkour|Camera", meta = (ClampMin = "0"))
	float CameraLagSpeed = 12.f;

private:
	bool bMontageMove = false;

	/** Flying braking of the character before the montage move; flying has none, so the last root-motion velocity would slide the capsule on. */
	float SavedBrakingDecelerationFlying = 0.f;
	int32 MontagePlaybackId = 0;
	FVector MontageTarget = FVector::ZeroVector;
	FVector SafeLocation = FVector::ZeroVector;
	TWeakObjectPtr<AActor> IgnoredObstacle;
	TWeakObjectPtr<UMotionWarpingComponent> WarpComponent;
	TWeakObjectPtr<UARParkourIKComponent> HandIKComponent;
	FTimerHandle SafeLocationTimer;

	/** The collision test is switched back on after this long at the latest, if the camera's spot never clears (s). */
	static constexpr float MaxCameraRestoreSeconds = 10.f;

	FTimerHandle CameraRestoreTimer;
	float CameraRestoreElapsed = 0.f;
	TWeakObjectPtr<USpringArmComponent> OverriddenBoom;
	bool bSavedCollisionTest = true;
	bool bSavedCameraLag = false;
	float SavedLagSpeed = 10.f;
};
