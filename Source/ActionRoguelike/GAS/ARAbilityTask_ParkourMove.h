// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "ARAbilityTask_ParkourMove.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARParkourMoveDelegate);

/**
 * Moves the avatar along lift -> traverse -> settle at constant speed: straight up to ApexZ, across to above the target,
 * then down onto it. Every step is swept, so the path never goes through geometry; a blocked sweep ends the task with
 * OnBlocked. On arrival the character is switched to walking with no velocity. If the task is cut short, the character
 * is left falling from where it is (always a free spot, since the path is swept).
 */
UCLASS()
class ACTIONROGUELIKE_API UARAbilityTask_ParkourMove : public UAbilityTask
{
	GENERATED_BODY()

public:
	UARAbilityTask_ParkourMove();

	/** ApexZ is raised to at least the start and target heights. */
	static UARAbilityTask_ParkourMove* ParkourMove(UGameplayAbility* OwningAbility, const FVector& Target, float ApexZ, float Duration);

	UPROPERTY(BlueprintAssignable)
	FARParkourMoveDelegate OnFinished;

	UPROPERTY(BlueprintAssignable)
	FARParkourMoveDelegate OnBlocked;

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInAbilityEnded) override;

private:
	FVector PositionAtDistance(float Distance) const;
	void Finish(bool bArrived);

	FVector Target = FVector::ZeroVector;
	float ApexZ = 0.f;
	float Duration = 0.5f;
	float Elapsed = 0.f;

	FVector Waypoints[4];
	float SegmentLength[3] = {0.f, 0.f, 0.f};
	float TotalLength = 0.f;
	/** Path length already swept. */
	float Travelled = 0.f;
};
