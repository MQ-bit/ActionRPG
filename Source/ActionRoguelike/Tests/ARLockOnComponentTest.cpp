// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "AbilitySystemComponent.h"
#include "Character/AREnemyCharacter.h"
#include "Character/ARLockOnComponent.h"
#include "Character/ARPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "GameplayEffect.h"
#include "GAS/ARAbilitySystemComponent.h"
#include "GAS/ARGameplayTags.h"
#include "GAS/ARGE_Damage.h"

#define AR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

namespace
{
	/**
	 * A floor, a player (facing +X) and enemies placed on request. The player is possessed by an AI controller: in a headless world a
	 * player controller has no local player, so the movement component would treat the player as a remote client and not move it.
	 */
	struct FARLockOnFixture
	{
		FTestWorldWrapper WorldWrapper;
		AARPlayerCharacter* Player = nullptr;
		AAIController* Controller = nullptr;
		UARLockOnComponent* LockOn = nullptr;
		UAbilitySystemComponent* ASC = nullptr;

		void AddBox(const FVector& Center, const FVector& Extent)
		{
			AActor* Box = WorldWrapper.GetTestWorld()->SpawnActor<AActor>(Center, FRotator::ZeroRotator);
			UBoxComponent* Component = NewObject<UBoxComponent>(Box);
			Component->SetBoxExtent(Extent);
			Component->SetCollisionProfileName(TEXT("BlockAll"));
			Box->SetRootComponent(Component);
			Component->RegisterComponent();
			Box->SetActorLocation(Center);
		}

		AAREnemyCharacter* AddEnemy(const FVector& Location)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AAREnemyCharacter* Enemy = WorldWrapper.GetTestWorld()->SpawnActor<AAREnemyCharacter>(Location + FVector(0.f, 0.f, 96.f), FRotator::ZeroRotator, Params);
			if (Enemy)
			{
				Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			}
			return Enemy;
		}

		bool Init(FAutomationTestBase& Test)
		{
			if (!WorldWrapper.CreateTestWorld(EWorldType::Game) || !WorldWrapper.BeginPlayInTestWorld())
			{
				WorldWrapper.ForwardErrorMessages(&Test);
				return false;
			}

			AddBox(FVector(0.f, 0.f, -50.f), FVector(5000.f, 5000.f, 50.f));

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = WorldWrapper.GetTestWorld()->SpawnActor<AARPlayerCharacter>(FVector(0.f, 0.f, 96.f), FRotator::ZeroRotator, Params);
			Controller = WorldWrapper.GetTestWorld()->SpawnActor<AAIController>();
			if (!Test.TestNotNull(TEXT("player spawned"), Player) || !Test.TestNotNull(TEXT("controller"), Controller))
			{
				return false;
			}
			// An AI controller would copy the pawn's rotation into the control rotation every tick.
			Controller->bSetControlRotationFromPawnOrientation = false;
			Controller->Possess(Player);
			Controller->SetControlRotation(FRotator::ZeroRotator);
			Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

			LockOn = Player->FindComponentByClass<UARLockOnComponent>();
			ASC = Player->GetAbilitySystemComponent();
			return Test.TestNotNull(TEXT("player has a lock-on component and an ASC"), LockOn) && Test.TestNotNull(TEXT("ASC"), ASC);
		}

		bool Tagged() const { return ASC->HasMatchingGameplayTag(TAG_State_LockedOn.GetTag()); }

		void Tick(int32 Count, float Dt = 0.05f)
		{
			for (int32 i = 0; i < Count; ++i)
			{
				WorldWrapper.TickTestWorld(Dt);
			}
		}

