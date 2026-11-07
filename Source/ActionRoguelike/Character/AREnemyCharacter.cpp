// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/AREnemyCharacter.h"

#include "AI/AREnemyAIController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AAREnemyCharacter::AAREnemyCharacter()
{
	TeamId = ARTeam::Enemy;

	AIControllerClass = AAREnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// The pawn follows the controller's desired rotation (the move direction while walking, the focus target while
	// standing). Orient-to-movement would never turn a standing pawn, so BTTask_RotateToFaceBBEntry would wait forever.
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 360.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 300.f;
}

void AAREnemyCharacter::OnDeath()
{
	if (DeathLifeSpan > 0.f)
	{
		SetLifeSpan(DeathLifeSpan);
	}
}
