// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/ARCharacterBase.h"
#include "ARPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UInputMappingContext;
class UARHealthBarWidget;
struct FInputActionValue;

/** Third-person player: camera boom, Enhanced Input move/look. Assets are assigned in a data-only Blueprint. */
UCLASS()
class ACTIONROGUELIKE_API AARPlayerCharacter : public AARCharacterBase
{
	GENERATED_BODY()

public:
	AARPlayerCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	/** Forwards the attack input to the ASC by tag; the ability itself lives in UARGA_BasicAttack. */
	void OnAttackInput();

	/** Console command `ARAttack`: same path as the attack input, for testing without a mouse. */
	UFUNCTION(Exec)
	void ARAttack();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> AttackAction;

	/** Player HUD health bar, shown for the local player. Optional. Set in the character's data-only Blueprint. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UARHealthBarWidget> HealthBarWidgetClass;
};