		void Kill(AAREnemyCharacter* Enemy)
		{
			UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
			FGameplayEffectSpecHandle Spec = EnemyASC->MakeOutgoingSpec(UARGE_Damage::StaticClass(), 1.f, EnemyASC->MakeEffectContext());
			Spec.Data->SetSetByCallerMagnitude(TAG_Data_Damage.GetTag(), -1000.f);
			EnemyASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARLockOnToggleTest, "ActionRoguelike.LockOn.Component.ToggleLocksAndRestores", AR_TEST_FLAGS)
bool FARLockOnToggleTest::RunTest(const FString& Parameters)
{
	FARLockOnFixture F;
	if (!F.Init(*this)) { return false; }
	AAREnemyCharacter* Near = F.AddEnemy(FVector(400.f, 0.f, 0.f));
	AAREnemyCharacter* Far = F.AddEnemy(FVector(900.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("enemies"), Near) || !TestNotNull(TEXT("far enemy"), Far)) { return false; }

	const bool bOrient = F.Player->GetCharacterMovement()->bOrientRotationToMovement;
	const bool bDesired = F.Player->GetCharacterMovement()->bUseControllerDesiredRotation;

	F.LockOn->Toggle();
	TestTrue(TEXT("locked"), F.LockOn->IsLockedOn());
	TestTrue(TEXT("the nearer enemy is the target"), F.LockOn->GetTarget() == Near);
	TestTrue(TEXT("State.LockedOn is set"), F.Tagged());
	TestTrue(TEXT("the target shows its marker"), Near->IsLockedOn());
	TestFalse(TEXT("the other enemy does not"), Far->IsLockedOn());
	TestFalse(TEXT("strafe: no orient to movement"), F.Player->GetCharacterMovement()->bOrientRotationToMovement);
	TestTrue(TEXT("strafe: follows the controller"), F.Player->GetCharacterMovement()->bUseControllerDesiredRotation);

	F.LockOn->Toggle();
	TestFalse(TEXT("released"), F.LockOn->IsLockedOn());
	TestFalse(TEXT("tag removed"), F.Tagged());
	TestFalse(TEXT("marker hidden"), Near->IsLockedOn());
	TestEqual(TEXT("orient setting restored"), F.Player->GetCharacterMovement()->bOrientRotationToMovement, bOrient);
	TestEqual(TEXT("controller rotation setting restored"), F.Player->GetCharacterMovement()->bUseControllerDesiredRotation, bDesired);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARLockOnReleasePathsTest, "ActionRoguelike.LockOn.Component.ReleasePaths", AR_TEST_FLAGS)
bool FARLockOnReleasePathsTest::RunTest(const FString& Parameters)
{
	{
		FARLockOnFixture F;
		if (!F.Init(*this)) { return false; }
		AAREnemyCharacter* Enemy = F.AddEnemy(FVector(400.f, 0.f, 0.f));
		F.LockOn->Toggle();
		F.Kill(Enemy);
		TestTrue(TEXT("the enemy is dead"), Enemy->IsDead());
		TestFalse(TEXT("the target died: released at once"), F.LockOn->IsLockedOn());
		TestFalse(TEXT("tag removed after death"), F.Tagged());
	}
	{
		FARLockOnFixture F;
		if (!F.Init(*this)) { return false; }
		AAREnemyCharacter* Enemy = F.AddEnemy(FVector(400.f, 0.f, 0.f));
		F.LockOn->Toggle();
		Enemy->Destroy();
		F.Tick(2);
		TestFalse(TEXT("the target was destroyed: released"), F.LockOn->IsLockedOn());
		TestFalse(TEXT("tag removed after destroy"), F.Tagged());
	}
	{
		FARLockOnFixture F;
		if (!F.Init(*this)) { return false; }
		AAREnemyCharacter* Enemy = F.AddEnemy(FVector(400.f, 0.f, 0.f));
		F.LockOn->Toggle();
		Enemy->SetActorLocation(FVector(1500.f, 0.f, 96.f));
		F.Tick(2);
		TestFalse(TEXT("farther than 1.2x the range: released"), F.LockOn->IsLockedOn());
	}
	{
		FARLockOnFixture F;
		if (!F.Init(*this)) { return false; }
		F.AddEnemy(FVector(400.f, 0.f, 0.f));
		F.LockOn->Toggle();
		F.ASC->AddLooseGameplayTag(TAG_State_Parkouring.GetTag());
		F.Tick(2);
		TestFalse(TEXT("a parkour move starts: the lock-on steps aside"), F.LockOn->IsLockedOn());
		F.LockOn->Toggle();
		TestFalse(TEXT("no new lock while parkouring"), F.LockOn->IsLockedOn());
		F.ASC->RemoveLooseGameplayTag(TAG_State_Parkouring.GetTag());
	}
	{
		FARLockOnFixture F;
		if (!F.Init(*this)) { return false; }
		F.AddEnemy(FVector(400.f, 0.f, 0.f));
		F.LockOn->Toggle();
		F.LockOn->Release();
		F.LockOn->Release();
		TestFalse(TEXT("releasing twice is harmless"), F.LockOn->IsLockedOn());
		TestFalse(TEXT("tag stays removed"), F.Tagged());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARLockOnOcclusionTest, "ActionRoguelike.LockOn.Component.Occlusion", AR_TEST_FLAGS)
bool FARLockOnOcclusionTest::RunTest(const FString& Parameters)
{
	// A hidden nearer enemy is not picked; a visible one beside it is.
	{
		FARLockOnFixture F;
		if (!F.Init(*this)) { return false; }
		F.AddEnemy(FVector(400.f, 0.f, 0.f));
		AAREnemyCharacter* Visible = F.AddEnemy(FVector(400.f, 300.f, 0.f));
		F.AddBox(FVector(200.f, 0.f, 150.f), FVector(10.f, 100.f, 150.f));
		F.LockOn->Toggle();
		TestTrue(TEXT("locked on the visible enemy"), F.LockOn->IsLockedOn() && F.LockOn->GetTarget() == Visible);
	}
	// Only a hidden enemy: nothing to lock.
	{
		FARLockOnFixture F;
		if (!F.Init(*this)) { return false; }
		F.AddEnemy(FVector(400.f, 0.f, 0.f));
		F.AddBox(FVector(200.f, 0.f, 150.f), FVector(10.f, 100.f, 150.f));
		F.LockOn->Toggle();
		TestFalse(TEXT("a hidden enemy is not locked"), F.LockOn->IsLockedOn());
	}
	// Held target goes out of sight: kept for the grace time, then released.
	{
		FARLockOnFixture F;
		if (!F.Init(*this)) { return false; }
		F.AddEnemy(FVector(400.f, 0.f, 0.f));
		F.LockOn->Toggle();
		F.AddBox(FVector(200.f, 0.f, 150.f), FVector(10.f, 100.f, 150.f));
		F.Tick(10);
		TestTrue(TEXT("still locked after 0.5 s out of sight"), F.LockOn->IsLockedOn());
		F.Tick(12);
		TestFalse(TEXT("released after more than 1 s out of sight"), F.LockOn->IsLockedOn());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARLockOnRotationTest, "ActionRoguelike.LockOn.Component.TurnsControllerToTarget", AR_TEST_FLAGS)
bool FARLockOnRotationTest::RunTest(const FString& Parameters)
{
	FARLockOnFixture F;
	if (!F.Init(*this)) { return false; }
	F.AddEnemy(FVector(400.f, 300.f, 0.f));
	F.LockOn->Toggle();
	if (!TestTrue(TEXT("locked"), F.LockOn->IsLockedOn())) { return false; }

	F.Tick(1);
	const float Early = F.Controller->GetControlRotation().Yaw;
	TestTrue(TEXT("turning gradually, not snapping"), Early > 1.f && Early < 36.f);

	F.Tick(11);
	TestEqual(TEXT("controller faces the target (atan2(300, 400) = 36.9 deg)"), static_cast<double>(F.Controller->GetControlRotation().Yaw), 36.87, 1.5);
	TestEqual(TEXT("the body turns with the controller (strafe)"), static_cast<double>(F.Player->GetActorRotation().Yaw), 36.87, 12.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
