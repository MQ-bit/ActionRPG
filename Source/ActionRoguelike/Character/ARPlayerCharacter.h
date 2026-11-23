// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/ARCharacterBase.h"
#include "ARPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UInputMappingContext;
class UAnimInstance;
class UARParkourIKComponent;
class UARLockOnComponent;
class UMotionWarpingComponent;
class UARHealthBarWidget;
struct FInputActionValue;
struct FGameplayTag;

/** Third-person player: camera boom, Enhanced Input move/look. Assets are assigned in a data-only Blueprint. */
UCLASS()
class ACTIONROGUELIKE_API AARPlayerCharacter : public AARCharacterBase
{
	GENERATED_BODY()

public:
	AARPlayerCharacter();

	/** Remembers a movement input (the key or stick state) with its time; Move calls it even when the input is not applied. Also an exec command for tests. */
	UFUNCTION(Exec)
	void ARMoveInput(float X, float Y);

	/** A movement input was given in the last 0.15 s: the player wants to go somewhere (also while the input is ignored for a parkour move). */
	bool IsMoveInputHeld() const;

	/** Walk (WalkSpeed) while true, the movement component's own speed (the jog) otherwise. Held with WalkAction. */
	void SetWalking(bool bWalk);
	bool IsWalking() const { return bWalking; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	/** Forwards the attack input to the ASC by tag; the ability itself lives in UARGA_BasicAttack. */
	void OnAttackInput();

	/** Press: parkour if an obstacle is in reach, else a jump. Release stops the jump. Ignored while attacking or vaulting. */
	void OnJumpInput();

	/** Toggles the lock-on. */
	void OnLockOnInput();

	/** Tries the parkour ability. */
	bool TryParkour();

	FVector2D LastMoveInput = FVector2D::ZeroVector;
	double LastMoveInputTime = -1000.0;

	/** The speed the movement component had when play began (the jog), restored when the walk input is released. */
	float RunSpeed = 0.f;
	bool bWalking = false;

	/** A running character pressed jump with a wall ahead that is still too far for the parkour clip. */
	bool ShouldBufferParkour() const;

	/** Retries the parkour ability every frame while the buffered press is valid. */
	void ParkourBufferTick();
	void ClearParkourBuffer();

	FTimerHandle ParkourBufferTimer;
	double ParkourBufferEnd = 0.0;

	/** Console command `ARCancelAbilities`: cancels every running ability, for testing cancel paths (parkour) without a death. */
	UFUNCTION(Exec)
	void ARCancelAbilities();

	/** Console command `ARJump`: same path as the jump input, for testing without a keyboard. */
	UFUNCTION(Exec)
	void ARJump();

	/** Console command `ARLock`: same path as the lock-on input, for testing without a mouse. */
	UFUNCTION(Exec)
	void ARLock();

	/** Console command `ARAttack`: same path as the attack input, for testing without a mouse. */
	UFUNCTION(Exec)
	void ARAttack();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Lets parkour montages with root motion land exactly on the obstacle (target set by UARGA_Parkour). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parkour")
	TObjectPtr<UMotionWarpingComponent> MotionWarping;

	/**
	 * Bones hidden on the mesh when the game starts (the bone and everything below it). For a mesh with parts built in that the game does
	 * not want, e.g. a Paragon backpack cannon ("canon_base"). Set in the character's data-only Blueprint.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Mesh")
	TArray<FName> HiddenBones;

	/** Bones hidden only while State.Parkouring or State.Attacking is on (e.g. the rifle in the hand: those clips use the hands for something else). */
	UPROPERTY(EditDefaultsOnly, Category = "Mesh")
	TArray<FName> ActionHiddenBones;

	void OnActionTagChanged(const FGameplayTag Tag, int32 Count);

	/** Post-process anim blueprint with the hand IK (ABP_ParkourIK). Applied to this mesh only, the shared mannequin mesh asset stays untouched. */
	UPROPERTY(EditDefaultsOnly, Category = "Parkour")
	TSubclassOf<UAnimInstance> ParkourIKAnimClass;

	/** Holds the lock-on target; rules in Core/ARLockOn.h. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LockOn")
	TObjectPtr<UARLockOnComponent> LockOn;

	/** Hand targets for the mantle hand IK, read by the post-process anim instance. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parkour")
	TObjectPtr<UARParkourIKComponent> ParkourIK;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LockOnAction;

	/** Held to walk instead of jog. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> WalkAction;

	/** Speed while walking (cm/s): the walk row of the movement blend space is made for 300. */
	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0"))
	float WalkSpeed = 300.f;

	/** How long (s) a jump press made too far from a vaultable wall is kept: the character runs up to it, then the parkour starts. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (ClampMin = "0"))
	float ParkourBufferSeconds = 0.6f;

	/** Player HUD health bar, shown for the local player. Optional. Set in the character's data-only Blueprint. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UARHealthBarWidget> HealthBarWidgetClass;
};
