// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AREnemyAIController.generated.h"

struct FAIStimulus;

/** Blackboard key names written by the controller. The behavior tree assets must define keys with these names. */
namespace ARBlackboard
{
	inline const FName TargetActor = TEXT("TargetActor");
}

/**
 * Runs the behavior tree set on the possessed AAREnemyCharacter and feeds it the perceived hostile target
 * (sight only; hostile means a different team, see ARTeam).
 */
UCLASS()
class ACTIONROGUELIKE_API AAREnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AAREnemyAIController();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
};
