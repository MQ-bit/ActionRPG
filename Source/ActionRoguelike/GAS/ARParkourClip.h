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

	/** Where in the montage to start (seconds), to skip a long run-in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour", meta = (ClampMin = "0"))
	float StartPosition = 0.f;
};
