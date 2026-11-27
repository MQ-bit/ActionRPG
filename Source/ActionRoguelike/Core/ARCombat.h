// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Pure combat rules (no UObject), unit tested in Tests/ARCombatTest.cpp. */
namespace ARCombat
{
	enum class EHitDirection : uint8
	{
		Front,
		Back,
	};

	/** Which side of the target the attacker is on (horizontal plane). */
	inline EHitDirection HitDirection(const FVector& TargetForward, const FVector& ToInstigator)
	{
		return FVector::DotProduct(TargetForward.GetSafeNormal2D(), ToInstigator.GetSafeNormal2D()) >= 0.f ? EHitDirection::Front : EHitDirection::Back;
	}

	enum class EDefense : uint8
	{
		None,
		Blocked,
		Parried,
	};

	/**
	 * Result of a hit on a target. A block only works against attackers in front (within FrontHalfAngleDeg of the target's
	 * forward); a hit inside the first ParryWindow seconds of the block is a parry.
	 */
	inline EDefense ResolveDefense(bool bBlocking, float TimeSinceBlockStart, float ParryWindow, const FVector& TargetForward,
		const FVector& ToAttacker, float FrontHalfAngleDeg)
	{
		const FVector Forward = TargetForward.GetSafeNormal2D();
		const FVector ToOther = ToAttacker.GetSafeNormal2D();
		if (!bBlocking || Forward.IsNearlyZero() || ToOther.IsNearlyZero()
			|| FVector::DotProduct(Forward, ToOther) < FMath::Cos(FMath::DegreesToRadians(FrontHalfAngleDeg)))
		{
			return EDefense::None;
		}
		return TimeSinceBlockStart <= ParryWindow ? EDefense::Parried : EDefense::Blocked;
	}
}
