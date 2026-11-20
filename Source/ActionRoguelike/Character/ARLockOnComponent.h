// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARLockOnComponent.generated.h"

class AARCharacterBase;
class AAREnemyCharacter;

/**
 * Lock-on for the player: Toggle() picks the nearest visible hostile enemy in front of the camera (rules in Core/ARLockOn.h),
 * turns the controller toward it and lets the body strafe. Ticks only while locked. Release() is safe to call at any time.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ACTIONROGUELIKE_API UARLockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UARLockOnComponent();

	/** Locks the best target, or lets go of the current one. Does nothing while parkouring or dead. */
	void Toggle();

	/** Lets go of the target and restores the movement settings. Repeated calls do nothing. */
	void Release();

	bool IsLockedOn() const { return bLocked; }
	AAREnemyCharacter* GetTarget() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Controller turn speed toward the target (deg/s). */
	UPROPERTY(EditAnywhere, Category = "LockOn", meta = (ClampMin = "1"))
	float RotationSpeedDegrees = 360.f;

	/** The camera pitch toward the target is limited to this (deg). */
	UPROPERTY(EditAnywhere, Category = "LockOn", meta = (ClampMin = "0", ClampMax = "80"))
	float MaxPitchDegrees = 30.f;

private:
	bool HasLineOfSight(const AActor& Other) const;
	void HandleTargetDied(AARCharacterBase* Died);

	TWeakObjectPtr<AAREnemyCharacter> Target;
	FDelegateHandle DiedHandle;
	float OccludedSeconds = 0.f;
	bool bLocked = false;
	bool bSavedOrientToMovement = true;
	bool bSavedDesiredRotation = false;
};
