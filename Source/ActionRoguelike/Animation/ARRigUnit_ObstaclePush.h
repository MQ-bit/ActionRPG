// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Units/RigUnit.h"
#include "ARRigUnit_ObstaclePush.generated.h"

/** One part of the body that must stay out of the obstacle: the segment from Parent to Child, thickened by Radius. Parent is the bone that swings. */
USTRUCT()
struct FARObstaclePushSegment
{
	GENERATED_BODY()

	FARObstaclePushSegment() = default;
	FARObstaclePushSegment(FName InParent, FName InChild, float InRadius, bool bInMovesBody = false) : Parent(InParent), Child(InChild), Radius(InRadius), bMovesBody(bInMovesBody) {}

	UPROPERTY(EditAnywhere, Category = "Segment", meta = (Input))
	FName Parent = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Segment", meta = (Input))
	FName Child = NAME_None;

	/** Half the thickness of the body part around the line from Parent to Child (cm). */
	UPROPERTY(EditAnywhere, Category = "Segment", meta = (Input))
	float Radius = 5.f;

	/** The parent bone of this segment is part of the trunk or hips: when it is inside the obstacle the whole body (BodyBone) moves out, swinging the parent cannot help there. */
	UPROPERTY(EditAnywhere, Category = "Segment", meta = (Input))
	bool bMovesBody = false;
};

/**
 * Keeps limbs and the trunk out of the parkour obstacle's box: every segment (parent bone to child bone, with a radius) that is inside the box has its
 * parent bone swung so the child leaves it. It judges the pose this unit receives (the animation, before any IK of this unit's own), and smooths the
 * push at a fixed speed, so the result does not feed back into the next frame and does not pop. Place it before the hand and foot IK: they
 * pull the hands and feet back to their targets afterwards.
 */
USTRUCT(meta = (DisplayName = "AR Obstacle Push", Category = "ActionRoguelike", Keywords = "parkour,obstacle,penetration,collision"))
struct ACTIONROGUELIKE_API FRigUnit_ARObstaclePush : public FRigUnitMutable
{
	GENERATED_BODY()

	FRigUnit_ARObstaclePush();

	RIGVM_METHOD()
	virtual void Execute() override;

	/** 0 switches the push off (the pushed bones go back to the animation at PushSpeed), 1 is full. The obstacle itself comes from the character's UARParkourIKComponent (its ledge's bounds, world-space box) while a parkour move runs. */
	UPROPERTY(meta = (Input, ClampMin = "0.0", ClampMax = "1.0"))
	float Weight = 1.f;

	/** The bone that carries the whole body (the pelvis): it is moved first, by the deepest trunk or hip penetration, then the limbs are swung. */
	UPROPERTY(meta = (Input))
	FName BodyBone = TEXT("pelvis");

	/** The obstacle is kept for this long after the parkour move lets go of it (s): the last frames of a clip are still next to it. */
	UPROPERTY(meta = (Input))
	float HoldSeconds = 0.4f;

	/** The largest push of the whole body (cm); 0 leaves the body alone (a second node after the IK only swings limbs). */
	UPROPERTY(meta = (Input))
	float MaxBodyPushCm = 10.f;

	/** The largest push of one segment (cm). */
	UPROPERTY(meta = (Input))
	float MaxPushCm = 12.f;

	/** How fast the push changes (cm/s). */
	UPROPERTY(meta = (Input))
	float PushSpeed = 150.f;

	UPROPERTY(meta = (Input, ExpandByDefault))
	TArray<FARObstaclePushSegment> Segments;

	/** The push applied to each segment last frame (world space). */
	UPROPERTY(transient)
	TArray<FVector> Applied;

	/** The last obstacle box seen and how long it is still held. */
	UPROPERTY(transient)
	FVector LastBoxMin = FVector::ZeroVector;

	UPROPERTY(transient)
	FVector LastBoxMax = FVector::ZeroVector;

	UPROPERTY(transient)
	float HoldRemaining = 0.f;

	/** The push applied to the whole body last frame (world space). */
	UPROPERTY(transient)
	FVector BodyApplied = FVector::ZeroVector;
};
