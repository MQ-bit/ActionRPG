// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BTTask_ARActivateAbilityByTag.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UBTTask_ARActivateAbilityByTag::UBTTask_ARActivateAbilityByTag()
{
	NodeName = TEXT("Activate Ability By Tag");
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_ARActivateAbilityByTag::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* Controller = OwnerComp.GetAIOwner();
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Controller ? Controller->GetPawn() : nullptr);
	if (!ASC || !AbilityTag.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	Unbind();
	BoundASC = ASC;
	OwnerCompWeak = &OwnerComp;
	EndedHandle = ASC->OnAbilityEnded.AddUObject(this, &UBTTask_ARActivateAbilityByTag::OnAbilityEnded);

	bExecuting = true;
	ResultWhenEndedDuringExecute = EBTNodeResult::InProgress;
	const bool bActivated = ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(AbilityTag));
	bExecuting = false;

	if (!bActivated)
	{
		Unbind();
		return EBTNodeResult::Failed;
	}

	// An ability that ended inside TryActivate cannot call FinishLatentTask from here; report it as the result instead.
	if (ResultWhenEndedDuringExecute != EBTNodeResult::InProgress)
	{
		Unbind();
		return ResultWhenEndedDuringExecute;
	}
	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_ARActivateAbilityByTag::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		const FGameplayTagContainer Tags(AbilityTag);
		ASC->CancelAbilities(&Tags);
	}
	Unbind();
	return EBTNodeResult::Aborted;
}

void UBTTask_ARActivateAbilityByTag::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	Unbind();
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UBTTask_ARActivateAbilityByTag::OnAbilityEnded(const FAbilityEndedData& Data)
{
	if (!Data.AbilityThatEnded || !Data.AbilityThatEnded->GetAssetTags().HasTag(AbilityTag))
	{
		return;
	}

	const EBTNodeResult::Type Result = Data.bWasCancelled ? EBTNodeResult::Failed : EBTNodeResult::Succeeded;
	if (bExecuting)
	{
		ResultWhenEndedDuringExecute = Result;
		return;
	}

	if (UBehaviorTreeComponent* OwnerComp = OwnerCompWeak.Get())
	{
		FinishLatentTask(*OwnerComp, Result);
	}
}

void UBTTask_ARActivateAbilityByTag::Unbind()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		if (EndedHandle.IsValid())
		{
			ASC->OnAbilityEnded.Remove(EndedHandle);
		}
	}
	EndedHandle.Reset();
	BoundASC.Reset();
}
