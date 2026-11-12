// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Parkour move rules as plain numbers (cm, degrees), so they can be unit tested without a world. Values are first guesses to tune in PIE. */
namespace ARParkour
{
	enum class EMove : uint8
	{
		None,
		Vault,
		Mantle,
	};

	/** What the forward trace found. Heights are measured from the character's feet. */
	struct FObstacle
	{
		/** Height of the top surface. */
		float Height = 0.f;
		/** Depth of the obstacle along the move direction. */
		float Thickness = 0.f;
		/** Free vertical space above the top surface. */
		float ClearanceAbove = 0.f;
		/** Slope of the top surface in degrees (0 = flat). */
		float SurfaceSlopeDegrees = 0.f;
		/** True when there is floor to land on behind the obstacle (needed to vault). */
		bool bLandingBehind = false;
	};

	constexpr float VaultMinHeight = 50.f;
	constexpr float VaultMaxHeight = 120.f;
	constexpr float VaultMaxThickness = 150.f;
	constexpr float MantleMaxHeight = 240.f;
	constexpr float MaxWalkableSlopeDegrees = 30.f;
	/** Capsule height of the player (2 x 96). The top surface needs this much free space. */
	constexpr float RequiredClearance = 192.f;

	/** Vault: low and thin with floor behind it. Mantle: taller, flat enough to stand on. Anything else: no move, a plain jump. */
	inline EMove Classify(const FObstacle& Obstacle)
	{
		if (Obstacle.ClearanceAbove < RequiredClearance || Obstacle.SurfaceSlopeDegrees > MaxWalkableSlopeDegrees)
		{
			return EMove::None;
		}

		if (Obstacle.Height >= VaultMinHeight && Obstacle.Height <= VaultMaxHeight)
		{
			return (Obstacle.Thickness <= VaultMaxThickness && Obstacle.bLandingBehind) ? EMove::Vault : EMove::None;
		}

		if (Obstacle.Height > VaultMaxHeight && Obstacle.Height <= MantleMaxHeight)
		{
			return EMove::Mantle;
		}

		return EMove::None;
	}
}
