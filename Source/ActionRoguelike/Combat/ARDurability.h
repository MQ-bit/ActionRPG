// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Weapon durability. Pure logic (no UObject) so it is unit tested directly.
 * Max 0 means the weapon has no durability (it never breaks). A broken weapon stays usable; repairing it above 0 un-breaks it.
 */
struct FARDurability
{
	float Max = 0.f;
	float Current = 0.f;

	void Init(float InMax)
	{
		Max = FMath::Max(0.f, InMax);
		Current = Max;
	}

	bool HasDurability() const { return Max > 0.f; }
	bool IsBroken() const { return HasDurability() && Current <= 0.f; }
	float Fraction() const { return HasDurability() ? Current / Max : 1.f; }

	/** Returns true when this call broke the weapon. A broken weapon or a weapon without durability loses nothing. */
	bool Consume(float Amount)
	{
		if (!HasDurability() || IsBroken() || Amount <= 0.f)
		{
			return false;
		}
		Current = FMath::Max(0.f, Current - Amount);
		return IsBroken();
	}

	/** Returns true when this call took the weapon out of the broken state. */
	bool Repair(float Amount)
	{
		if (!HasDurability() || Amount <= 0.f)
		{
			return false;
		}
		const bool bWasBroken = IsBroken();
		Current = FMath::Min(Max, Current + Amount);
		return bWasBroken && !IsBroken();
	}
};
