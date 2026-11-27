// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Ballistic path rules for thrown items (cm, cm/s), so they can be unit tested without a world. GravityZ is negative, e.g. -980. */
namespace ARThrow
{
	/** Closer than this horizontally counts as straight up; the angle formula divides by the horizontal distance. */
	constexpr double StraightUpDistance = 1.0;

	inline FVector PointAt(const FVector& Start, const FVector& Velocity, double GravityZ, double Time)
	{
		return Start + Velocity * Time + FVector(0.0, 0.0, 0.5 * GravityZ * Time * Time);
	}

	/** Lower-arc launch velocity that passes through Target at the given speed; false (OutVelocity zero) when out of range. */
	inline bool SolveLaunch(const FVector& Start, const FVector& Target, double Speed, double GravityZ, FVector& OutVelocity)
	{
		OutVelocity = FVector::ZeroVector;
		if (Speed <= 0.0 || GravityZ >= 0.0)
		{
			return false;
		}

		const double G = -GravityZ;
		const FVector Delta = Target - Start;
		const double Dist = FVector(Delta.X, Delta.Y, 0.0).Size();
		const double SpeedSq = Speed * Speed;

		if (Dist < StraightUpDistance)
		{
			// Going straight up reaches the apex at Speed^2 / 2G; anything higher is out of reach.
			if (Delta.Z > SpeedSq / (2.0 * G))
			{
				return false;
			}
			OutVelocity = FVector(0.0, 0.0, Speed);
			return true;
		}

		const double Discriminant = SpeedSq * SpeedSq - G * (G * Dist * Dist + 2.0 * Delta.Z * SpeedSq);
		if (Discriminant < 0.0)
		{
			return false;
		}

		const double Tan = (SpeedSq - FMath::Sqrt(Discriminant)) / (G * Dist);
		const double Cos = 1.0 / FMath::Sqrt(1.0 + Tan * Tan);
		const FVector Dir(Delta.X / Dist, Delta.Y / Dist, 0.0);
		OutVelocity = (Dir * Cos + FVector(0.0, 0.0, Tan * Cos)) * Speed;
		return true;
	}

	/** Up to Count points every Step seconds, starting at Start; the last one is cut to GroundZ where the path crosses it (straight chord, so slightly short of the true landing). */
	inline void SamplePath(const FVector& Start, const FVector& Velocity, double GravityZ, int32 Count, double Step, double GroundZ, TArray<FVector>& OutPoints)
	{
		OutPoints.Reset();
		if (Count < 1)
		{
			return;
		}

		OutPoints.Add(Start);
		for (int32 Index = 1; Index < Count; ++Index)
		{
			const FVector Point = PointAt(Start, Velocity, GravityZ, Index * Step);
			if (Point.Z < GroundZ)
			{
				const FVector Prev = OutPoints.Last();
				if (Prev.Z >= GroundZ)
				{
					OutPoints.Add(FMath::Lerp(Prev, Point, (Prev.Z - GroundZ) / (Prev.Z - Point.Z)));
				}
				return;
			}
			OutPoints.Add(Point);
		}
	}
}
