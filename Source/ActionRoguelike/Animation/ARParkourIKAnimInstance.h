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
};
