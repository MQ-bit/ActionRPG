// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Core/ARTeam.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "ARCharacterBase.generated.h"

class AARCharacterBase;
class UAnimMontage;
class UARAbilitySystemComponent;
class UARAttributeSet;
class UARGameplayAbility;

DECLARE_MULTICAST_DELEGATE_OneParam(FAROnCharacterDied, AARCharacterBase*);

/** Shared base for player and enemy characters. Owns the ASC and attributes (single-player: no PlayerState ownership). */
UCLASS(Abstract)
class ACTIONROGUELIKE_API AARCharacterBase : public ACharacter, public IAbilitySystemInterface, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	AARCharacterBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId(TeamId); }
	virtual void SetGenericTeamId(const FGenericTeamId& InTeamId) override { TeamId = InTeamId.GetId(); }
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	bool IsDead() const { return bDead; }

	/** Read access for UI (see UARHealthBarWidget). Change attributes only through Gameplay Effects. */
	UARAttributeSet* GetAttributeSet() const { return AttributeSet; }

	/** Broadcast once, when Health reaches 0. */
	FAROnCharacterDied OnDied;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Extra death handling for subclasses (called once, after the shared handling). */
	virtual void OnDeath() {}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UARAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UARAttributeSet> AttributeSet;

	/** Granted in BeginPlay. Set in the character's data-only Blueprint. */
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TArray<TSubclassOf<UARGameplayAbility>> StartupAbilities;

	/** See ARTeam::Player / ARTeam::Enemy. A character without a team is hostile to nobody. */
	UPROPERTY(EditDefaultsOnly, Category = "Team")
	uint8 TeamId = ARTeam::NoTeam;

	/** Played when the character dies. Optional. */
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	TObjectPtr<UAnimMontage> DeathMontage;

private:
	void HandleHealthChanged(float OldValue, float NewValue, float MaxHealth);
	void Die();

	bool bDead = false;
};
