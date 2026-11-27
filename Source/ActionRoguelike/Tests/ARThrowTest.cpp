// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/ARThrow.h"

#define AR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

namespace
{
	constexpr double Gravity = -1000.0;

	bool IsFinite(const FVector& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
	}

	/** Flies the velocity for as long as it takes to cover the target's horizontal distance and returns how far the path ends from the target. */
	double MissDistance(const FVector& Start, const FVector& Target, const FVector& Velocity)
	{
		const double Time = FVector2D(Target.X - Start.X, Target.Y - Start.Y).Size() / FVector2D(Velocity.X, Velocity.Y).Size();
		return FVector::Dist(ARThrow::PointAt(Start, Velocity, Gravity, Time), Target);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARThrowPointAtTest, "ActionRoguelike.Throw.PointAt", AR_TEST_FLAGS)
bool FARThrowPointAtTest::RunTest(const FString& Parameters)
{
	const FVector Start(10.0, 20.0, 30.0);
	const FVector Velocity(100.0, 0.0, 200.0);

	TestTrue(TEXT("t=0 is the start"), ARThrow::PointAt(Start, Velocity, Gravity, 0.0).Equals(Start, 0.001));
	// t=0.5: x = 10 + 100*0.5 = 60, y = 20, z = 30 + 200*0.5 - 0.5*1000*0.25 = 30 + 100 - 125 = 5
	TestTrue(TEXT("t=0.5"), ARThrow::PointAt(Start, Velocity, Gravity, 0.5).Equals(FVector(60.0, 20.0, 5.0), 0.001));
	// Zero gravity is a straight line at t=2: x = 10 + 200 = 210, y = 20, z = 30 + 400 = 430
	TestTrue(TEXT("no gravity flies straight"), ARThrow::PointAt(Start, Velocity, 0.0, 2.0).Equals(FVector(210.0, 20.0, 430.0), 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARThrowSolveLaunchTest, "ActionRoguelike.Throw.SolveLaunch", AR_TEST_FLAGS)
bool FARThrowSolveLaunchTest::RunTest(const FString& Parameters)
{
	const FVector Start = FVector::ZeroVector;
	FVector Velocity;

	// Level target at d=500, v=1000, g=1000: disc = 1e12 - 1000*(1000*250000) = 7.5e11, sqrt = 866025.4,
	// tan = (1e6 - 866025.4) / (1000*500) = 0.267949 = tan(15 deg), so Velocity = (1000 cos15, 0, 1000 sin15) = (965.93, 0, 258.82).
	const FVector Level(500.0, 0.0, 0.0);
	TestTrue(TEXT("level target is reachable"), ARThrow::SolveLaunch(Start, Level, 1000.0, Gravity, Velocity));
	TestTrue(TEXT("lower arc, 15 degrees"), Velocity.Equals(FVector(965.93, 0.0, 258.82), 0.1));
	TestEqual(TEXT("launch speed is kept"), Velocity.Size(), 1000.0, 0.01);
	TestTrue(TEXT("path lands on the target"), MissDistance(Start, Level, Velocity) < 0.5);

	// Target 100 below: disc = 1e12 - 1000*(2.5e8 - 2e8) = 9.5e11 > 0, so it is reachable and the path ends on it.
	const FVector Below(500.0, 0.0, -100.0);
	TestTrue(TEXT("target below is reachable"), ARThrow::SolveLaunch(Start, Below, 1000.0, Gravity, Velocity));
	TestTrue(TEXT("path lands on the lower target"), MissDistance(Start, Below, Velocity) < 0.5);

	// (300, 400, 0) is 500 away like the level case, heading (0.6, 0.8), so Y/X = 4/3.
	const FVector Diagonal(300.0, 400.0, 0.0);
	TestTrue(TEXT("diagonal target is reachable"), ARThrow::SolveLaunch(Start, Diagonal, 1000.0, Gravity, Velocity));
	TestTrue(TEXT("diagonal keeps its heading"), FMath::IsNearlyEqual(Velocity.Y / Velocity.X, 4.0 / 3.0, 0.001));
	TestTrue(TEXT("diagonal lands on the target"), MissDistance(Start, Diagonal, Velocity) < 0.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARThrowUnreachableTest, "ActionRoguelike.Throw.Unreachable", AR_TEST_FLAGS)
bool FARThrowUnreachableTest::RunTest(const FString& Parameters)
{
	const FVector Start = FVector::ZeroVector;
	FVector Velocity(1.0, 2.0, 3.0);

	// Max range at v=1000, g=1000 is v^2/g = 1000, so d=2000 gives disc = 1e12 - 1000*(1000*4e6) < 0.
	TestFalse(TEXT("too far"), ARThrow::SolveLaunch(Start, FVector(2000.0, 0.0, 0.0), 1000.0, Gravity, Velocity));
	TestTrue(TEXT("unreachable leaves a zero velocity, not garbage"), Velocity.IsZero());

	TestFalse(TEXT("zero speed"), ARThrow::SolveLaunch(Start, FVector(500.0, 0.0, 0.0), 0.0, Gravity, Velocity));
	TestTrue(TEXT("zero speed gives a finite zero velocity"), Velocity.IsZero() && IsFinite(Velocity));
	TestFalse(TEXT("negative speed"), ARThrow::SolveLaunch(Start, FVector(500.0, 0.0, 0.0), -10.0, Gravity, Velocity));
	TestFalse(TEXT("no gravity cannot be solved"), ARThrow::SolveLaunch(Start, FVector(500.0, 0.0, 0.0), 1000.0, 0.0, Velocity));
	// 600 up and 500 away: disc = 1e12 - 1000*(1000*250000 + 2*600*1e6) = 1e12 - 1.45e12 < 0.
	TestFalse(TEXT("too high for the speed"), ARThrow::SolveLaunch(Start, FVector(500.0, 0.0, 600.0), 1000.0, Gravity, Velocity));
	TestTrue(TEXT("still a finite zero velocity"), Velocity.IsZero() && IsFinite(Velocity));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARThrowStraightUpTest, "ActionRoguelike.Throw.StraightUp", AR_TEST_FLAGS)
bool FARThrowStraightUpTest::RunTest(const FString& Parameters)
{
	const FVector Start = FVector::ZeroVector;
	FVector Velocity;

	// Apex at v=1000, g=1000 is 1000^2 / 2000 = 500, so a target 200 above is reachable by going straight up.
	TestTrue(TEXT("200 up is reachable"), ARThrow::SolveLaunch(Start, FVector(0.0, 0.0, 200.0), 1000.0, Gravity, Velocity));
	TestTrue(TEXT("velocity is straight up"), Velocity.Equals(FVector(0.0, 0.0, 1000.0), 0.001));

	TestFalse(TEXT("600 up is above the apex"), ARThrow::SolveLaunch(Start, FVector(0.0, 0.0, 600.0), 1000.0, Gravity, Velocity));
	TestTrue(TEXT("unreachable up is a finite zero velocity"), Velocity.IsZero() && IsFinite(Velocity));

	TestTrue(TEXT("straight down is reachable"), ARThrow::SolveLaunch(Start, FVector(0.0, 0.0, -300.0), 1000.0, Gravity, Velocity));
	TestTrue(TEXT("straight down is finite"), IsFinite(Velocity));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARThrowSamplePathTest, "ActionRoguelike.Throw.SamplePath", AR_TEST_FLAGS)
bool FARThrowSamplePathTest::RunTest(const FString& Parameters)
{
	const FVector Start = FVector::ZeroVector;
	const FVector Velocity(100.0, 0.0, 100.0);
	const double LowGravity = -200.0;
	TArray<FVector> Points;

	// Step 0.4: t=0.4 -> (40, 0, 40 - 16 = 24); t=0.8 -> (80, 0, 80 - 64 = 16); t=1.2 -> z = 120 - 144 = -24, below ground 0.
	// The cut lies 16/40 = 0.4 of the way from (80, 0, 16) to (120, 0, -24): (96, 0, 0). So 4 points in total.
	ARThrow::SamplePath(Start, Velocity, LowGravity, 10, 0.4, 0.0, Points);
	TestEqual(TEXT("stops at the ground crossing"), Points.Num(), 4);
	if (Points.Num() == 4)
	{
		TestTrue(TEXT("first point is the start"), Points[0].Equals(Start, 0.001));
		TestTrue(TEXT("second point"), Points[1].Equals(FVector(40.0, 0.0, 24.0), 0.01));
		TestTrue(TEXT("third point"), Points[2].Equals(FVector(80.0, 0.0, 16.0), 0.01));
		TestTrue(TEXT("last point sits on the ground"), Points[3].Equals(FVector(96.0, 0.0, 0.0), 0.01));
	}

	bool bAllFinite = true;
	for (const FVector& Point : Points)
	{
		bAllFinite &= IsFinite(Point);
	}
	TestTrue(TEXT("no NaN in the preview"), bAllFinite);

	ARThrow::SamplePath(Start, Velocity, LowGravity, 3, 0.4, 0.0, Points);
	TestEqual(TEXT("Count caps the points before the ground"), Points.Num(), 3);

	ARThrow::SamplePath(Start, Velocity, LowGravity, 1, 0.4, 0.0, Points);
	TestEqual(TEXT("Count 1 is only the start"), Points.Num(), 1);

	ARThrow::SamplePath(Start, Velocity, LowGravity, 0, 0.4, 0.0, Points);
	TestEqual(TEXT("Count 0 is empty"), Points.Num(), 0);

	// Thrown from below the ground: nothing past the start is previewed.
	ARThrow::SamplePath(FVector(0.0, 0.0, -50.0), FVector(0.0, 0.0, -10.0), LowGravity, 5, 0.4, 0.0, Points);
	TestEqual(TEXT("start below the ground is only the start"), Points.Num(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
