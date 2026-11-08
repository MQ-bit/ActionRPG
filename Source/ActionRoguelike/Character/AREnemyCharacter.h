// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/ARCharacterBase.h"
#include "AREnemyCharacter.generated.h"

class UBehaviorTree;
class UWidgetComponent;

/**
 * Basic enemy: team Enemy, AI controller, behavior tree set in a data-only Blueprint.
 * Without a behavior tree it just stands there (used as the training dummy).
 */
UCLASS()
class ACTIONROGUELIKE_API AAREnemyCharacter : public AARCharacterBase
{
	GENERATED_BODY()

public:
	AAREnemyCharacter();

	UBehaviorTree* GetBehaviorTree() const { return BehaviorTree; }

protected:
	virtual void BeginPlay() override;
	virtual void OnDeath() override;

	/** Health bar above the head. Without a widget class (set in the data-only Blueprint) it shows nothing. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UWidgetComponent> HealthBarComponent;

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	/** Seconds until the body is removed after death. 0 keeps it. */
	UPROPERTY(EditDefaultsOnly, Category = "Death", meta = (ClampMin = "0"))
	float DeathLifeSpan = 4.f;
};
