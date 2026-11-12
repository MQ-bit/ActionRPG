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
	TestTrue(TEXT("height 120 is the highest vault"), ARParkour::Classify(Wall(120.f)) == EMove::Vault);
	TestTrue(TEXT("thickness 150 still vaults"), ARParkour::Classify(Wall(90.f, 150.f)) == EMove::Vault);
	TestTrue(TEXT("thickness 151 does not vault"), ARParkour::Classify(Wall(90.f, 151.f)) == EMove::None);

	ARParkour::FObstacle NoLanding = Wall(90.f);
	NoLanding.bLandingBehind = false;
	TestTrue(TEXT("no floor behind: no vault"), ARParkour::Classify(NoLanding) == EMove::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourMantleTest, "ActionRoguelike.Parkour.Mantle", AR_TEST_FLAGS)
bool FARParkourMantleTest::RunTest(const FString& Parameters)
{
	using ARParkour::EMove;
	TestTrue(TEXT("just above the vault limit mantles"), ARParkour::Classify(Wall(121.f)) == EMove::Mantle);
	TestTrue(TEXT("height 200 mantles"), ARParkour::Classify(Wall(200.f)) == EMove::Mantle);
	TestTrue(TEXT("height 240 is the highest mantle"), ARParkour::Classify(Wall(240.f)) == EMove::Mantle);
	TestTrue(TEXT("height 241 is too high"), ARParkour::Classify(Wall(241.f)) == EMove::None);
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

#endif // WITH_DEV_AUTOMATION_TESTS
