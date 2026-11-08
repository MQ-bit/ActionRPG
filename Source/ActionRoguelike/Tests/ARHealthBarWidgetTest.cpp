// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "UI/ARHealthBarWidget.h"

#define AR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARHealthPercentTest, "ActionRoguelike.UI.HealthPercent", AR_TEST_FLAGS)
bool FARHealthPercentTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("full health"), UARHealthBarWidget::ComputeHealthPercent(100.f, 100.f), 1.f);
	TestEqual(TEXT("half health"), UARHealthBarWidget::ComputeHealthPercent(50.f, 100.f), 0.5f);
	TestEqual(TEXT("dead"), UARHealthBarWidget::ComputeHealthPercent(0.f, 100.f), 0.f);
	TestEqual(TEXT("health above max is clamped"), UARHealthBarWidget::ComputeHealthPercent(150.f, 100.f), 1.f);
	TestEqual(TEXT("negative health is clamped"), UARHealthBarWidget::ComputeHealthPercent(-10.f, 100.f), 0.f);
	TestEqual(TEXT("zero max health reads as empty"), UARHealthBarWidget::ComputeHealthPercent(10.f, 0.f), 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
