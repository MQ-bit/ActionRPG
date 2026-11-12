// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "AbilitySystemComponent.h"
#include "Character/AREnemyCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/ARAbilitySystemComponent.h"
#include "GAS/ARGA_BasicAttack.h"
#include "GAS/ARGA_Parkour.h"
#include "GAS/ARGameplayTags.h"

#define AR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

namespace
{
	/**
	 * A test world with a floor (top at z 0) and a character facing +X with its feet on the floor. An obstacle box is
	 * placed with its near face 100 cm in front of the character. Parkour is only probed here, the motion is not ticked.
	 */
	struct FARParkourFixture
	{
		FTestWorldWrapper WorldWrapper;
		AAREnemyCharacter* Actor = nullptr;
		UARAbilitySystemComponent* ASC = nullptr;

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

		/** FloorEndX: the floor stops there (x, cm). LowCeilingZ: a slab above the obstacle whose top is at this height (0 = none). */
		bool Init(FAutomationTestBase& Test, float ObstacleHeight, float Thickness, float FloorEndX = 5000.f, float LowCeilingZ = 0.f)
		{
			if (!WorldWrapper.CreateTestWorld(EWorldType::Game) || !WorldWrapper.BeginPlayInTestWorld())
			{
				WorldWrapper.ForwardErrorMessages(&Test);
				return false;
			}

			AddBox(FVector((FloorEndX - 5000.f) * 0.5f, 0.f, -50.f), FVector((FloorEndX + 5000.f) * 0.5f, 5000.f, 50.f));
			if (LowCeilingZ > 0.f)
			{
				// 70 cm thick slab from x 80 to 450, clear of the character at x 0.
				AddBox(FVector(265.f, 0.f, LowCeilingZ - 35.f), FVector(185.f, 300.f, 35.f));
			}
			if (ObstacleHeight > 0.f)
			{
				AddBox(FVector(100.f + Thickness * 0.5f, 0.f, ObstacleHeight * 0.5f), FVector(Thickness * 0.5f, 300.f, ObstacleHeight * 0.5f));
			}

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Actor = WorldWrapper.GetTestWorld()->SpawnActor<AAREnemyCharacter>(FVector(0.f, 0.f, 96.f), FRotator::ZeroRotator, Params);
			ASC = Actor ? Actor->FindComponentByClass<UARAbilitySystemComponent>() : nullptr;
			if (!Test.TestNotNull(TEXT("character spawned with an ASC"), ASC))
			{
				return false;
			}

			Actor->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			ASC->GiveAbility(FGameplayAbilitySpec(UARGA_Parkour::StaticClass(), 1));
			ASC->GiveAbility(FGameplayAbilitySpec(UARGA_BasicAttack::StaticClass(), 1));
			return true;
		}

		bool TryParkour() const
		{
			return ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(TAG_Ability_Parkour.GetTag()));
		}

		bool Parkouring() const { return ASC->HasMatchingGameplayTag(TAG_State_Parkouring.GetTag()); }

		/** True when the capsule overlaps something blocking (the obstacle or the floor). */
		bool CapsuleBlocked() const
		{
			const UCapsuleComponent* Capsule = Actor->GetCapsuleComponent();
			FCollisionQueryParams Params(NAME_None, false, Actor);
			return WorldWrapper.GetTestWorld()->OverlapBlockingTestByChannel(Actor->GetActorLocation(), FQuat::Identity, ECC_Pawn,
				FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Params);
		}

