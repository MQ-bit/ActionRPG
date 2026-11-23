// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Character/ARPlayerCharacter.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

#define AR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARPlayerWalkTest, "ActionRoguelike.Player.Walk", AR_TEST_FLAGS)
bool FARPlayerWalkTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game) || !WorldWrapper.BeginPlayInTestWorld())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AARPlayerCharacter* Player = WorldWrapper.GetTestWorld()->SpawnActor<AARPlayerCharacter>(FVector(0.f, 0.f, 96.f), FRotator::ZeroRotator, Params);
	if (!TestNotNull(TEXT("player spawned"), Player))
	{
		return false;
	}

	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	const float JogSpeed = Movement->MaxWalkSpeed;
	TestFalse(TEXT("jogs by default"), Player->IsWalking());

	Player->SetWalking(true);
	TestTrue(TEXT("walking"), Player->IsWalking());
	TestEqual(TEXT("walk speed while walking"), Movement->MaxWalkSpeed, 300.f);

	Player->SetWalking(false);
	TestFalse(TEXT("not walking after the release"), Player->IsWalking());
	TestEqual(TEXT("the jog speed is back after the release"), Movement->MaxWalkSpeed, JogSpeed);

	// A second press and release must not lose the jog speed (it is read once, not from the walking value).
	Player->SetWalking(true);
	Player->SetWalking(true);
	Player->SetWalking(false);
	TestEqual(TEXT("still the jog speed after repeated presses"), Movement->MaxWalkSpeed, JogSpeed);

	// The movement input is remembered (the parkour move reads it while the input itself is ignored), and forgotten after a moment.
	TestFalse(TEXT("no input at first"), Player->IsMoveInputHeld());
	Player->ARMoveInput(1.f, 0.f);
	TestTrue(TEXT("input held right after it was given"), Player->IsMoveInputHeld());
	Player->ARMoveInput(0.1f, 0.f);
	TestFalse(TEXT("a tiny input is no input"), Player->IsMoveInputHeld());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
