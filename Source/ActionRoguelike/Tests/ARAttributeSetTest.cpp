// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GAS/ARAttributeSet.h"
#include "UObject/Package.h"

#define AR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

namespace
{
	/** Runs PreAttributeChange (current value clamp) and returns the clamped value. */
	float ClampCurrent(UARAttributeSet* Set, const FGameplayAttribute& Attribute, float Value)
	{
		Set->PreAttributeChange(Attribute, Value);
		return Value;
	}

	/** Runs PreAttributeBaseChange (base value clamp) and returns the clamped value. */
	float ClampBase(UARAttributeSet* Set, const FGameplayAttribute& Attribute, float Value)
	{
		Set->PreAttributeBaseChange(Attribute, Value);
		return Value;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARAttributeDefaultsTest, "ActionRoguelike.GAS.AttributeSet.Defaults", AR_TEST_FLAGS)
bool FARAttributeDefaultsTest::RunTest(const FString& Parameters)
{
	UARAttributeSet* Set = NewObject<UARAttributeSet>(GetTransientPackage());
	TestEqual(TEXT("Health starts at 100"), Set->GetHealth(), 100.f);
	TestEqual(TEXT("MaxHealth starts at 100"), Set->GetMaxHealth(), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARAttributeHealthClampTest, "ActionRoguelike.GAS.AttributeSet.HealthCurrentClamp", AR_TEST_FLAGS)
bool FARAttributeHealthClampTest::RunTest(const FString& Parameters)
{
	UARAttributeSet* Set = NewObject<UARAttributeSet>(GetTransientPackage());
	const FGameplayAttribute Health = UARAttributeSet::GetHealthAttribute();

	TestEqual(TEXT("below zero clamps to 0"), ClampCurrent(Set, Health, -5.f), 0.f);
	TestEqual(TEXT("above max clamps to max"), ClampCurrent(Set, Health, 150.f), 100.f);
	TestEqual(TEXT("in range is untouched"), ClampCurrent(Set, Health, 55.f), 55.f);

	Set->InitMaxHealth(50.f);
	TestEqual(TEXT("clamp follows a changed MaxHealth"), ClampCurrent(Set, Health, 80.f), 50.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARAttributeHealthBaseClampTest, "ActionRoguelike.GAS.AttributeSet.HealthBaseClamp", AR_TEST_FLAGS)
bool FARAttributeHealthBaseClampTest::RunTest(const FString& Parameters)
{
	// Review finding: instant effects change the base value; without this clamp overheal/overkill hides in the base.
	UARAttributeSet* Set = NewObject<UARAttributeSet>(GetTransientPackage());
	const FGameplayAttribute Health = UARAttributeSet::GetHealthAttribute();

	TestEqual(TEXT("overheal does not accumulate in the base value"), ClampBase(Set, Health, 150.f), 100.f);
	TestEqual(TEXT("overkill does not accumulate in the base value"), ClampBase(Set, Health, -20.f), 0.f);
	TestEqual(TEXT("in range is untouched"), ClampBase(Set, Health, 40.f), 40.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARAttributeMaxHealthClampTest, "ActionRoguelike.GAS.AttributeSet.MaxHealthMinimum", AR_TEST_FLAGS)
bool FARAttributeMaxHealthClampTest::RunTest(const FString& Parameters)
{
	UARAttributeSet* Set = NewObject<UARAttributeSet>(GetTransientPackage());
	const FGameplayAttribute MaxHealth = UARAttributeSet::GetMaxHealthAttribute();

	TestEqual(TEXT("current: MaxHealth never drops below 1"), ClampCurrent(Set, MaxHealth, 0.f), 1.f);
	TestEqual(TEXT("base: MaxHealth never drops below 1"), ClampBase(Set, MaxHealth, -10.f), 1.f);
	TestEqual(TEXT("larger MaxHealth is allowed"), ClampCurrent(Set, MaxHealth, 250.f), 250.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
