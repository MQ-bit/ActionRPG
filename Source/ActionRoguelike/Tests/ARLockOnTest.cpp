// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/ARLockOn.h"

#define AR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

namespace
{
	using ARLockOn::FCandidate;

	// Facing +X from the origin; offsets are (forward, right, up).
	FCandidate At(float Forward, float Right = 0.f, float Up = 0.f)
	{
		FCandidate Candidate;
		Candidate.Location = FVector(Forward, Right, Up);
		return Candidate;
	}

	int32 Pick(const TArray<FCandidate>& Candidates)
	{
		return ARLockOn::PickTarget(Candidates, FVector::ZeroVector, FVector::ForwardVector);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARLockOnPickDistanceTest, "ActionRoguelike.LockOn.Pick.Distance", AR_TEST_FLAGS)
bool FARLockOnPickDistanceTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("no candidates"), Pick({}), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("1200 is still in reach"), Pick({At(1200.f)}), 0);
	TestEqual(TEXT("1201 is out of reach"), Pick({At(1201.f)}), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("the nearest wins"), Pick({At(900.f), At(300.f), At(600.f)}), 1);
	TestEqual(TEXT("height counts for the distance"), Pick({At(0.f, 0.f, 1300.f)}), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARLockOnPickAngleTest, "ActionRoguelike.LockOn.Pick.Angle", AR_TEST_FLAGS)
bool FARLockOnPickAngleTest::RunTest(const FString& Parameters)
{
	const float Tan59 = FMath::Tan(FMath::DegreesToRadians(59.f));
	const float Tan61 = FMath::Tan(FMath::DegreesToRadians(61.f));
	TestEqual(TEXT("59 degrees to the side is inside"), Pick({At(100.f, 100.f * Tan59)}), 0);
	TestEqual(TEXT("61 degrees to the side is outside"), Pick({At(100.f, 100.f * Tan61)}), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("left counts like right"), Pick({At(100.f, -100.f * Tan59)}), 0);
	TestEqual(TEXT("behind is outside"), Pick({At(-100.f)}), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("height is ignored for the angle"), Pick({At(100.f, 0.f, 80.f)}), 0);
	TestEqual(TEXT("a candidate on top of the origin is always in front"), Pick({At(0.f)}), 0);
	// The angle filter applies before the distance ranking: the near one is out of the view cone.
	TestEqual(TEXT("a near candidate outside the cone loses to a far one inside"), Pick({At(0.f, 200.f), At(500.f)}), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARLockOnPickFilterTest, "ActionRoguelike.LockOn.Pick.Filter", AR_TEST_FLAGS)
bool FARLockOnPickFilterTest::RunTest(const FString& Parameters)
{
	FCandidate Dead = At(100.f);
	Dead.bAlive = false;
	FCandidate Hidden = At(100.f);
	Hidden.bVisible = false;
	TestEqual(TEXT("a dead one is skipped"), Pick({Dead, At(500.f)}), 1);
	TestEqual(TEXT("an occluded one is skipped"), Pick({Hidden, At(500.f)}), 1);
	TestEqual(TEXT("only dead or hidden: nothing"), Pick({Dead, Hidden}), static_cast<int32>(INDEX_NONE));

	// A tie within 1 cm goes to the more frontal one, whatever the order.
	const float Cos40 = FMath::Cos(FMath::DegreesToRadians(40.f));
	const float Sin40 = FMath::Sin(FMath::DegreesToRadians(40.f));
	const FCandidate Front = At(300.f);                            // 300 cm straight ahead
	const FCandidate Side = At(300.f * Cos40, 300.f * Sin40);     // 300 cm at 40 degrees
	TestEqual(TEXT("tie: the more frontal wins (frontal last)"), Pick({Side, Front}), 1);
	TestEqual(TEXT("tie: the more frontal wins (frontal first)"), Pick({Front, Side}), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARLockOnReleaseTest, "ActionRoguelike.LockOn.Release", AR_TEST_FLAGS)
bool FARLockOnReleaseTest::RunTest(const FString& Parameters)
{
	using ARLockOn::ShouldRelease;
	TestFalse(TEXT("alive, near, in sight: keep"), ShouldRelease(500.f, 0.f, true));
	TestTrue(TEXT("dead: release"), ShouldRelease(500.f, 0.f, false));
	TestFalse(TEXT("1440 is the last distance kept"), ShouldRelease(1440.f, 0.f, true));
	TestTrue(TEXT("1441 is too far"), ShouldRelease(1441.f, 0.f, true));
	TestFalse(TEXT("out of sight 0.99 s: keep"), ShouldRelease(500.f, 0.99f, true));
	TestTrue(TEXT("out of sight 1.0 s: release"), ShouldRelease(500.f, 1.f, true));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
