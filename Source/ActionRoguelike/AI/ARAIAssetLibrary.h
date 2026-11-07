// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ARAIAssetLibrary.generated.h"

class UBehaviorTree;
class UBlackboardData;

/**
 * Editor-time helpers that fill the first enemy's AI assets from code, so the tree is reproducible and does not
 * have to be assembled by hand. Node properties that are protected in the engine are set through reflection.
 * Call them from the editor (Python or a utility widget), then open the asset: the editor builds the graph from the tree.
 */
UCLASS()
class ACTIONROGUELIKE_API UARAIAssetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Adds or replaces the keys the enemy tree uses: TargetActor (Actor), DistanceToTarget (float), bInAttackRange (bool). */
	UFUNCTION(BlueprintCallable, Category = "AR|AI|Editor")
	static bool SetupEnemyBlackboard(UBlackboardData* Blackboard);

	/**
	 * Replaces the tree with: Selector [ in range: face target, attack ability, wait | has target: move to target | wait ].
	 * Returns false if any node property name could not be found.
	 */
	UFUNCTION(BlueprintCallable, Category = "AR|AI|Editor")
	static bool BuildEnemyBehaviorTree(UBehaviorTree* Tree, UBlackboardData* Blackboard);
};
