// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayTagContainer.h"
#include "BTTask_ARActivateAbilityByTag.generated.h"

class UAbilitySystemComponent;
struct FAbilityEndedData;

/**
 * Activates the pawn's ability with the given asset tag and finishes when that ability ends
 * (Succeeded if it ended normally, Failed if it was cancelled or could not start). Aborting cancels the ability.
 */
UCLASS()
class ACTIONROGUELIKE_API UBTTask_ARActivateAbilityByTag : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ARActivateAbilityByTag();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Ability")
	FGameplayTag AbilityTag;

private:
	void OnAbilityEnded(const FAbilityEndedData& Data);
	void Unbind();

	// This node is instanced per AI, so these members belong to one running task.
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<UBehaviorTreeComponent> OwnerCompWeak;
	FDelegateHandle EndedHandle;
	bool bExecuting = false;
	EBTNodeResult::Type ResultWhenEndedDuringExecute = EBTNodeResult::InProgress;
};
