// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ARAIAssetLibrary.h"

#include "ActionRoguelike.h"
#include "AI/AREnemyAIController.h"
#include "AI/BTService_ARUpdateTargetDistance.h"
#include "AI/BTTask_ARActivateAbilityByTag.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_RotateToFaceBBEntry.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "GameFramework/Actor.h"

namespace
{
	const FName DistanceKeyName(TEXT("DistanceToTarget"));
	const FName InAttackRangeKeyName(TEXT("bInAttackRange"));

	/** Sets a property by C++ name from its text form, which also reaches protected properties. */
	bool SetProp(UObject* Object, const TCHAR* PropertyName, const FString& Text)
	{
		FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName);
		if (!Property)
		{
			UE_LOG(LogAR, Error, TEXT("%s has no property %s"), *Object->GetClass()->GetName(), PropertyName);
			return false;
		}
		if (!Property->ImportText_Direct(*Text, Property->ContainerPtrToValuePtr<void>(Object), Object, PPF_None))
		{
			UE_LOG(LogAR, Error, TEXT("%s.%s: could not parse \"%s\""), *Object->GetClass()->GetName(), PropertyName, *Text);
			return false;
		}
		return true;
	}

	FString KeyText(const FName KeyName)
	{
		return FString::Printf(TEXT("(SelectedKeyName=\"%s\")"), *KeyName.ToString());
	}

	/** Engine 5.8 BT nodes take FValueOrBBKey_Float for times and radii: a literal value uses DefaultValue. */
	FString FloatOrKeyText(const float Value)
	{
		return FString::Printf(TEXT("(DefaultValue=%f)"), Value);
	}

	void EnsureKey(UBlackboardData* Blackboard, const FName Name, UBlackboardKeyType* Type)
	{
		for (FBlackboardEntry& Entry : Blackboard->Keys)
		{
			if (Entry.EntryName == Name)
			{
				Entry.KeyType = Type;
				return;
			}
		}
		FBlackboardEntry Entry;
		Entry.EntryName = Name;
		Entry.KeyType = Type;
		Blackboard->Keys.Add(Entry);
	}

	UBTTaskNode* AddTask(UBTCompositeNode* Parent, UBTTaskNode* Task)
	{
		FBTCompositeChild Child;
		Child.ChildTask = Task;
		Parent->Children.Add(Child);
		return Task;
	}

	UBTDecorator_Blackboard* MakeKeyDecorator(UObject* Outer, const FName KeyName, const TCHAR* FlowAbortMode, const TCHAR* Label, bool& bOk)
	{
		UBTDecorator_Blackboard* Decorator = NewObject<UBTDecorator_Blackboard>(Outer);
		Decorator->NodeName = Label;
		bOk &= SetProp(Decorator, TEXT("BlackboardKey"), KeyText(KeyName));
		bOk &= SetProp(Decorator, TEXT("BasicOperation"), TEXT("Set"));
		bOk &= SetProp(Decorator, TEXT("FlowAbortMode"), FlowAbortMode);
		return Decorator;
	}

	void AddCompositeChild(UBTCompositeNode* Parent, UBTCompositeNode* Child, UBTDecorator_Blackboard* Decorator)
	{
		FBTCompositeChild Entry;
		Entry.ChildComposite = Child;
		Entry.Decorators.Add(Decorator);
		// One decorator: And over 1 operand, then Test of operand 0 (the same pattern the editor writes).
		Entry.DecoratorOps.Add(FBTDecoratorLogic(EBTDecoratorLogic::And, 1));
		Entry.DecoratorOps.Add(FBTDecoratorLogic(EBTDecoratorLogic::Test, 0));
		Parent->Children.Add(Entry);
	}
}

