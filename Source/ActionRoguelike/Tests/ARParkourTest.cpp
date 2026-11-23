// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/ARParkour.h"

#define AR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

namespace
{
	ARParkour::FObstacle Wall(float Height, float Thickness = 40.f)
	{
		ARParkour::FObstacle Obstacle;
		Obstacle.Height = Height;
		Obstacle.Thickness = Thickness;
		Obstacle.ClearanceAbove = 300.f;
		Obstacle.bLandingBehind = true;
		return Obstacle;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourVaultTest, "ActionRoguelike.Parkour.Vault", AR_TEST_FLAGS)
bool FARParkourVaultTest::RunTest(const FString& Parameters)
{
	using ARParkour::EMove;
	TestTrue(TEXT("height 50 is the lowest vault"), ARParkour::Classify(Wall(50.f)) == EMove::Vault);
	TestTrue(TEXT("height 90 vaults"), ARParkour::Classify(Wall(90.f)) == EMove::Vault);
	TestTrue(TEXT("height 95 is the highest vault"), ARParkour::Classify(Wall(95.f)) == EMove::Vault);
	TestTrue(TEXT("thickness 59 still vaults"), ARParkour::Classify(Wall(90.f, 59.f)) == EMove::Vault);
	TestTrue(TEXT("thickness 60 is climbed onto instead"), ARParkour::Classify(Wall(90.f, 60.f)) == EMove::Mantle);
	TestTrue(TEXT("a very long low obstacle is climbed onto"), ARParkour::Classify(Wall(90.f, 600.f)) == EMove::Mantle);
	TestTrue(TEXT("but too low to climb onto is still nothing"), ARParkour::Classify(Wall(49.f, 600.f)) == EMove::None);

	ARParkour::FObstacle NoLanding = Wall(90.f);
	NoLanding.bLandingBehind = false;
	TestTrue(TEXT("no floor behind: no vault, climb onto it"), ARParkour::Classify(NoLanding) == EMove::Mantle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourMantleTest, "ActionRoguelike.Parkour.Mantle", AR_TEST_FLAGS)
bool FARParkourMantleTest::RunTest(const FString& Parameters)
{
	using ARParkour::EMove;
	TestTrue(TEXT("just above the vault limit mantles"), ARParkour::Classify(Wall(96.f)) == EMove::Mantle);
	TestTrue(TEXT("height 200 mantles"), ARParkour::Classify(Wall(200.f)) == EMove::Mantle);
	TestTrue(TEXT("height 205 is the highest mantle"), ARParkour::Classify(Wall(205.f)) == EMove::Mantle);
	TestTrue(TEXT("height 206 is too high"), ARParkour::Classify(Wall(206.f)) == EMove::None);
	TestTrue(TEXT("a 240 cm wall is a plain jump"), ARParkour::Classify(Wall(240.f)) == EMove::None);
	TestTrue(TEXT("a thick block can be mantled"), ARParkour::Classify(Wall(200.f, 600.f)) == EMove::Mantle);

	ARParkour::FObstacle NoLanding = Wall(200.f);
	NoLanding.bLandingBehind = false;
	TestTrue(TEXT("mantle does not need floor behind"), ARParkour::Classify(NoLanding) == EMove::Mantle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourRejectTest, "ActionRoguelike.Parkour.Rejected", AR_TEST_FLAGS)
bool FARParkourRejectTest::RunTest(const FString& Parameters)
{
	using ARParkour::EMove;
	TestTrue(TEXT("height 49 is too low"), ARParkour::Classify(Wall(49.f)) == EMove::None);
	TestTrue(TEXT("height 0 is a floor"), ARParkour::Classify(Wall(0.f)) == EMove::None);

	ARParkour::FObstacle LowCeiling = Wall(90.f);
	LowCeiling.ClearanceAbove = 191.f;
	TestTrue(TEXT("clearance 191 is not enough"), ARParkour::Classify(LowCeiling) == EMove::None);
	LowCeiling.ClearanceAbove = 192.f;
	TestTrue(TEXT("clearance 192 is enough"), ARParkour::Classify(LowCeiling) == EMove::Vault);

	ARParkour::FObstacle Ramp = Wall(200.f);
	Ramp.SurfaceSlopeDegrees = 31.f;
	TestTrue(TEXT("slope 31 is not standable"), ARParkour::Classify(Ramp) == EMove::None);
	Ramp.SurfaceSlopeDegrees = 30.f;
	TestTrue(TEXT("slope 30 is standable"), ARParkour::Classify(Ramp) == EMove::Mantle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourPickClipTest, "ActionRoguelike.Parkour.PickClip", AR_TEST_FLAGS)
bool FARParkourPickClipTest::RunTest(const FString& Parameters)
{
	using namespace ARParkour;
	const TArray<FClipRule> Rules = {
		{EMove::Vault, 50.f, 120.f, EVariant::Stand},   // 0
		{EMove::Vault, 50.f, 120.f, EVariant::Run},     // 1
		{EMove::Mantle, 120.f, 170.f, EVariant::Stand}, // 2
		{EMove::Mantle, 120.f, 170.f, EVariant::Run},   // 3
		{EMove::Mantle, 170.f, 240.f, EVariant::Walk},  // 4
	};

	TestEqual(TEXT("vault, run: the run clip"), PickClip(Rules, EMove::Vault, 90.f, EVariant::Run), 1);
	TestEqual(TEXT("vault, stand: the stand clip"), PickClip(Rules, EMove::Vault, 90.f, EVariant::Stand), 0);
	TestEqual(TEXT("vault, walk: no walk clip, the first of the two nearest wins"), PickClip(Rules, EMove::Vault, 90.f, EVariant::Walk), 0);
	TestEqual(TEXT("low mantle, run"), PickClip(Rules, EMove::Mantle, 150.f, EVariant::Run), 3);
	TestEqual(TEXT("high mantle has only a walk clip, so run uses it"), PickClip(Rules, EMove::Mantle, 200.f, EVariant::Run), 4);

	TestEqual(TEXT("height 120 is still a vault"), PickClip(Rules, EMove::Vault, 120.f, EVariant::Stand), 0);
	TestEqual(TEXT("height 130 has no vault clip"), PickClip(Rules, EMove::Vault, 130.f, EVariant::Stand), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("height 170 belongs to both mantle ranges, the better variant wins"), PickClip(Rules, EMove::Mantle, 170.f, EVariant::Stand), 2);
	TestEqual(TEXT("height 241 has no clip"), PickClip(Rules, EMove::Mantle, 241.f, EVariant::Stand), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("no move, no clip"), PickClip(Rules, EMove::None, 90.f, EVariant::Stand), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("empty table"), PickClip(TArray<FClipRule>(), EMove::Vault, 90.f, EVariant::Stand), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourVariantTest, "ActionRoguelike.Parkour.VariantForSpeed", AR_TEST_FLAGS)
bool FARParkourVariantTest::RunTest(const FString& Parameters)
{
	using namespace ARParkour;
	TestTrue(TEXT("0 is stand"), VariantForSpeed(0.f) == EVariant::Stand);
	TestTrue(TEXT("49.9 is stand"), VariantForSpeed(49.9f) == EVariant::Stand);
	TestTrue(TEXT("50 is walk"), VariantForSpeed(50.f) == EVariant::Walk);
	TestTrue(TEXT("299 is walk"), VariantForSpeed(299.f) == EVariant::Walk);
	TestTrue(TEXT("300 is run"), VariantForSpeed(300.f) == EVariant::Run);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourHandWeightTest, "ActionRoguelike.Parkour.HandIK.Weight", AR_TEST_FLAGS)
bool FARParkourHandWeightTest::RunTest(const FString& Parameters)
{
	using namespace ARParkour;
	// Window 1.0 .. 1.5 with a 0.2 s ramp on each side.
	TestEqual(TEXT("long before the window"), HandWeight(0.5f, 1.0f, 1.5f, 0.2f), 0.f);
	TestEqual(TEXT("ramp starts at Start - Ramp"), HandWeight(0.8f, 1.0f, 1.5f, 0.2f), 0.f);
	TestEqual(TEXT("halfway up the ramp"), HandWeight(0.9f, 1.0f, 1.5f, 0.2f), 0.5f, 0.001f);
	TestEqual(TEXT("full at Start"), HandWeight(1.0f, 1.0f, 1.5f, 0.2f), 1.f);
	TestEqual(TEXT("full inside"), HandWeight(1.25f, 1.0f, 1.5f, 0.2f), 1.f);
	TestEqual(TEXT("full at End"), HandWeight(1.5f, 1.0f, 1.5f, 0.2f), 1.f);
	TestEqual(TEXT("halfway down the ramp"), HandWeight(1.6f, 1.0f, 1.5f, 0.2f), 0.5f, 0.001f);
	TestEqual(TEXT("gone at End + Ramp"), HandWeight(1.7f, 1.0f, 1.5f, 0.2f), 0.f);
	TestEqual(TEXT("no ramp: a step"), HandWeight(0.99f, 1.0f, 1.5f, 0.f), 0.f);
	TestEqual(TEXT("no window (End <= Start): the animation keeps the hand"), HandWeight(1.0f, 1.0f, 1.0f, 0.2f), 0.f);
	TestEqual(TEXT("inverted window"), HandWeight(1.2f, 1.5f, 1.0f, 0.2f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourHandCandidateTest, "ActionRoguelike.Parkour.HandIK.Candidate", AR_TEST_FLAGS)
bool FARParkourHandCandidateTest::RunTest(const FString& Parameters)
{
	using namespace ARParkour;
	const FVector Edge(100.f, 0.f, 150.f);

	// Facing +X: right is +Y (UE's X forward, Y right), so side -1 is the left hand at -Y.
	const FVector Left = HandCandidate(Edge, FVector::ForwardVector, -1, 20.f, 5.f);
	const FVector Right = HandCandidate(Edge, FVector::ForwardVector, 1, 20.f, 5.f);
	TestTrue(TEXT("left hand: 5 cm onto the top, 20 cm to the left"), Left.Equals(FVector(105.f, -20.f, 150.f), 0.01f));
	TestTrue(TEXT("right hand: 5 cm onto the top, 20 cm to the right"), Right.Equals(FVector(105.f, 20.f, 150.f), 0.01f));

	// Facing +Y: right is -X.
	const FVector RightY = HandCandidate(Edge, FVector::RightVector, 1, 20.f, 5.f);
	TestTrue(TEXT("rotated wall: the offsets follow the move direction"), RightY.Equals(FVector(80.f, 5.f, 150.f), 0.01f));

	// A tilted direction is flattened: the target stays on the edge height.
	const FVector Tilted = HandCandidate(Edge, FVector(1.f, 0.f, 0.5f), 1, 20.f, 5.f);
	TestEqual(TEXT("height is the edge height"), Tilted.Z, 150.0, 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourFootLiftTest, "ActionRoguelike.Parkour.FootIK.Lift", AR_TEST_FLAGS)
bool FARParkourFootLiftTest::RunTest(const FString& Parameters)
{
	using namespace ARParkour;
	TestEqual(TEXT("sole 10 below the ledge: lift 10"), FootLift(0.f, -10.f, 0.f, 20.f), 10.f);
	TestEqual(TEXT("already lifted 10 and the sole is on the ledge: keep 10"), FootLift(10.f, 0.f, 0.f, 20.f), 10.f);
	TestEqual(TEXT("sole 4 above the ledge after a lift of 10: ease back to 6"), FootLift(10.f, 4.f, 0.f, 20.f), 6.f);
	TestEqual(TEXT("sole well above the ledge: no lift"), FootLift(0.f, 30.f, 0.f, 20.f), 0.f);
	TestEqual(TEXT("capped"), FootLift(0.f, -50.f, 0.f, 20.f), 20.f);
	TestEqual(TEXT("a hovering sole is only pulled down when lowering is allowed"), FootLift(0.f, 9.f, 0.f, 20.f, -15.f), -9.f);
	TestEqual(TEXT("pulling down is capped"), FootLift(0.f, 40.f, 0.f, 20.f, -15.f), -15.f);
	TestEqual(TEXT("already pulled down 9 and the sole is on the ledge: keep -9"), FootLift(-9.f, 0.f, 0.f, 20.f, -15.f), -9.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourMoveCancelTest, "ActionRoguelike.Parkour.MoveCancel", AR_TEST_FLAGS)
bool FARParkourMoveCancelTest::RunTest(const FString& Parameters)
{
	using namespace ARParkour;
	TestTrue(TEXT("after the cancel time, input held, on the target"), MoveCancelAllowed(1.2f, 1.0f, true, 2.f));
	TestFalse(TEXT("a clip without a cancel time always plays out"), MoveCancelAllowed(1.2f, 0.f, true, 2.f));
	TestFalse(TEXT("too early in the clip"), MoveCancelAllowed(0.8f, 1.0f, true, 2.f));
	TestFalse(TEXT("no input: the clip goes on"), MoveCancelAllowed(1.2f, 1.0f, false, 2.f));
	TestFalse(TEXT("not on the target yet: no pop"), MoveCancelAllowed(1.2f, 1.0f, true, MoveCancelArrivalCm + 1.f));
	TestTrue(TEXT("exactly at the cancel time"), MoveCancelAllowed(1.0f, 1.0f, true, MoveCancelArrivalCm));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourStartMatchTest, "ActionRoguelike.Parkour.StartMatch", AR_TEST_FLAGS)
bool FARParkourStartMatchTest::RunTest(const FString& Parameters)
{
	using namespace ARParkour;
	// A clip that accelerates: 0.0 s at 0 cm/s with 200 cm of approach left, 0.1 s at 150 with 120, 0.2 s at 300 with 40.
	const TArray<FStartCandidate> Clip = {{0.0f, 200.f, 0.f}, {0.1f, 120.f, 150.f}, {0.2f, 40.f, 300.f}};

	TestEqual(TEXT("empty list: nothing to start from"), ChooseStart({}, 100.f, 100.f), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("standing, far from the wall: the start of the clip"), ChooseStart(Clip, 200.f, 0.f), 0);
	TestEqual(TEXT("running, 40 cm left: the late candidate"), ChooseStart(Clip, 40.f, 300.f), 2);
	TestEqual(TEXT("walking, 120 cm left: the middle one"), ChooseStart(Clip, 120.f, 150.f), 1);
	// Speed and distance disagree: 100 cm to cover while running at 300 cm/s. Costs: candidate 0 = 16 + 9, candidate 1 = 0.64 + 2.25, candidate 2 = 5.76 + 0.
	TestEqual(TEXT("both count: the middle candidate fits best"), ChooseStart(Clip, 100.f, 300.f), 1);
	TestEqual(TEXT("first wins a tie"), ChooseStart({{0.f, 100.f, 100.f}, {0.1f, 100.f, 100.f}}, 100.f, 100.f), 0);

	// The warp scales the root speed by needed / remaining: a clip that moves at 200 cm/s with 50 cm of approach left moves at 400 when 100 cm are needed.
	float Cost = -1.f;
	ChooseStart({{0.f, 50.f, 200.f}}, 100.f, 400.f, &Cost);
	TestEqual(TEXT("the cost counts the distance error and the scaled speed"), Cost, 4.f, 0.001f);
	// Backing away from the wall is worse than a clip that is slower than the character: 90 cm to the wall, the clip needs 120 of approach.
	float Backing = 0.f;
	float Slow = 0.f;
	ChooseStart({{0.f, 30.f, 400.f}}, -30.f, 500.f, &Backing);
	ChooseStart({{0.f, 30.f, 200.f}}, 18.f, 500.f, &Slow);
	TestTrue(TEXT("backing away costs more than a slower clip"), Backing > Slow);

	// The feet: two candidates that fit equally in distance and speed, the one whose feet are closer to the character's feet wins.
	FStartCandidate A;
	A.Time = 0.f;
	A.Remaining = 100.f;
	A.Speed = 100.f;
	A.Feet = FVector4(30.0, 0.0, -30.0, 0.0);
	A.bHasFeet = true;
	FStartCandidate B = A;
	B.Time = 0.1f;
	B.Feet = FVector4(-30.0, 0.0, 30.0, 0.0);
	const FVector4 Query(-25.0, 0.0, 25.0, 0.0);
	TestEqual(TEXT("closer feet win"), ChooseStart({A, B}, 100.f, 100.f, nullptr, &Query), 1);
	TestEqual(TEXT("no feet query: the first wins"), ChooseStart({A, B}, 100.f, 100.f), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
