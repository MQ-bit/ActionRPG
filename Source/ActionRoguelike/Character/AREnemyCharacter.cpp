// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/AREnemyCharacter.h"

#include "AI/AREnemyAIController.h"
#include "GAS/ARAbilitySystemComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GAS/ARGameplayTags.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UI/ARHealthBarWidget.h"
#include "UI/ARLockOnMarkerWidget.h"

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

	LockOnMarkerComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("LockOnMarker"));
	LockOnMarkerComponent->SetupAttachment(RootComponent);
	LockOnMarkerComponent->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	LockOnMarkerComponent->SetWidgetClass(UARLockOnMarkerWidget::StaticClass());
	LockOnMarkerComponent->SetWidgetSpace(EWidgetSpace::Screen);
	LockOnMarkerComponent->SetDrawSize(FVector2D(40.f, 40.f));
	LockOnMarkerComponent->SetVisibility(false);
}

void AAREnemyCharacter::SetLockedOn(bool bLocked)
{
	bLockedOn = bLocked;
	LockOnMarkerComponent->SetVisibility(bLocked);
}

void AAREnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// The widget exists only if the Blueprint set a widget class on the component.
	HealthBarComponent->InitWidget();
	LockOnMarkerComponent->InitWidget();
	if (UARHealthBarWidget* HealthBar = Cast<UARHealthBarWidget>(HealthBarComponent->GetUserWidgetObject()))
	{
		HealthBar->BindToCharacter(this);
	}

	AbilitySystemComponent->RegisterGameplayTagEvent(TAG_State_HitReact.GetTag(), EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &AAREnemyCharacter::OnHitReactTagChanged);
}

void AAREnemyCharacter::OnHitReactTagChanged(const FGameplayTag /*Tag*/, int32 Count)
{
	// The current move is paused, and the behavior tree too: otherwise a cancelled attack lets it start a new MoveTo
	// during the reaction and the enemy slides. Only a pause made here is resumed.
	const AAIController* AI = Cast<AAIController>(GetController());
	UPathFollowingComponent* PathFollowing = AI ? AI->GetPathFollowingComponent() : nullptr;
	UBrainComponent* Brain = AI ? AI->GetBrainComponent() : nullptr;
	if (Count > 0 && !bPausedForHitReact)
	{
		bPausedForHitReact = true;
		if (PathFollowing)
		{
			PathFollowing->PauseMove();
		}
		if (Brain)
		{
			Brain->PauseLogic(TEXT("HitReact"));
		}
	}
	else if (Count == 0 && bPausedForHitReact)
	{
		bPausedForHitReact = false;
		if (Brain)
		{
			Brain->ResumeLogic(TEXT("HitReact"));
		}
		if (PathFollowing)
		{
			PathFollowing->ResumeMove();
		}
	}
}

void AAREnemyCharacter::OnDeath()
{
	HealthBarComponent->SetVisibility(false);
	SetLockedOn(false);

	if (DeathLifeSpan > 0.f)
	{
		SetLifeSpan(DeathLifeSpan);
	}
}
