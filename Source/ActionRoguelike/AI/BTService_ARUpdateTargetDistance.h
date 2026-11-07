// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_ARUpdateTargetDistance.generated.h"

/** Writes the flat distance to the target and whether it is inside the attack range into the blackboard. */
UCLASS()
class ACTIONROGUELIKE_API UBTService_ARUpdateTargetDistance : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_ARUpdateTargetDistance();

protected:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector DistanceKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector InAttackRangeKey;

	/** Flat distance (cm) under which the target counts as in attack range. */
	UPROPERTY(EditAnywhere, Category = "Attack", meta = (ClampMin = "0"))
	float AttackRange = 200.f;
};
