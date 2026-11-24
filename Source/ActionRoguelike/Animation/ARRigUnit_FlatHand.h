// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Units/RigUnit.h"
#include "ARRigUnit_FlatHand.generated.h"

/**
 * The rotation of a hand lying flat on the top of the parkour obstacle: the fingers point into the obstacle (horizontally), the palm faces the
 * surface. The hand's own axes are read from the rig's initial pose (the finger direction hand -> middle finger, the palm plane from the
 * index and the pinky knuckle), so any skeleton with those bones works. The rotation is blended from the animated one at Weight, and the
 * weight itself changes at a fixed speed so the hand turns over a few frames instead of snapping. Feed it the rotation the hand IK uses.
 */
USTRUCT(meta = (DisplayName = "AR Flat Hand Rotation", Category = "ActionRoguelike", Keywords = "parkour,hand,palm,ledge"))
struct ACTIONROGUELIKE_API FRigUnit_ARFlatHandRotation : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_ARFlatHandRotation() = default;

	RIGVM_METHOD()
	virtual void Execute() override;

	/** The rotation the hand has in the animation (rig space). */
	UPROPERTY(meta = (Input))
	FQuat Animated = FQuat::Identity;

	/** 0 = the animated rotation, 1 = flat on the surface. Normally the weight of the hand IK. */
	UPROPERTY(meta = (Input, ClampMin = "0.0", ClampMax = "1.0"))
	float Weight = 0.f;

	UPROPERTY(meta = (Input))
	FName HandBone = TEXT("hand_l");

	UPROPERTY(meta = (Input))
	FName MiddleBone = TEXT("middle_01_l");

	UPROPERTY(meta = (Input))
	FName IndexBone = TEXT("index_01_l");

	UPROPERTY(meta = (Input))
	FName PinkyBone = TEXT("pinky_01_l");

	/**
	 * Which way the palm faces when the fingers point forward and the hand lies flat: with the knuckle plane read as (finger x across) the left hand's
	 * normal points up and the right hand's down (mirrored axes). true = up (left), false = down (right).
	 */
	UPROPERTY(meta = (Input))
	bool bPalmNormalUp = true;

	/** How fast the weight follows (1/s): 5 = a fifth of a second from the animated to the flat hand. */
	UPROPERTY(meta = (Input))
	float WeightSpeed = 5.f;

	UPROPERTY(meta = (Output))
	FQuat Result = FQuat::Identity;

	/** The smoothed weight of the last frame. */
	UPROPERTY(transient)
	float SmoothedWeight = 0.f;
};
