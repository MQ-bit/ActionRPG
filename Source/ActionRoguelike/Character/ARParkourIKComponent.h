// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARParkourIKComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class UPrimitiveComponent;

/**
 * Hands for a mantle: the parkour ability puts world-space targets on the ledge edge here for the length of the montage,
 * and the post-process anim instance (UARParkourIKAnimInstance) reads them to drive the hand IK. A hand is only pulled
 * to its target while the montage plays inside that hand's window.
 */
UCLASS(ClassGroup = (Parkour), meta = (BlueprintSpawnableComponent))
class ACTIONROGUELIKE_API UARParkourIKComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UARParkourIKComponent();

	/** Side 0 = left hand, 1 = right hand. A window with End <= Start leaves that hand alone. Montage times in seconds. */
	void SetHand(int32 Side, const FVector& WorldTarget, float WindowStart, float WindowEnd);

	/** Which montage the windows refer to, and the ramp in and out of each window (s). */
	void Begin(UAnimMontage* InMontage, float InRamp);

	/** Clears everything: no hand is pulled any more. */
	void End();

	/** The top the mantle ends on: the feet are kept above its surface (see UARParkourIKAnimInstance). */
	void SetLedge(const UPrimitiveComponent* Top, const FVector& InInward = FVector::ZeroVector) { Ledge = Top; Inward = InInward.GetSafeNormal2D(); }
	const UPrimitiveComponent* GetLedge() const { return Ledge.Get(); }

	/** The horizontal direction into the obstacle (its front face looks the other way); zero when unknown. */
	const FVector& GetInward() const { return Inward; }

	bool IsActive() const { return bActive; }

	/** True while the montage is within its last WindowSeconds (its blend-out included): the feet may be planted on the ledge then. */
	bool InFinalPhase(const UAnimInstance* MainAnimInstance, float WindowSeconds) const;

	/** World target and IK weight (0..1) of a hand right now, read from the montage position of MainAnimInstance. */
	void GetHand(int32 Side, const UAnimInstance* MainAnimInstance, FVector& OutWorldTarget, float& OutWeight) const;

private:
	bool bActive = false;
	TWeakObjectPtr<const UPrimitiveComponent> Ledge;
	FVector Inward = FVector::ZeroVector;
	TWeakObjectPtr<UAnimMontage> Montage;
	float Ramp = 0.15f;
	FVector Targets[2] = {FVector::ZeroVector, FVector::ZeroVector};
	float WindowStart[2] = {0.f, 0.f};
	float WindowEnd[2] = {0.f, 0.f};
};
