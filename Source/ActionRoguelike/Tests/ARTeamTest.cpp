// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/ARTeam.h"

#define AR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARTeamHostilityTest, "ActionRoguelike.Team.Hostility", AR_TEST_FLAGS)
bool FARTeamHostilityTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("player vs enemy is hostile"), ARTeam::IsHostile(ARTeam::Player, ARTeam::Enemy));
	TestTrue(TEXT("hostility is symmetric"), ARTeam::IsHostile(ARTeam::Enemy, ARTeam::Player));
	TestFalse(TEXT("same team is not hostile (players)"), ARTeam::IsHostile(ARTeam::Player, ARTeam::Player));
	TestFalse(TEXT("same team is not hostile (enemies)"), ARTeam::IsHostile(ARTeam::Enemy, ARTeam::Enemy));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARTeamNoTeamTest, "ActionRoguelike.Team.NoTeamIsHostileToNobody", AR_TEST_FLAGS)
bool FARTeamNoTeamTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("no team vs player"), ARTeam::IsHostile(ARTeam::NoTeam, ARTeam::Player));
	TestFalse(TEXT("enemy vs no team"), ARTeam::IsHostile(ARTeam::Enemy, ARTeam::NoTeam));
	TestFalse(TEXT("no team vs no team"), ARTeam::IsHostile(ARTeam::NoTeam, ARTeam::NoTeam));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
