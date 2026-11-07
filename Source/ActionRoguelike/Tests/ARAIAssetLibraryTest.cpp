// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/ARAIAssetLibrary.h"
#include "AI/AREnemyAIController.h"
#include "AI/BTService_ARUpdateTargetDistance.h"
#include "AI/BTTask_ARActivateAbilityByTag.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"

#define AR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARAIBlackboardTest, "ActionRoguelike.AI.EnemyBlackboardKeys", AR_TEST_FLAGS)
bool FARAIBlackboardTest::RunTest(const FString& Parameters)
{
	UBlackboardData* Blackboard = NewObject<UBlackboardData>(GetTransientPackage());
	TestTrue(TEXT("setup succeeds"), UARAIAssetLibrary::SetupEnemyBlackboard(Blackboard));
	TestTrue(TEXT("setup is repeatable"), UARAIAssetLibrary::SetupEnemyBlackboard(Blackboard));

	TestTrue(TEXT("TargetActor key exists"), Blackboard->GetKeyID(ARBlackboard::TargetActor) != FBlackboard::InvalidKey);
	TestTrue(TEXT("DistanceToTarget key exists"), Blackboard->GetKeyID(TEXT("DistanceToTarget")) != FBlackboard::InvalidKey);
	TestTrue(TEXT("bInAttackRange key exists"), Blackboard->GetKeyID(TEXT("bInAttackRange")) != FBlackboard::InvalidKey);

	const FBlackboardEntry* Target = Blackboard->GetKey(Blackboard->GetKeyID(ARBlackboard::TargetActor));
	const UBlackboardKeyType_Object* TargetType = Target ? Cast<UBlackboardKeyType_Object>(Target->KeyType) : nullptr;
	TestTrue(TEXT("TargetActor is an Object key"), TargetType != nullptr);
	TestTrue(TEXT("TargetActor accepts Actors (needed by MoveTo and RotateToFace)"), TargetType && TargetType->BaseClass == AActor::StaticClass());

	int32 TargetKeys = 0;
	for (const FBlackboardEntry& Entry : Blackboard->Keys)
	{
		TargetKeys += Entry.EntryName == ARBlackboard::TargetActor ? 1 : 0;
	}
	TestEqual(TEXT("running setup twice does not duplicate keys"), TargetKeys, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARAITreeTest, "ActionRoguelike.AI.EnemyBehaviorTreeStructure", AR_TEST_FLAGS)
bool FARAITreeTest::RunTest(const FString& Parameters)
{
	UBlackboardData* Blackboard = NewObject<UBlackboardData>(GetTransientPackage());
	UARAIAssetLibrary::SetupEnemyBlackboard(Blackboard);
	UBehaviorTree* Tree = NewObject<UBehaviorTree>(GetTransientPackage());

	TestTrue(TEXT("every node property name resolves"), UARAIAssetLibrary::BuildEnemyBehaviorTree(Tree, Blackboard));
	TestTrue(TEXT("tree uses the blackboard"), Tree->BlackboardAsset == Blackboard);

	UBTCompositeNode* Root = Tree->RootNode;
	if (!TestNotNull(TEXT("root exists"), Root) || !TestTrue(TEXT("root is a selector"), Root->IsA<UBTComposite_Selector>()))
	{
		return false;
	}

	TestEqual(TEXT("root has three branches"), Root->Children.Num(), 3);
	TestEqual(TEXT("root has the distance service"), Root->Services.Num(), 1);
	TestTrue(TEXT("service is the project service"), Root->Services.Num() > 0 && Root->Services[0]->IsA<UBTService_ARUpdateTargetDistance>());

	if (Root->Children.Num() != 3) { return false; }

	const FBTCompositeChild& Attack = Root->Children[0];
	TestTrue(TEXT("branch 1 is a sequence"), Attack.ChildComposite && Attack.ChildComposite->IsA<UBTComposite_Sequence>());
	TestEqual(TEXT("branch 1 has one decorator"), Attack.Decorators.Num(), 1);
	TestEqual(TEXT("branch 1 decorator ops: And + Test"), Attack.DecoratorOps.Num(), 2);
	if (Attack.ChildComposite)
	{
		const TArray<FBTCompositeChild>& Steps = Attack.ChildComposite->Children;
		TestEqual(TEXT("attack branch has face, ability, wait"), Steps.Num(), 3);
		TestTrue(TEXT("second step activates the ability"), Steps.Num() > 1 && Steps[1].ChildTask && Steps[1].ChildTask->IsA<UBTTask_ARActivateAbilityByTag>());
	}

	const FBTCompositeChild& Chase = Root->Children[1];
	TestTrue(TEXT("branch 2 is a sequence"), Chase.ChildComposite && Chase.ChildComposite->IsA<UBTComposite_Sequence>());
	TestTrue(TEXT("branch 2 moves to the target"), Chase.ChildComposite && Chase.ChildComposite->Children.Num() == 1
		&& Chase.ChildComposite->Children[0].ChildTask && Chase.ChildComposite->Children[0].ChildTask->IsA<UBTTask_MoveTo>());

	TestTrue(TEXT("branch 3 is the idle task"), Root->Children[2].ChildTask != nullptr);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
