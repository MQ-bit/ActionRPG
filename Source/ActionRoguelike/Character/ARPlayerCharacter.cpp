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
#include "GAS/ARGA_Parkour.h"
#include "GAS/ARGameplayTags.h"
#include "InputActionValue.h"
#include "MotionWarpingComponent.h"
#include "Character/AREquipmentComponent.h"
#include "Character/ARLockOnComponent.h"
#include "Character/ARParkourIKComponent.h"
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

	MotionWarping = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));
	ParkourIK = CreateDefaultSubobject<UARParkourIKComponent>(TEXT("ParkourIK"));
	LockOn = CreateDefaultSubobject<UARLockOnComponent>(TEXT("LockOn"));
	Equipment = CreateDefaultSubobject<UAREquipmentComponent>(TEXT("Equipment"));
}

void AARPlayerCharacter::SetWalking(bool bWalk)
{
	bWalking = bWalk;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (RunSpeed <= 0.f)
		{
			RunSpeed = Movement->MaxWalkSpeed;
		}
		Movement->MaxWalkSpeed = bWalk ? WalkSpeed : RunSpeed;
	}
}

void AARPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (ParkourIKAnimClass)
	{
		GetMesh()->SetOverridePostProcessAnimBP(ParkourIKAnimClass);
	}

	for (const FName Bone : HiddenBones)
	{
		GetMesh()->HideBoneByName(Bone, PBO_None);
	}

	AbilitySystemComponent->RegisterGameplayTagEvent(TAG_State_Parkouring.GetTag(), EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AARPlayerCharacter::OnActionTagChanged);
	AbilitySystemComponent->RegisterGameplayTagEvent(TAG_State_Attacking.GetTag(), EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AARPlayerCharacter::OnActionTagChanged);

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
		if (LockOnAction)
		{
			EIC->BindAction(LockOnAction, ETriggerEvent::Started, this, &AARPlayerCharacter::OnLockOnInput);
		}
		if (ToggleWeaponAction)
		{
			EIC->BindAction(ToggleWeaponAction, ETriggerEvent::Started, this, &AARPlayerCharacter::OnToggleWeaponInput);
		}
		if (BlockAction)
		{
			EIC->BindAction(BlockAction, ETriggerEvent::Started, this, &AARPlayerCharacter::OnBlockPressed);
			EIC->BindAction(BlockAction, ETriggerEvent::Completed, this, &AARPlayerCharacter::OnBlockReleased);
		}
		if (WalkAction)
		{
			EIC->BindAction(WalkAction, ETriggerEvent::Started, this, &AARPlayerCharacter::SetWalking, true);
			EIC->BindAction(WalkAction, ETriggerEvent::Completed, this, &AARPlayerCharacter::SetWalking, false);
		}
		if (JumpAction)
		{
			EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &AARPlayerCharacter::OnJumpInput);
			EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}
	}
}

void AARPlayerCharacter::ARMoveInput(float X, float Y)
{
	LastMoveInput = FVector2D(X, Y);
	LastMoveInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

bool AARPlayerCharacter::IsMoveInputHeld() const
{
	return GetWorld() && LastMoveInput.SizeSquared() > 0.04 && GetWorld()->GetTimeSeconds() - LastMoveInputTime < 0.15;
}

void AARPlayerCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Raw = Value.Get<FVector2D>();
	ARMoveInput(Raw.X, Raw.Y);

	// The attack and hit montages animate the whole body, so moving during them looks like sliding (blocking may walk).
	if (AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_Attacking.GetTag())
		|| AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_Parkouring.GetTag())
		|| AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_HitReact.GetTag()))
	{
		return;
	}

	const FVector2D Input = Value.Get<FVector2D>();
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);

	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Input.X);
}

void AARPlayerCharacter::OnActionTagChanged(const FGameplayTag /*Tag*/, int32 /*Count*/)
{
	const bool bHide = AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_Parkouring.GetTag())
		|| AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_Attacking.GetTag());
	for (const FName Bone : ActionHiddenBones)
	{
		if (bHide)
		{
			GetMesh()->HideBoneByName(Bone, PBO_None);
		}
		else
		{
			GetMesh()->UnHideBoneByName(Bone);
		}
	}
}

