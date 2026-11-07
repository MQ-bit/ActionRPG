// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/AREnemyAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/AREnemyCharacter.h"
#include "GenericTeamAgentInterface.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

AAREnemyAIController::AAREnemyAIController()
{
	SetPerceptionComponent(*CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception")));

	UAISenseConfig_Sight* Sight = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	Sight->SightRadius = 1500.f;
	Sight->LoseSightRadius = 1800.f;
	Sight->PeripheralVisionAngleDegrees = 100.f;
	Sight->DetectionByAffiliation.bDetectEnemies = true;
	Sight->DetectionByAffiliation.bDetectNeutrals = false;
	Sight->DetectionByAffiliation.bDetectFriendlies = false;
	Sight->SetMaxAge(ARAI::TargetMemorySeconds);

	GetPerceptionComponent()->ConfigureSense(*Sight);
	GetPerceptionComponent()->SetDominantSense(Sight->GetSenseImplementation());
}

void AAREnemyAIController::BeginPlay()
{
	Super::BeginPlay();

	GetPerceptionComponent()->OnTargetPerceptionUpdated.AddDynamic(this, &AAREnemyAIController::OnTargetPerceptionUpdated);
	GetPerceptionComponent()->OnTargetPerceptionForgotten.AddDynamic(this, &AAREnemyAIController::OnTargetPerceptionForgotten);
}

void AAREnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Perception decides who is an enemy from the controller's team, which follows the pawn.
	if (const IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(InPawn))
	{
		SetGenericTeamId(TeamAgent->GetGenericTeamId());
		GetPerceptionComponent()->RequestStimuliListenerUpdate();
	}

	if (const AAREnemyCharacter* Enemy = Cast<AAREnemyCharacter>(InPawn))
	{
		if (UBehaviorTree* Tree = Enemy->GetBehaviorTree())
		{
			RunBehaviorTree(Tree);
		}
	}
}

void AAREnemyAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// Not named "Blackboard": that hides AAIController::Blackboard.
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!BlackboardComp || !Actor)
	{
		return;
	}

	// Losing sight keeps the target; it is dropped when perception forgets it (ARAI::TargetMemorySeconds later).
	if (Stimulus.WasSuccessfullySensed())
	{
		BlackboardComp->SetValueAsObject(ARBlackboard::TargetActor, Actor);
	}
}

void AAREnemyAIController::OnTargetPerceptionForgotten(AActor* Actor)
{
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (BlackboardComp && Actor && BlackboardComp->GetValueAsObject(ARBlackboard::TargetActor) == Actor)
	{
		BlackboardComp->ClearValue(ARBlackboard::TargetActor);
	}
}
