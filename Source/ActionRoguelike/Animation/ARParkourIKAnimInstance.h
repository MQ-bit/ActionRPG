// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ARParkourIKAnimInstance.generated.h"

/**
 * Parent of the post-process anim blueprint that holds the hand IK Control Rig (ABP_ParkourIK). It runs after the main
 * anim instance, so the montage position it reads from the main instance is already this frame's. It only copies values:
 * the hand targets are world-space (the rig converts them to its own space while it evaluates, after root motion has moved
 * the character) and the weights follow the montage windows set by the parkour ability.
 */
UCLASS()
class ACTIONROGUELIKE_API UARParkourIKAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/**
	 * Heights (cm) of the ankle joint (foot bone) and the ball joint above the sole when the foot stands flat on the ground. They turn the bone
	 * heights into a sole height for the foot lift and plant. Measured per character: the mannequin 8.5 / 0.5, Paragon Wraith -0.4 / 3.8.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Foot")
	float AnkleHeightCm = 8.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Foot")
	float BallHeightCm = 0.5f;

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** World-space hand targets (location only; the rig keeps the animated hand rotation). */
	UPROPERTY(BlueprintReadOnly, Category = "Parkour IK")
	FTransform HandTargetL;

	UPROPERTY(BlueprintReadOnly, Category = "Parkour IK")
	FTransform HandTargetR;

	/** 0 = animation only, 1 = hand on the target. */
	UPROPERTY(BlueprintReadOnly, Category = "Parkour IK")
	float WeightL = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Parkour IK")
	float WeightR = 0.f;

	/**
	 * Foot lift for the mantle ledge: the translation is an offset (0, 0, cm) the rig adds to the animated foot, just enough to
	 * keep the sole on top of the ledge. Weight is 0 when no lift is needed, so the animation keeps the leg.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Parkour IK")
	FTransform FootTargetL;

	UPROPERTY(BlueprintReadOnly, Category = "Parkour IK")
	FTransform FootTargetR;

	UPROPERTY(BlueprintReadOnly, Category = "Parkour IK")
	float FootWeightL = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Parkour IK")
	float FootWeightR = 0.f;

private:
	/** Offset (cm) applied to each hand last frame to keep it out of the obstacle; see UpdateHandPush. */
	FVector HandPush[2] = {FVector::ZeroVector, FVector::ZeroVector};

	/**
	 * A hand the animation puts inside the obstacle (an arm reaching up a wall that is too close) is pushed out through the nearest
	 * face. Only used where the ledge window does not already hold the hand (the window weight wins).
	 */
	void UpdateHandPush(int32 Side, const class UARParkourIKComponent* IK, float DeltaSeconds);

	/** Lift (cm) applied to each foot last frame; the new lift is worked out from the pose it produced. */
	float FootLiftCm[2] = {0.f, 0.f};

	/** Horizontal offset (cm, world) applied to each foot last frame to put it against the wall face; see UpdateFoot. */
	FVector FootWallCm[2] = {FVector::ZeroVector, FVector::ZeroVector};

	void UpdateFoot(int32 Side, const class UARParkourIKComponent* IK, float DeltaSeconds);
};
