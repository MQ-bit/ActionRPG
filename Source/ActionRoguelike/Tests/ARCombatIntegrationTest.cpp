// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "AbilitySystemComponent.h"
#include "Character/AREnemyCharacter.h"
#include "Character/ARPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GAS/ARAbilitySystemComponent.h"
#include "GAS/ARAttributeSet.h"
#include "GAS/ARGameplayTags.h"
#include "GAS/ARGE_Damage.h"

#define AR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

namespace
{
	/** A test world with one enemy; its ASC and attribute set are initialised by BeginPlay. */
	struct FARCombatFixture
	{
		FTestWorldWrapper WorldWrapper;
		AAREnemyCharacter* Enemy = nullptr;
		UARAbilitySystemComponent* ASC = nullptr;
		UARAttributeSet* Set = nullptr;

		template <typename T>
		T* Spawn()
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return WorldWrapper.GetTestWorld()->SpawnActor<T>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		}

		bool Init(FAutomationTestBase& Test)
		{
			if (!WorldWrapper.CreateTestWorld(EWorldType::Game) || !WorldWrapper.BeginPlayInTestWorld())
			{
				WorldWrapper.ForwardErrorMessages(&Test);
				return false;
			}

			Enemy = Spawn<AAREnemyCharacter>();
			ASC = Enemy ? Enemy->FindComponentByClass<UARAbilitySystemComponent>() : nullptr;
			Set = ASC ? const_cast<UARAttributeSet*>(ASC->GetSet<UARAttributeSet>()) : nullptr;
			return Test.TestNotNull(TEXT("enemy spawned with an ASC and attribute set"), Set);
		}

		void ApplyDamage(float Amount)
		{
			FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UARGE_Damage::StaticClass(), 1.f, ASC->MakeEffectContext());
			Spec.Data->SetSetByCallerMagnitude(TAG_Data_Damage.GetTag(), Amount);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARHealthDelegateTest, "ActionRoguelike.Combat.HealthDelegateReportsChange", AR_TEST_FLAGS)
bool FARHealthDelegateTest::RunTest(const FString& Parameters)
{
	FARCombatFixture F;
	if (!F.Init(*this)) { return false; }

	int32 Calls = 0;
	float LastOld = -1.f, LastNew = -1.f, LastMax = -1.f;
	const FDelegateHandle Handle = F.Set->OnHealthChanged.AddLambda([&](float Old, float New, float Max)
	{
		++Calls; LastOld = Old; LastNew = New; LastMax = Max;
	});

	F.ApplyDamage(-30.f);
	TestEqual(TEXT("one broadcast per Health change"), Calls, 1);
	TestEqual(TEXT("old value"), LastOld, 100.f);
	TestEqual(TEXT("new value"), LastNew, 70.f);
	TestEqual(TEXT("max health"), LastMax, 100.f);

	F.Set->OnHealthChanged.Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARDeathTest, "ActionRoguelike.Combat.ZeroHealthKillsOnce", AR_TEST_FLAGS)
bool FARDeathTest::RunTest(const FString& Parameters)
{
	FARCombatFixture F;
	if (!F.Init(*this)) { return false; }

	int32 DiedCount = 0;
	const FDelegateHandle Handle = F.Enemy->OnDied.AddLambda([&](AARCharacterBase*) { ++DiedCount; });

	F.ApplyDamage(-60.f);
	TestFalse(TEXT("alive at 40 health"), F.Enemy->IsDead());
	TestEqual(TEXT("no death yet"), DiedCount, 0);

	F.ApplyDamage(-100.f);
	TestTrue(TEXT("dead at 0 health"), F.Enemy->IsDead());
	TestTrue(TEXT("State.Dead tag is set"), F.ASC->HasMatchingGameplayTag(TAG_State_Dead.GetTag()));
	TestEqual(TEXT("died broadcast once"), DiedCount, 1);
	TestTrue(TEXT("collision is off"), F.Enemy->GetCapsuleComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);

	F.ApplyDamage(-10.f);
	TestEqual(TEXT("hitting a corpse does not kill it again"), DiedCount, 1);

	F.Enemy->OnDied.Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARTeamAttitudeTest, "ActionRoguelike.Combat.TeamAttitudeBetweenCharacters", AR_TEST_FLAGS)
bool FARTeamAttitudeTest::RunTest(const FString& Parameters)
{
	FARCombatFixture F;
	if (!F.Init(*this)) { return false; }

	AAREnemyCharacter* OtherEnemy = F.Spawn<AAREnemyCharacter>();
	AARPlayerCharacter* Player = F.Spawn<AARPlayerCharacter>();
	if (!TestNotNull(TEXT("second enemy"), OtherEnemy) || !TestNotNull(TEXT("player"), Player)) { return false; }

	TestTrue(TEXT("enemy sees the player as hostile"), F.Enemy->GetTeamAttitudeTowards(*Player) == ETeamAttitude::Hostile);
	TestTrue(TEXT("player sees the enemy as hostile"), Player->GetTeamAttitudeTowards(*F.Enemy) == ETeamAttitude::Hostile);
	TestTrue(TEXT("enemies are friendly to each other"), F.Enemy->GetTeamAttitudeTowards(*OtherEnemy) == ETeamAttitude::Friendly);
	TestEqual(TEXT("enemy team id"), (int32)F.Enemy->GetGenericTeamId().GetId(), (int32)ARTeam::Enemy);
	TestEqual(TEXT("player team id"), (int32)Player->GetGenericTeamId().GetId(), (int32)ARTeam::Player);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