void AARPlayerCharacter::Look(const FInputActionValue& Value)
{
	// The controller follows the target while locked.
	if (LockOn->IsLockedOn())
	{
		return;
	}

	const FVector2D Input = Value.Get<FVector2D>();

	AddControllerYawInput(Input.X);
	AddControllerPitchInput(Input.Y);
}

void AARPlayerCharacter::OnAttackInput()
{
	AbilitySystemComponent->AbilityInputTagPressed(TAG_InputTag_Attack.GetTag());
}

void AARPlayerCharacter::OnLockOnInput()
{
	LockOn->Toggle();
}

void AARPlayerCharacter::OnJumpInput()
{
	// The attack montage animates the whole body; a jump would cut it. A running vault or mantle, a hit reaction and a
	// block are not interrupted either.
	if (IsBusyWithAction())
	{
		return;
	}

	// An obstacle in reach means parkour; otherwise it is a plain jump.
	if (TryParkour())
	{
		// Parkour turns the body itself, so the lock-on steps aside (the component also lets go when it sees the parkour tag).
		LockOn->Release();
		ClearParkourBuffer();
		return;
	}

	// A running character that pressed early keeps running: the press waits until it is at the distance the clip needs.
	if (ParkourBufferSeconds > 0.f && ShouldBufferParkour())
	{
		ParkourBufferEnd = GetWorld()->GetTimeSeconds() + ParkourBufferSeconds;
		GetWorldTimerManager().SetTimer(ParkourBufferTimer, this, &AARPlayerCharacter::ParkourBufferTick, 1.f / 120.f, true);
		return;
	}
	Jump();
}

bool AARPlayerCharacter::TryParkour()
{
	return AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(TAG_Ability_Parkour.GetTag()));
}

bool AARPlayerCharacter::ShouldBufferParkour() const
{
	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (const UARGA_Parkour* Parkour = Cast<UARGA_Parkour>(Spec.GetPrimaryInstance()))
		{
			return Parkour->ShouldBufferJump();
		}
	}
	return false;
}

void AARPlayerCharacter::ParkourBufferTick()
{
	// Done when the time is up, something else took over, the parkour started, or there is nothing left to wait for (the wall
	// is gone or the character stopped: a standing character is walked up to the wall by the ability itself).
	if (GetWorld()->GetTimeSeconds() > ParkourBufferEnd
		|| AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_Attacking.GetTag())
		|| AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_Parkouring.GetTag())
		|| TryParkour() || !ShouldBufferParkour())
	{
		ClearParkourBuffer();
	}
}

void AARPlayerCharacter::ClearParkourBuffer()
{
	GetWorldTimerManager().ClearTimer(ParkourBufferTimer);
}

void AARPlayerCharacter::ARCancelAbilities()
{
	AbilitySystemComponent->CancelAllAbilities();
}

void AARPlayerCharacter::ARJump()
{
	OnJumpInput();
}

void AARPlayerCharacter::ARLock()
{
	OnLockOnInput();
}

void AARPlayerCharacter::ARAttack()
{
	OnAttackInput();
}

void AARPlayerCharacter::OnToggleWeaponInput()
{
	// The attack read its weapon when it started and the parkour hides the mesh: no swapping in the middle of an action.
	if (IsBusyWithAction())
	{
		return;
	}
	Equipment->ToggleDefaultWeapon();
}

bool AARPlayerCharacter::IsBusyWithAction() const
{
	static const FGameplayTagContainer BusyTags = FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{
		TAG_State_Attacking.GetTag(), TAG_State_Parkouring.GetTag(), TAG_State_HitReact.GetTag(), TAG_State_Blocking.GetTag() });
	return AbilitySystemComponent->HasAnyMatchingGameplayTags(BusyTags);
}

void AARPlayerCharacter::OnBlockPressed()
{
	AbilitySystemComponent->AbilityInputTagPressed(TAG_InputTag_Block.GetTag());
}

void AARPlayerCharacter::OnBlockReleased()
{
	AbilitySystemComponent->AbilityInputTagReleased(TAG_InputTag_Block.GetTag());
}

void AARPlayerCharacter::ARBlock(int32 bHold)
{
	bHold ? OnBlockPressed() : OnBlockReleased();
}

void AARPlayerCharacter::ARToggleWeapon()
{
	OnToggleWeaponInput();
}

void AARPlayerCharacter::ARRepairWeapon(float Amount)
{
	Equipment->Repair(Amount);
}
