// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "AbilitySystemComponent.h"
#include "Character/AREnemyCharacter.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GAS/ARAbilitySystemComponent.h"
#include "GAS/ARAttributeSet.h"
#include "GAS/ARGameplayTags.h"
#include "GAS/ARGE_Damage.h"
#include "UObject/Package.h"

#define AR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

namespace
{
	/** A test world with one enemy character; its ASC and attribute set are initialised by BeginPlay. */
	struct FARGasFixture
	{
		FTestWorldWrapper WorldWrapper;
		AAREnemyCharacter* Actor = nullptr;
		UARAbilitySystemComponent* ASC = nullptr;
		const UARAttributeSet* Set = nullptr;

		bool Init(FAutomationTestBase& Test)
		{
			if (!WorldWrapper.CreateTestWorld(EWorldType::Game) || !WorldWrapper.BeginPlayInTestWorld())
			{
				WorldWrapper.ForwardErrorMessages(&Test);
				return false;
			}

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Actor = WorldWrapper.GetTestWorld()->SpawnActor<AAREnemyCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
			ASC = Actor ? Actor->FindComponentByClass<UARAbilitySystemComponent>() : nullptr;
			Set = ASC ? ASC->GetSet<UARAttributeSet>() : nullptr;
			return Test.TestNotNull(TEXT("enemy spawned with an ASC and attribute set"), Set);
		}

		float Health() const { return ASC->GetNumericAttribute(UARAttributeSet::GetHealthAttribute()); }
		float HealthBase() const { return ASC->GetNumericAttributeBase(UARAttributeSet::GetHealthAttribute()); }
		float MaxHealth() const { return ASC->GetNumericAttribute(UARAttributeSet::GetMaxHealthAttribute()); }

		/** Applies the project damage effect with a SetByCaller amount (negative = damage). */
		void ApplyDamage(float Amount)
		{
			FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UARGE_Damage::StaticClass(), 1.f, ASC->MakeEffectContext());
			Spec.Data->SetSetByCallerMagnitude(TAG_Data_Damage.GetTag(), Amount);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}

		/** Applies an instant effect that changes one attribute (Additive or Override). */
		void ApplyInstant(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op, float Magnitude)
		{
			UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
			Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
			FGameplayModifierInfo Modifier;
			Modifier.Attribute = Attribute;
			Modifier.ModifierOp = Op;
			Modifier.ModifierMagnitude = FScalableFloat(Magnitude);
			Effect->Modifiers.Add(Modifier);
			ASC->ApplyGameplayEffectToSelf(Effect, 1.f, ASC->MakeEffectContext());
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARGasDamageEffectTest, "ActionRoguelike.GAS.Integration.DamageEffectReducesHealth", AR_TEST_FLAGS)
bool FARGasDamageEffectTest::RunTest(const FString& Parameters)
{
	FARGasFixture F;
	if (!F.Init(*this)) { return false; }

	TestEqual(TEXT("starts at 100"), F.Health(), 100.f);
	F.ApplyDamage(-30.f);
	TestEqual(TEXT("damage 30 -> 70"), F.Health(), 70.f);
	F.ApplyDamage(-30.f);
	TestEqual(TEXT("damage 30 again -> 40"), F.Health(), 40.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARGasOverkillTest, "ActionRoguelike.GAS.Integration.OverkillStopsAtZeroInCurrentAndBase", AR_TEST_FLAGS)
bool FARGasOverkillTest::RunTest(const FString& Parameters)
{
	FARGasFixture F;
	if (!F.Init(*this)) { return false; }

	F.ApplyDamage(-250.f);
	TestEqual(TEXT("current Health stops at 0"), F.Health(), 0.f);
	TestEqual(TEXT("base Health stops at 0 (no hidden negative value)"), F.HealthBase(), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARGasOverhealTest, "ActionRoguelike.GAS.Integration.OverhealDoesNotHideInBaseValue", AR_TEST_FLAGS)
bool FARGasOverhealTest::RunTest(const FString& Parameters)
{
	// Review finding: a heal past MaxHealth used to leave base Health at 120, so the next hit showed no damage.
	FARGasFixture F;
	if (!F.Init(*this)) { return false; }

	F.ApplyDamage(-30.f);
	TestEqual(TEXT("70 after the first hit"), F.Health(), 70.f);

	F.ApplyInstant(UARAttributeSet::GetHealthAttribute(), EGameplayModOp::Additive, 50.f);
	TestEqual(TEXT("current Health is capped at MaxHealth"), F.Health(), 100.f);
	TestEqual(TEXT("base Health is capped at MaxHealth too"), F.HealthBase(), 100.f);

	F.ApplyDamage(-20.f);
	TestEqual(TEXT("the next hit is fully visible: 80, not 100"), F.Health(), 80.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARGasMaxHealthReconcileTest, "ActionRoguelike.GAS.Integration.LoweringMaxHealthPullsHealthDown", AR_TEST_FLAGS)
bool FARGasMaxHealthReconcileTest::RunTest(const FString& Parameters)
{
	// Review finding: GAS does not reconcile separate attributes, so Health could stay above a lowered MaxHealth.
	FARGasFixture F;
	if (!F.Init(*this)) { return false; }

	TestEqual(TEXT("Health starts at 100"), F.Health(), 100.f);
	F.ApplyInstant(UARAttributeSet::GetMaxHealthAttribute(), EGameplayModOp::Override, 50.f);
	TestEqual(TEXT("MaxHealth is now 50"), F.MaxHealth(), 50.f);
	TestEqual(TEXT("Health follows it down to 50"), F.Health(), 50.f);

	F.ApplyInstant(UARAttributeSet::GetMaxHealthAttribute(), EGameplayModOp::Override, 120.f);
	TestEqual(TEXT("raising MaxHealth does not heal"), F.Health(), 50.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
