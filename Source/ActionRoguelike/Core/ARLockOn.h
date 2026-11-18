// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Lock-on rules as plain numbers (cm, degrees, seconds), so they can be unit tested without a world. Values are first guesses to tune in PIE. */
namespace ARLockOn
{
	constexpr float MaxDistance = 1200.f;
	constexpr float MaxAngleDegrees = 60.f;
	/** A held target is only dropped once it is this many times farther than MaxDistance. */
	constexpr float ReleaseDistanceFactor = 1.2f;
	constexpr float OccludedGraceSeconds = 1.f;

	/** What the component found for one enemy; the world queries (team, ASC, line of sight) are done before this. */
	struct FCandidate
	{
		FVector Location = FVector::ZeroVector;
		bool bAlive = true;
		bool bVisible = true;
	};

	/**
	 * The living, visible candidate nearest to Origin inside MaxDistance and within MaxAngle of Forward (the angle is measured on the
	 * ground plane, so height differences do not count). A tie within 1 cm goes to the one more in front. INDEX_NONE if none qualifies.
	 */
	inline int32 PickTarget(const TArray<FCandidate>& Candidates, const FVector& Origin, const FVector& Forward, float InMaxDistance = MaxDistance, float InMaxAngleDegrees = MaxAngleDegrees)
	{
		const FVector Facing = Forward.GetSafeNormal2D();
		int32 Best = INDEX_NONE;
		float BestDistance = 0.f;
		float BestCos = 0.f;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const FCandidate& Candidate = Candidates[Index];
			if (!Candidate.bAlive || !Candidate.bVisible)
			{
				continue;
			}
			const FVector Offset = Candidate.Location - Origin;
			const float Distance = Offset.Size();
			if (Distance > InMaxDistance)
			{
				continue;
			}
			// A candidate on top of the origin has no direction: it is always in front.
			const FVector Direction = Offset.GetSafeNormal2D();
			const float Cos = Direction.IsNearlyZero() ? 1.f : static_cast<float>(FVector::DotProduct(Facing, Direction));
			if (Cos < FMath::Cos(FMath::DegreesToRadians(InMaxAngleDegrees)) - UE_KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const bool bCloser = Best == INDEX_NONE || Distance < BestDistance - 1.f;
			const bool bTieMoreFrontal = Best != INDEX_NONE && FMath::Abs(Distance - BestDistance) <= 1.f && Cos > BestCos;
			if (bCloser || bTieMoreFrontal)
			{
				Best = Index;
				BestDistance = Distance;
				BestCos = Cos;
			}
		}
		return Best;
	}

	/** Whether to let go of the held target: it is dead, too far, or has been out of sight for the whole grace time. */
	inline bool ShouldRelease(float Distance, float OccludedSeconds, bool bTargetAlive, float InMaxDistance = MaxDistance)
	{
		return !bTargetAlive || Distance > InMaxDistance * ReleaseDistanceFactor || OccludedSeconds >= OccludedGraceSeconds;
	}
}
