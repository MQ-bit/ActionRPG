// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/ARPlayerCharacter.h"

#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GAS/ARAbilitySystemComponent.h"
#include "GAS/ARGameplayTags.h"
#include "InputActionValue.h"
#include "UI/ARHealthBarWidget.h"

AARPlayerCharacter::AARPlayerCharacter()
{
	TeamId = ARTeam::Player;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// The camera drives yaw; the mesh turns toward the movement direction instead.
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void AARPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// shortcut: the pawn creates its HUD; move this to the player controller if a respawn keeps the controller.
	if (HealthBarWidgetClass && PC->IsLocalController())
	{
		if (UARHealthBarWidget* HealthBar = CreateWidget<UARHealthBarWidget>(PC, HealthBarWidgetClass))
		{
			HealthBar->BindToCharacter(this);
			HealthBar->AddToViewport();
		}
	}

	if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
	{
		if (auto* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void AARPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (auto* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AARPlayerCharacter::Move);
		}
		if (LookAction)
		{
			EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AARPlayerCharacter::Look);
		}
		if (AttackAction)
		{
			EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &AARPlayerCharacter::OnAttackInput);
		}
		if (JumpAction)
		{
			EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &AARPlayerCharacter::OnJumpInput);
			EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}
	}
}

void AARPlayerCharacter::Move(const FInputActionValue& Value)
{
	// The attack montage animates the whole body, so moving during it looks like sliding.
	if (AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_Attacking.GetTag())
		|| AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_Parkouring.GetTag()))
	{
		return;
	}

	const FVector2D Input = Value.Get<FVector2D>();
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);

	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Input.X);
}

void AARPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();

	AddControllerYawInput(Input.X);
	AddControllerPitchInput(Input.Y);
}

void AARPlayerCharacter::OnAttackInput()
{
	AbilitySystemComponent->AbilityInputTagPressed(TAG_InputTag_Attack.GetTag());
}

void AARPlayerCharacter::OnJumpInput()
{
	// The attack montage animates the whole body; a jump would cut it. A running vault or mantle is not interrupted either.
	if (AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_Attacking.GetTag())
		|| AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_Parkouring.GetTag()))
	{
		return;
	}

	// An obstacle in reach means parkour; otherwise it is a plain jump.
	if (!AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(TAG_Ability_Parkour.GetTag())))
	{
		Jump();
	}
}

void AARPlayerCharacter::ARAttack()
{
	OnAttackInput();
}
