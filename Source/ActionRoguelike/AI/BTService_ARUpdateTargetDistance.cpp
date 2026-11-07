// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BTService_ARUpdateTargetDistance.h"

#include "ActionRoguelike.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "GameFramework/Pawn.h"

UBTService_ARUpdateTargetDistance::UBTService_ARUpdateTargetDistance()
{
	NodeName = TEXT("Update Target Distance");
	Interval = 0.2f;
	RandomDeviation = 0.05f;

	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_ARUpdateTargetDistance, TargetKey), AActor::StaticClass());
	DistanceKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_ARUpdateTargetDistance, DistanceKey));
	InAttackRangeKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_ARUpdateTargetDistance, InAttackRangeKey));
}

void UBTService_ARUpdateTargetDistance::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		TargetKey.ResolveSelectedKey(*BlackboardAsset);
		DistanceKey.ResolveSelectedKey(*BlackboardAsset);
		InAttackRangeKey.ResolveSelectedKey(*BlackboardAsset);
	}
}

void UBTService_ARUpdateTargetDistance::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	const AAIController* Controller = OwnerComp.GetAIOwner();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Blackboard || !Pawn)
	{
		return;
	}

	const bool bWasInRange = Blackboard->GetValueAsBool(InAttackRangeKey.SelectedKeyName);

	const AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!Target)
	{
		Blackboard->SetValueAsBool(InAttackRangeKey.SelectedKeyName, false);
		Blackboard->ClearValue(DistanceKey.SelectedKeyName);
		if (bWasInRange)
		{
			UE_LOG(LogAR, Verbose, TEXT("[BT] %s: target lost while in attack range"), *GetNameSafe(Pawn));
		}
		return;
	}

	const float Distance = FVector::Dist2D(Pawn->GetActorLocation(), Target->GetActorLocation());
	const bool bInRange = Distance <= AttackRange;
	Blackboard->SetValueAsFloat(DistanceKey.SelectedKeyName, Distance);
	Blackboard->SetValueAsBool(InAttackRangeKey.SelectedKeyName, bInRange);

	// Log only the transitions; the service ticks several times a second.
	if (bInRange != bWasInRange)
	{
		UE_LOG(LogAR, Verbose, TEXT("[BT] %s: attack range %s (target %s, dist %.0f, range %.0f)"),
			*GetNameSafe(Pawn), bInRange ? TEXT("entered") : TEXT("left"), *GetNameSafe(Target), Distance, AttackRange);
	}
}
