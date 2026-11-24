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
	 * GASP-style ledge registration. The approach and back-ledge windows put the clip's ledge point (AuthoredHeightCm high, ReferenceDistanceCm
	 * from where the clip starts) on the real ledge; with this set the rest of the clip is NOT warped to the landing, it plays on as made, so its
	 * poses stay in step with the ledge (hands, feet, knees at the same place against the edge at every height). The move then ends where the
	 * clip ends. Used only when that spot is free and on the top; else the landing warp (ParkourTarget window) is used as before.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	bool bRigidAfterLedge = false;

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

	/** Playback speed of the montage (1 = as made). The warp windows are in montage time and scale with it; the root travel does not change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour", meta = (ClampMin = "0.1", ClampMax = "3"))
	float PlayRate = 1.f;

	/**
	 * Montage time (s) from which a movement input ends the move early: the montage blends out and the character walks on. Only once the
	 * capsule is on the target, so nothing pops. 0 = the clip always plays to its end.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour", meta = (ClampMin = "0"))
	float MoveCancelFrom = 0.f;

	/**
	 * Picks the start time when the move starts instead of using StartPosition: the moment of the approach (from the montage start to
	 * ApproachEnd) whose remaining root travel fits the distance to the wall and whose root speed fits the character's speed now. Needs
	 * ApproachEnd. ReferenceDistanceCm stays the distance for StartPosition; the one for the picked time is worked out from it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	bool bMatchStart = false;

	/**
	 * For the start match: where the montage has the feet every 1/30 s from time 0 to ApproachEnd, relative to the capsule in the move
	 * direction: X left foot forward, Y left foot height above the floor, Z right foot forward, W right foot height (cm). Measured from a
	 * playback by the tools (tmp clip_probe). Empty: the feet are not compared.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	TArray<FVector4> FootPose;

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
