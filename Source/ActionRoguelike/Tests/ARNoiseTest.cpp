// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/ARNoise.h"

#define AR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

namespace
{
	ARNoise::FARNoiseEvent Noise(const FVector& Location, float Loudness, double Time)
	{
		ARNoise::FARNoiseEvent Event;
		Event.Location = Location;
		Event.Loudness = Loudness;
		Event.Time = Time;
		Event.Kind = ARNoise::Gunshot;
		return Event;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARNoiseCanHearTest, "ActionRoguelike.Noise.CanHear", AR_TEST_FLAGS)
bool FARNoiseCanHearTest::RunTest(const FString& Parameters)
{
	const FVector Listener = FVector::ZeroVector;
	const double Now = 10.0;
	const double MaxAge = 5.0;

	TestTrue(TEXT("inside the radius and fresh"), ARNoise::CanHear(Listener, Noise(FVector(300.0, 0.0, 0.0), 500.f, 9.0), Now, MaxAge));
	TestTrue(TEXT("exactly on the radius"), ARNoise::CanHear(Listener, Noise(FVector(500.0, 0.0, 0.0), 500.f, 9.0), Now, MaxAge));
	TestFalse(TEXT("just outside the radius"), ARNoise::CanHear(Listener, Noise(FVector(501.0, 0.0, 0.0), 500.f, 9.0), Now, MaxAge));
	// 3-4-5: (300, 400, 0) is exactly 500 away, so the radius is a real distance, not per axis.
	TestTrue(TEXT("diagonal on the radius"), ARNoise::CanHear(Listener, Noise(FVector(300.0, 400.0, 0.0), 500.f, 9.0), Now, MaxAge));
	TestTrue(TEXT("age exactly MaxAge"), ARNoise::CanHear(Listener, Noise(FVector(100.0, 0.0, 0.0), 500.f, 5.0), Now, MaxAge));
	TestFalse(TEXT("older than MaxAge"), ARNoise::CanHear(Listener, Noise(FVector(100.0, 0.0, 0.0), 500.f, 4.9), Now, MaxAge));
	TestFalse(TEXT("zero loudness is silent even on top of it"), ARNoise::CanHear(Listener, Noise(Listener, 0.f, 10.0), Now, MaxAge));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARNoisePickTest, "ActionRoguelike.Noise.Pick", AR_TEST_FLAGS)
bool FARNoisePickTest::RunTest(const FString& Parameters)
{
	const FVector Listener = FVector::ZeroVector;
	const double Now = 11.0;
	const double MaxAge = 5.0;

	// Scores (loudness - distance): [0] 500-300 = 200, [1] 250-100 = 150, [2] 100-50 = 50.
	TArray<ARNoise::FARNoiseEvent> Events;
	Events.Add(Noise(FVector(300.0, 0.0, 0.0), 500.f, 10.0));
	Events.Add(Noise(FVector(100.0, 0.0, 0.0), 250.f, 10.0));
	Events.Add(Noise(FVector(0.0, 0.0, 50.0), 100.f, 10.0));
	TestEqual(TEXT("highest score wins"), ARNoise::PickNoiseToInvestigate(Listener, Events, Now, MaxAge), 0);

	// A far loud noise (1000-400 = 600) beats a near quiet one (100-50 = 50): it is not simply the nearest.
	TArray<ARNoise::FARNoiseEvent> FarLoud;
	FarLoud.Add(Noise(FVector(50.0, 0.0, 0.0), 100.f, 10.0));
	FarLoud.Add(Noise(FVector(400.0, 0.0, 0.0), 1000.f, 10.0));
	TestEqual(TEXT("loud far beats quiet near"), ARNoise::PickNoiseToInvestigate(Listener, FarLoud, Now, MaxAge), 1);

	// [0] is 1200 away with radius 1000 (inaudible), [1] is 10 away but 10 s old (expired), [2] scores 200-100 = 100.
	TArray<ARNoise::FARNoiseEvent> Skipped;
	Skipped.Add(Noise(FVector(1200.0, 0.0, 0.0), 1000.f, 10.0));
	Skipped.Add(Noise(FVector(0.0, 0.0, 10.0), 900.f, 1.0));
	Skipped.Add(Noise(FVector(100.0, 0.0, 0.0), 200.f, 10.0));
	TestEqual(TEXT("inaudible and expired events are ignored"), ARNoise::PickNoiseToInvestigate(Listener, Skipped, Now, MaxAge), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARNoiseNothingToHearTest, "ActionRoguelike.Noise.NothingToHear", AR_TEST_FLAGS)
bool FARNoiseNothingToHearTest::RunTest(const FString& Parameters)
{
	const FVector Listener = FVector::ZeroVector;
	TArray<ARNoise::FARNoiseEvent> Events;
	TestEqual(TEXT("empty list"), ARNoise::PickNoiseToInvestigate(Listener, Events, 10.0, 5.0), static_cast<int32>(INDEX_NONE));

	// Age 10 - 0 = 10 > 5, so the only event is expired.
	Events.Add(Noise(FVector(10.0, 0.0, 0.0), 500.f, 0.0));
	TestEqual(TEXT("all expired"), ARNoise::PickNoiseToInvestigate(Listener, Events, 10.0, 5.0), static_cast<int32>(INDEX_NONE));

	// 1000 away with radius 500.
	Events.Reset();
	Events.Add(Noise(FVector(1000.0, 0.0, 0.0), 500.f, 10.0));
	TestEqual(TEXT("all out of range"), ARNoise::PickNoiseToInvestigate(Listener, Events, 10.0, 5.0), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARNoiseTieTest, "ActionRoguelike.Noise.Tie", AR_TEST_FLAGS)
bool FARNoiseTieTest::RunTest(const FString& Parameters)
{
	const FVector Listener = FVector::ZeroVector;
	const double Now = 10.0;
	const double MaxAge = 5.0;

	// Both score 200 (400-200 and 500-300); the later one (t=9) wins whatever its place in the list.
	const ARNoise::FARNoiseEvent Old = Noise(FVector(200.0, 0.0, 0.0), 400.f, 8.0);
	const ARNoise::FARNoiseEvent Recent = Noise(FVector(0.0, 300.0, 0.0), 500.f, 9.0);

	TArray<ARNoise::FARNoiseEvent> Events;
	Events.Add(Old);
	Events.Add(Recent);
	TestEqual(TEXT("tie goes to the most recent (second)"), ARNoise::PickNoiseToInvestigate(Listener, Events, Now, MaxAge), 1);

	Events.Reset();
	Events.Add(Recent);
	Events.Add(Old);
	TestEqual(TEXT("tie goes to the most recent (first)"), ARNoise::PickNoiseToInvestigate(Listener, Events, Now, MaxAge), 0);

	// Same score and same time: the first one stays.
	Events.Reset();
	Events.Add(Old);
	Events.Add(Old);
	TestEqual(TEXT("full tie keeps the first"), ARNoise::PickNoiseToInvestigate(Listener, Events, Now, MaxAge), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARNoiseEdgeTest, "ActionRoguelike.Noise.Edge", AR_TEST_FLAGS)
bool FARNoiseEdgeTest::RunTest(const FString& Parameters)
{
	// Listener standing on the event: distance 0, score = loudness 300, index 0; nothing divides by the distance.
	TArray<ARNoise::FARNoiseEvent> Events;
	Events.Add(Noise(FVector(5.0, 5.0, 5.0), 300.f, 10.0));
	TestEqual(TEXT("zero distance is fine"), ARNoise::PickNoiseToInvestigate(FVector(5.0, 5.0, 5.0), Events, 10.0, 0.0), 0);

	// MaxAge 0 only hears events from this very instant: age 0 <= 0, age 1 > 0.
	TestTrue(TEXT("zero MaxAge hears now"), ARNoise::CanHear(FVector::ZeroVector, Noise(FVector(1.0, 0.0, 0.0), 10.f, 10.0), 10.0, 0.0));
	TestFalse(TEXT("zero MaxAge ignores the past"), ARNoise::CanHear(FVector::ZeroVector, Noise(FVector(1.0, 0.0, 0.0), 10.f, 9.0), 10.0, 0.0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
