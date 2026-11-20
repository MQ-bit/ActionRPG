// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "ARParkourClip.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class EARParkourMoveKind : uint8
{
	Vault,
	Mantle,
};

/** Mirrors ARParkour::EVariant (Stand/Walk/Run by speed). */
UENUM(BlueprintType)
enum class EARParkourVariant : uint8
{
	Stand,
	Walk,
	Run,
};

/**
 * One montage the parkour ability may play: for this move, obstacle heights (cm) and speed. The montage needs root motion and
 * a Motion Warping notify state with the warp target "ParkourTarget". Set in the ability's data-only Blueprint; an empty table
 * (or a missing asset) makes the ability use the plain interpolated move.
 */
USTRUCT(BlueprintType)
struct FARParkourClip
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	EARParkourMoveKind Move = EARParkourMoveKind::Vault;

	/** Obstacle heights this clip covers, inclusive (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	float MinHeight = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	float MaxHeight = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	EARParkourVariant Variant = EARParkourVariant::Stand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	TSoftObjectPtr<UAnimMontage> Montage;

	/**
	 * How far the capsule centre is from the wall when the clip, played from StartPosition, was made to start (cm). The climb part
	 * of the clip fits the obstacle for this distance only; the approach window of the montage makes up the difference.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour", meta = (ClampMin = "0"))
	float ReferenceDistanceCm = 100.f;

	/**
	 * Montage time (s) where the approach ends, i.e. where the clip has the body at the obstacle. The montage has a Motion Warping
	 * notify state named ParkourApproach up to here and one named ParkourTarget after it. 0 = no approach window.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour", meta = (ClampMin = "0"))
	float ApproachEnd = 0.f;

	/**
	 * Montage time (s) where the hands leave the obstacle, i.e. the end of the part of the clip in which the body is held at the
	 * height of the obstacle (the hands planted on it). Between ApproachEnd and here the montage has a Motion Warping notify state
	 * named ParkourBackLedge that moves the body by the difference between the obstacle's height and AuthoredHeightCm, so the hands
	 * land on the top whatever the height. 0 = no back-ledge window.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour", meta = (ClampMin = "0"))
	float BackLedgeEnd = 0.f;

	/** The obstacle height (cm) the clip was made for: where its hands plant (hand bone height minus about 4 cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour", meta = (ClampMin = "0"))
	float AuthoredHeightCm = 0.f;

	/**
	 * Added to the height of the warp target (cm), for a clip whose last frame ends this much above (use a negative value) or
	 * below the target: the arrival snap then has nothing left to correct, so the capsule does not pop on the last frame.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	float WarpOffsetZCm = 0.f;

	/**
	 * Moves the landing spot along the move direction (cm), e.g. for a clip whose trailing leg is still over the obstacle when it lands
	 * on a thin one. Used for the warp target and for the arrival check alike, and only when the moved spot is free.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	float WarpOffsetForwardCm = 0.f;

	/** Where in the montage to start (seconds), to skip a long run-in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour", meta = (ClampMin = "0"))
	float StartPosition = 0.f;

	/**
	 * Mantle only: when each hand is pulled onto the ledge edge, in montage time (s). Full pull from Start to End, easing in
	 * and out over HandRamp. End <= Start leaves that hand to the animation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour|Hand IK", meta = (ClampMin = "0"))
	float LeftHandStart = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour|Hand IK", meta = (ClampMin = "0"))
	float LeftHandEnd = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour|Hand IK", meta = (ClampMin = "0"))
	float RightHandStart = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour|Hand IK", meta = (ClampMin = "0"))
	float RightHandEnd = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour|Hand IK", meta = (ClampMin = "0"))
	float HandRamp = 0.15f;
};