bool UARAIAssetLibrary::SetupEnemyBlackboard(UBlackboardData* Blackboard)
{
	if (!Blackboard)
	{
		return false;
	}

	UBlackboardKeyType_Object* TargetType = NewObject<UBlackboardKeyType_Object>(Blackboard);
	TargetType->BaseClass = AActor::StaticClass();

	Blackboard->Modify();
	EnsureKey(Blackboard, ARBlackboard::TargetActor, TargetType);
	EnsureKey(Blackboard, DistanceKeyName, NewObject<UBlackboardKeyType_Float>(Blackboard));
	EnsureKey(Blackboard, InAttackRangeKeyName, NewObject<UBlackboardKeyType_Bool>(Blackboard));
	Blackboard->UpdateKeyIDs();
	Blackboard->MarkPackageDirty();
	return true;
}

bool UARAIAssetLibrary::BuildEnemyBehaviorTree(UBehaviorTree* Tree, UBlackboardData* Blackboard)
{
	if (!Tree || !Blackboard)
	{
		return false;
	}

	bool bOk = true;

	// Root selector with the service that keeps distance and range up to date.
	UBTComposite_Selector* Root = NewObject<UBTComposite_Selector>(Tree);
	Root->NodeName = TEXT("Enemy");

	UBTService_ARUpdateTargetDistance* Service = NewObject<UBTService_ARUpdateTargetDistance>(Root);
	bOk &= SetProp(Service, TEXT("TargetKey"), KeyText(ARBlackboard::TargetActor));
	bOk &= SetProp(Service, TEXT("DistanceKey"), KeyText(DistanceKeyName));
	bOk &= SetProp(Service, TEXT("InAttackRangeKey"), KeyText(InAttackRangeKeyName));
	bOk &= SetProp(Service, TEXT("AttackRange"), TEXT("200.0"));
	Root->Services.Add(Service);

	// 1) In attack range: face the target, swing, wait. Only interrupts lower priority branches (the chase).
	UBTComposite_Sequence* Attack = NewObject<UBTComposite_Sequence>(Tree);
	Attack->NodeName = TEXT("Attack");

	UBTTask_RotateToFaceBBEntry* Face = NewObject<UBTTask_RotateToFaceBBEntry>(Tree);
	bOk &= SetProp(Face, TEXT("BlackboardKey"), KeyText(ARBlackboard::TargetActor));
	AddTask(Attack, Face);

	UBTTask_ARActivateAbilityByTag* Swing = NewObject<UBTTask_ARActivateAbilityByTag>(Tree);
	bOk &= SetProp(Swing, TEXT("AbilityTag"), TEXT("(TagName=\"Ability.Attack\")"));
	AddTask(Attack, Swing);

	UBTTask_Wait* Cooldown = NewObject<UBTTask_Wait>(Tree);
	Cooldown->NodeName = TEXT("Attack Cooldown");
	bOk &= SetProp(Cooldown, TEXT("WaitTime"), FloatOrKeyText(1.f));
	AddTask(Attack, Cooldown);

	AddCompositeChild(Root, Attack, MakeKeyDecorator(Root, InAttackRangeKeyName, TEXT("LowerPriority"), TEXT("In Attack Range"), bOk));

	// 2) Has a target: walk to it. Stops if the target is lost.
	UBTComposite_Sequence* Chase = NewObject<UBTComposite_Sequence>(Tree);
	Chase->NodeName = TEXT("Chase");

	UBTTask_MoveTo* Move = NewObject<UBTTask_MoveTo>(Tree);
	bOk &= SetProp(Move, TEXT("BlackboardKey"), KeyText(ARBlackboard::TargetActor));
	bOk &= SetProp(Move, TEXT("AcceptableRadius"), FloatOrKeyText(150.f));
	AddTask(Chase, Move);

	AddCompositeChild(Root, Chase, MakeKeyDecorator(Root, ARBlackboard::TargetActor, TEXT("Self"), TEXT("Has Target"), bOk));

	// 3) Nothing to do: idle.
	UBTTask_Wait* Idle = NewObject<UBTTask_Wait>(Tree);
	Idle->NodeName = TEXT("Idle");
	bOk &= SetProp(Idle, TEXT("WaitTime"), FloatOrKeyText(1.f));
	AddTask(Root, Idle);

	Tree->Modify();
	Tree->BlackboardAsset = Blackboard;
	Tree->RootNode = Root;
	Tree->MarkPackageDirty();
	return bOk;
}
