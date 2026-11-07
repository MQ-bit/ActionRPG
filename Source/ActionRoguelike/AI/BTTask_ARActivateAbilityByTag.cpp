// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BTTask_ARActivateAbilityByTag.h"

#include "ActionRoguelike.h"
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
		UE_LOG(LogAR, Verbose, TEXT("[BT] %s: TryActivate %s failed"), *GetNameSafe(ASC->GetAvatarActor()), *AbilityTag.ToString());
		Unbind();
		return EBTNodeResult::Failed;
	}

	// An ability that ended inside TryActivate cannot call FinishLatentTask from here; report it as the result instead.
	if (ResultWhenEndedDuringExecute != EBTNodeResult::InProgress)
	{
		UE_LOG(LogAR, Verbose, TEXT("[BT] %s: %s ended inside TryActivate, result %d"),
			*GetNameSafe(ASC->GetAvatarActor()), *AbilityTag.ToString(), (int32)ResultWhenEndedDuringExecute);
		Unbind();
		return ResultWhenEndedDuringExecute;
	}
	UE_LOG(LogAR, Verbose, TEXT("[BT] %s: %s started, waiting for it to end"), *GetNameSafe(ASC->GetAvatarActor()), *AbilityTag.ToString());
	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_ARActivateAbilityByTag::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		UE_LOG(LogAR, Verbose, TEXT("[BT] %s: task aborted, cancelling %s"), *GetNameSafe(ASC->GetAvatarActor()), *AbilityTag.ToString());
		const FGameplayTagContainer Tags(AbilityTag);
		ASC->CancelAbilities(&Tags);
	}
	Unbind();
	return EBTNodeResult::Aborted;
}

void UBTTask_ARActivateAbilityByTag::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	UE_LOG(LogAR, Verbose, TEXT("[BT] %s: task finished, result %d"), *GetNameSafe(OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr), (int32)TaskResult);
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

	UBehaviorTreeComponent* OwnerComp = OwnerCompWeak.Get();
	UE_LOG(LogAR, Verbose, TEXT("[BT] %s: ability ended (cancelled %d), owner component %s"),
		*GetNameSafe(BoundASC.IsValid() ? BoundASC->GetAvatarActor() : nullptr), Data.bWasCancelled ? 1 : 0, OwnerComp ? TEXT("valid") : TEXT("GONE"));
	if (OwnerComp)
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
