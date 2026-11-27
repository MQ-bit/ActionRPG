// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Noise rules for sound-reacting enemies as plain numbers (cm, seconds), so they can be unit tested without a world. */
namespace ARNoise
{
	constexpr uint8 Footstep = 0;
	constexpr uint8 Gunshot = 1;
	constexpr uint8 Impact = 2;

	struct FARNoiseEvent
	{
		FVector Location = FVector::ZeroVector;
		/** Hearing radius in cm: the event is audible to anyone within it. */
		float Loudness = 0.f;
		double Time = 0.0;
		uint8 Kind = Footstep;
	};

	inline bool CanHear(const FVector& Listener, const FARNoiseEvent& Event, double Now, double MaxAgeSeconds)
	{
		return Event.Loudness > 0.f
			&& FVector::Dist(Listener, Event.Location) <= Event.Loudness
			&& Now - Event.Time <= MaxAgeSeconds;
	}

	/** Index of the audible event with the highest Loudness - distance (newest wins a tie, then the lower index); INDEX_NONE if nothing is audible. */
	inline int32 PickNoiseToInvestigate(const FVector& Listener, const TArray<FARNoiseEvent>& Events, double Now, double MaxAgeSeconds)
	{
		int32 Best = INDEX_NONE;
		double BestScore = 0.0;
		for (int32 Index = 0; Index < Events.Num(); ++Index)
		{
			const FARNoiseEvent& Event = Events[Index];
			if (!CanHear(Listener, Event, Now, MaxAgeSeconds))
			{
				continue;
			}

			const double Score = Event.Loudness - FVector::Dist(Listener, Event.Location);
			if (Best == INDEX_NONE || Score > BestScore || (Score == BestScore && Event.Time > Events[Best].Time))
			{
				Best = Index;
				BestScore = Score;
			}
		}
		return Best;
	}
}
