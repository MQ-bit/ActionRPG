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

	// The AI controller drives yaw through movement; the mesh turns toward the movement direction.
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
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
