// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/ARCharacterBase.h"

#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GAS/ARAbilitySystemComponent.h"
#include "GAS/ARAttributeSet.h"
#include "GAS/ARGameplayTags.h"

AARCharacterBase::AARCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<UARAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UARAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AARCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

ETeamAttitude::Type AARCharacterBase::GetTeamAttitudeTowards(const AActor& Other) const
{
	const IGenericTeamAgentInterface* OtherAgent = Cast<const IGenericTeamAgentInterface>(&Other);
	if (!OtherAgent)
	{
		return ETeamAttitude::Neutral;
	}

	const uint8 OtherTeam = OtherAgent->GetGenericTeamId().GetId();
	if (TeamId == ARTeam::NoTeam || OtherTeam == ARTeam::NoTeam)
	{
		return ETeamAttitude::Neutral;
	}
	return ARTeam::IsHostile(TeamId, OtherTeam) ? ETeamAttitude::Hostile : ETeamAttitude::Friendly;
}

void AARCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// Owner and avatar are the same actor.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	for (const TSubclassOf<UARGameplayAbility>& AbilityClass : StartupAbilities)
	{
		AbilitySystemComponent->GiveStartupAbility(AbilityClass);
	}

	AttributeSet->OnHealthChanged.AddUObject(this, &AARCharacterBase::HandleHealthChanged);
}

void AARCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AttributeSet)
	{
		AttributeSet->OnHealthChanged.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AARCharacterBase::HandleHealthChanged(float OldValue, float NewValue, float MaxHealth)
{
	if (NewValue <= 0.f)
	{
		Die();
	}
}

void AARCharacterBase::Die()
{
	if (bDead)
	{
		return;
	}
	bDead = true;

	AbilitySystemComponent->AddLooseGameplayTag(TAG_State_Dead.GetTag());
	AbilitySystemComponent->CancelAllAbilities();

	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// An AI controller stops its behavior tree with the pawn; a player keeps the controller but loses input.
	if (AController* PawnController = GetController())
	{
		if (Cast<AAIController>(PawnController))
		{
			DetachFromControllerPendingDestroy();
		}
		else if (APlayerController* PlayerController = Cast<APlayerController>(PawnController))
		{
			DisableInput(PlayerController);
		}
	}

	if (DeathMontage)
	{
		PlayAnimMontage(DeathMontage);
	}

	OnDeath();
	OnDied.Broadcast(this);
}