		/** Ticks the world until the ability ends; returns how many ticks the capsule overlapped geometry on the way. */
		int32 TickUntilDone(int32 MaxTicks = 60)
		{
			int32 Overlaps = 0;
			for (int32 i = 0; i < MaxTicks && Parkouring(); ++i)
			{
				WorldWrapper.TickTestWorld(0.05f);
				Overlaps += CapsuleBlocked() ? 1 : 0;
			}
			return Overlaps;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourAbilityVaultTest, "ActionRoguelike.Parkour.Ability.VaultAndMantle", AR_TEST_FLAGS)
bool FARParkourAbilityVaultTest::RunTest(const FString& Parameters)
{
	{
		FARParkourFixture F;
		if (!F.Init(*this, 90.f, 100.f)) { return false; }
		TestTrue(TEXT("a 90 cm wall can be vaulted"), F.TryParkour());
		TestTrue(TEXT("State.Parkouring is on while it runs"), F.Parkouring());
	}
	{
		FARParkourFixture F;
		if (!F.Init(*this, 200.f, 300.f)) { return false; }
		TestTrue(TEXT("a 200 cm ledge can be mantled"), F.TryParkour());
		TestTrue(TEXT("State.Parkouring is on while it runs"), F.Parkouring());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourAbilityRejectTest, "ActionRoguelike.Parkour.Ability.Rejected", AR_TEST_FLAGS)
bool FARParkourAbilityRejectTest::RunTest(const FString& Parameters)
{
	{
		FARParkourFixture F;
		if (!F.Init(*this, 0.f, 0.f)) { return false; }
		TestFalse(TEXT("no obstacle: no parkour"), F.TryParkour());
		TestFalse(TEXT("State.Parkouring stays off"), F.Parkouring());
	}
	{
		FARParkourFixture F;
		if (!F.Init(*this, 300.f, 300.f)) { return false; }
		TestFalse(TEXT("a 300 cm wall is too high"), F.TryParkour());
	}
	{
		FARParkourFixture F;
		if (!F.Init(*this, 90.f, 400.f)) { return false; }
		TestFalse(TEXT("a thick low block is not vaulted"), F.TryParkour());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourAbilityNeighbourTest, "ActionRoguelike.Parkour.Ability.VaultWithObstacleBehind", AR_TEST_FLAGS)
bool FARParkourAbilityNeighbourTest::RunTest(const FString& Parameters)
{
	// Found in PIE: the back-face trace started 400 cm behind the wall, inside the next obstacle, and every wall
	// with another obstacle within 400 cm behind it was treated as thick. Wall x 100..160, ledge face 150 cm behind it.
	FARParkourFixture F;
	if (!F.Init(*this, 90.f, 60.f)) { return false; }
	F.AddBox(FVector(460.f, 0.f, 100.f), FVector(150.f, 300.f, 100.f));
	TestTrue(TEXT("a 60 cm wall with a ledge 150 cm behind it can be vaulted"), F.TryParkour());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourAbilityGapTest, "ActionRoguelike.Parkour.Ability.NoVaultOverGap", AR_TEST_FLAGS)
bool FARParkourAbilityGapTest::RunTest(const FString& Parameters)
{
	// Codex review: the floor was checked just past the obstacle edge, but the capsule lands one radius further.
	// Obstacle spans x 100..200; the floor ends at x 210, so the sampled point (205) has floor and the landing (247) does not.
	FARParkourFixture F;
	if (!F.Init(*this, 90.f, 100.f, 210.f)) { return false; }
	TestFalse(TEXT("no vault onto air"), F.TryParkour());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourAbilityCeilingTest, "ActionRoguelike.Parkour.Ability.CeilingIsNotATop", AR_TEST_FLAGS)
bool FARParkourAbilityCeilingTest::RunTest(const FString& Parameters)
{
	// Codex review: a 90 cm wall under a slab whose upper surface is at 220 cm was mantled onto the slab.
	FARParkourFixture F;
	if (!F.Init(*this, 90.f, 100.f, 5000.f, 220.f)) { return false; }
	TestFalse(TEXT("the slab above is not the obstacle top"), F.TryParkour());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourAbilityShapeTest, "ActionRoguelike.Parkour.Ability.ThicknessAndWidth", AR_TEST_FLAGS)
bool FARParkourAbilityShapeTest::RunTest(const FString& Parameters)
{
	// Codex review: thickness was measured in 25 cm steps, so a 154 cm wall passed the 150 cm limit.
	{
		FARParkourFixture F;
		if (!F.Init(*this, 90.f, 148.f)) { return false; }
		TestTrue(TEXT("148 cm thick still vaults"), F.TryParkour());
	}
	{
		FARParkourFixture F;
		if (!F.Init(*this, 90.f, 154.f)) { return false; }
		TestFalse(TEXT("154 cm thick does not vault"), F.TryParkour());
	}
	// Codex review: a 200 cm pillar only 30 cm deep was mantled; the capsule centre would stand beyond its top.
	{
		FARParkourFixture F;
		if (!F.Init(*this, 200.f, 30.f)) { return false; }
		TestFalse(TEXT("a narrow pillar cannot be mantled"), F.TryParkour());
	}
	{
		FARParkourFixture F;
		if (!F.Init(*this, 90.f, 30.f)) { return false; }
		TestTrue(TEXT("a low narrow pillar can still be vaulted"), F.TryParkour());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourAbilityMotionTest, "ActionRoguelike.Parkour.Ability.MotionReachesTarget", AR_TEST_FLAGS)
bool FARParkourAbilityMotionTest::RunTest(const FString& Parameters)
{
	{
		FARParkourFixture F;
		if (!F.Init(*this, 90.f, 100.f)) { return false; }
		TestTrue(TEXT("vault starts"), F.TryParkour());
		const int32 Overlaps = F.TickUntilDone();
		TestEqual(TEXT("vault: the capsule never overlaps geometry on the way"), Overlaps, 0);
		TestFalse(TEXT("vault: State.Parkouring is removed at the end"), F.Parkouring());
		TestEqual(TEXT("vault: ends 47 cm behind the back face"), F.Actor->GetActorLocation().X, 247.0, 5.0);
		TestEqual(TEXT("vault: ends standing on the floor"), F.Actor->GetActorLocation().Z, 97.0, 5.0);
		TestTrue(TEXT("vault: back in walking mode"), F.Actor->GetCharacterMovement()->MovementMode == MOVE_Walking);
	}
	{
		FARParkourFixture F;
		if (!F.Init(*this, 200.f, 300.f)) { return false; }
		TestTrue(TEXT("mantle starts"), F.TryParkour());
		const int32 Overlaps = F.TickUntilDone();
		TestEqual(TEXT("mantle: the capsule never overlaps geometry on the way"), Overlaps, 0);
		TestFalse(TEXT("mantle: State.Parkouring is removed at the end"), F.Parkouring());
		TestEqual(TEXT("mantle: ends on the ledge"), F.Actor->GetActorLocation().X, 152.0, 5.0);
		TestEqual(TEXT("mantle: ends standing on top"), F.Actor->GetActorLocation().Z, 297.0, 5.0);
		TestTrue(TEXT("mantle: back in walking mode"), F.Actor->GetCharacterMovement()->MovementMode == MOVE_Walking);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourAbilityHitchTest, "ActionRoguelike.Parkour.Ability.LongFrameDoesNotCutCorners", AR_TEST_FLAGS)
bool FARParkourAbilityHitchTest::RunTest(const FString& Parameters)
{
	// Codex review: a frame that spans a path corner swept the straight chord across it. At 0.2 s per frame the lift
	// ends inside the first frame; the chord from the start to the first position would run into the wall (the world
	// clamps longer frames, so 0.2 s is the biggest hitch a test can produce).
	FARParkourFixture F;
	if (!F.Init(*this, 90.f, 100.f)) { return false; }
	TestTrue(TEXT("vault starts"), F.TryParkour());
	for (int32 i = 0; i < 10 && F.Parkouring(); ++i)
	{
		F.WorldWrapper.TickTestWorld(0.2f);
	}
	TestFalse(TEXT("the vault completed instead of being blocked and cancelled"), F.Parkouring());
	TestEqual(TEXT("it arrives behind the wall"), F.Actor->GetActorLocation().X, 247.0, 5.0);
	TestTrue(TEXT("walking again"), F.Actor->GetCharacterMovement()->MovementMode == MOVE_Walking);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourAbilityCancelTest, "ActionRoguelike.Parkour.Ability.CancelMidMove", AR_TEST_FLAGS)
bool FARParkourAbilityCancelTest::RunTest(const FString& Parameters)
{
	FARParkourFixture F;
	if (!F.Init(*this, 90.f, 100.f)) { return false; }
	TestTrue(TEXT("vault starts"), F.TryParkour());
	for (int32 i = 0; i < 4; ++i)
	{
		F.WorldWrapper.TickTestWorld(0.05f);
	}
	TestTrue(TEXT("still moving after 0.2 s"), F.Parkouring());

	F.ASC->CancelAbilities();
	TestFalse(TEXT("State.Parkouring is removed by a cancel"), F.Parkouring());
	TestTrue(TEXT("a cancelled move leaves the character falling, not stuck in custom mode"),
		F.Actor->GetCharacterMovement()->MovementMode == MOVE_Falling);
	TestFalse(TEXT("and not inside geometry"), F.CapsuleBlocked());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARParkourAbilityBlockingTest, "ActionRoguelike.Parkour.Ability.BlocksAttack", AR_TEST_FLAGS)
bool FARParkourAbilityBlockingTest::RunTest(const FString& Parameters)
{
	{
		FARParkourFixture F;
		if (!F.Init(*this, 90.f, 100.f)) { return false; }
		F.ASC->AddLooseGameplayTag(TAG_State_Attacking.GetTag());
		TestFalse(TEXT("no parkour while attacking"), F.TryParkour());
	}
	{
		FARParkourFixture F;
		if (!F.Init(*this, 90.f, 100.f)) { return false; }
		TestTrue(TEXT("parkour starts"), F.TryParkour());
		TestFalse(TEXT("no attack while parkouring"), F.ASC->TryActivateAbilityByClass(UARGA_BasicAttack::StaticClass()));
	}
	{
		FARParkourFixture F;
		if (!F.Init(*this, 0.f, 0.f)) { return false; }
		TestTrue(TEXT("attack works when not parkouring (control case)"), F.ASC->TryActivateAbilityByClass(UARGA_BasicAttack::StaticClass()));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
