// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/AREnemyCharacter.h"

#include "AI/AREnemyAIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UI/ARHealthBarWidget.h"

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

	HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBarComponent->SetupAttachment(RootComponent);
	HealthBarComponent->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	HealthBarComponent->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarComponent->SetDrawSize(FVector2D(100.f, 10.f));
}

void AAREnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// The widget exists only if the Blueprint set a widget class on the component.
	HealthBarComponent->InitWidget();
	if (UARHealthBarWidget* HealthBar = Cast<UARHealthBarWidget>(HealthBarComponent->GetUserWidgetObject()))
	{
		HealthBar->BindToCharacter(this);
	}
}

void AAREnemyCharacter::OnDeath()
{
	HealthBarComponent->SetVisibility(false);

	if (DeathLifeSpan > 0.f)
	{
		SetLifeSpan(DeathLifeSpan);
	}
}
