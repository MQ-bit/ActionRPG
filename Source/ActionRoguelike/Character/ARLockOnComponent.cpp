// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/ARLockOnComponent.h"

#include "AbilitySystemComponent.h"
#include "Character/AREnemyCharacter.h"
#include "Core/ARLockOn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GAS/ARGameplayTags.h"

UARLockOnComponent::UARLockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

AAREnemyCharacter* UARLockOnComponent::GetTarget() const
{
	return Target.Get();
}

bool UARLockOnComponent::HasLineOfSight(const AActor& Other) const
{
	const AActor* Owner = GetOwner();
	FCollisionQueryParams Params(NAME_None, false, Owner);
	Params.AddIgnoredActor(&Other);
	// Eye to chest: only the centre line is tested, so a small cover does not hide anyone.
	return !GetWorld()->LineTraceTestByChannel(Owner->GetActorLocation() + FVector(0.f, 0.f, 60.f), Other.GetActorLocation() + FVector(0.f, 0.f, 30.f), ECC_Visibility, Params);
}

void UARLockOnComponent::Toggle()
{
	if (bLocked)
	{
		Release();
		return;
	}

	AARCharacterBase* Owner = Cast<AARCharacterBase>(GetOwner());
	UAbilitySystemComponent* ASC = Owner ? Owner->GetAbilitySystemComponent() : nullptr;
	AController* Controller = Owner ? Owner->GetController() : nullptr;
	if (!ASC || !Controller || Owner->IsDead() || ASC->HasMatchingGameplayTag(TAG_State_Parkouring.GetTag()))
	{
		return;
	}

	TArray<AAREnemyCharacter*> Enemies;
	TArray<ARLockOn::FCandidate> Candidates;
	for (TActorIterator<AAREnemyCharacter> It(GetWorld()); It; ++It)
	{
		AAREnemyCharacter* Enemy = *It;
		if (!ARTeam::IsHostile(Owner->GetGenericTeamId().GetId(), Enemy->GetGenericTeamId().GetId()))
		{
			continue;
		}
		ARLockOn::FCandidate Candidate;
		Candidate.Location = Enemy->GetActorLocation();
		Candidate.bAlive = !Enemy->IsDead();
		Candidate.bVisible = HasLineOfSight(*Enemy);
		Enemies.Add(Enemy);
		Candidates.Add(Candidate);
	}

	const int32 Picked = ARLockOn::PickTarget(Candidates, Owner->GetActorLocation(), Controller->GetControlRotation().Vector());
	if (Picked == INDEX_NONE)
	{
		return;
	}

	Target = Enemies[Picked];
	bLocked = true;
	OccludedSeconds = 0.f;
	DiedHandle = Enemies[Picked]->OnDied.AddUObject(this, &UARLockOnComponent::HandleTargetDied);
	Enemies[Picked]->SetLockedOn(true);
	ASC->AddLooseGameplayTag(TAG_State_LockedOn.GetTag());

	// Strafe: the body follows the controller (which turns toward the target) instead of the move direction.
	UCharacterMovementComponent* Movement = Owner->GetCharacterMovement();
	bSavedOrientToMovement = Movement->bOrientRotationToMovement;
	bSavedDesiredRotation = Movement->bUseControllerDesiredRotation;
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = true;

	SetComponentTickEnabled(true);
}

void UARLockOnComponent::Release()
{
	if (!bLocked)
	{
		return;
	}
	bLocked = false;
	SetComponentTickEnabled(false);

	if (AAREnemyCharacter* Enemy = Target.Get())
	{
		Enemy->OnDied.Remove(DiedHandle);
		Enemy->SetLockedOn(false);
	}
	DiedHandle.Reset();
	Target.Reset();

	if (AARCharacterBase* Owner = Cast<AARCharacterBase>(GetOwner()))
	{
		if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(TAG_State_LockedOn.GetTag());
		}
		if (UCharacterMovementComponent* Movement = Owner->GetCharacterMovement())
		{
			Movement->bOrientRotationToMovement = bSavedOrientToMovement;
			Movement->bUseControllerDesiredRotation = bSavedDesiredRotation;
		}
	}
}

void UARLockOnComponent::HandleTargetDied(AARCharacterBase* /*Died*/)
{
	Release();
}

void UARLockOnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Release();
	Super::EndPlay(EndPlayReason);
}

void UARLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AARCharacterBase* Owner = Cast<AARCharacterBase>(GetOwner());
	AAREnemyCharacter* Enemy = Target.Get();
	AController* Controller = Owner ? Owner->GetController() : nullptr;
	UAbilitySystemComponent* ASC = Owner ? Owner->GetAbilitySystemComponent() : nullptr;
	// The target is gone, the owner lost its controller or died, or a parkour move starts: parkour turns the body itself.
	if (!Enemy || !Controller || !ASC || Owner->IsDead() || ASC->HasMatchingGameplayTag(TAG_State_Parkouring.GetTag()))
	{
		Release();
		return;
	}

	const FVector Offset = Enemy->GetActorLocation() - Owner->GetActorLocation();
	OccludedSeconds = HasLineOfSight(*Enemy) ? 0.f : OccludedSeconds + DeltaTime;
	if (ARLockOn::ShouldRelease(static_cast<float>(Offset.Size()), OccludedSeconds, !Enemy->IsDead()))
	{
		Release();
		return;
	}

	FRotator Desired = Offset.Rotation();
	Desired.Pitch = FMath::Clamp(Desired.Pitch, -MaxPitchDegrees, MaxPitchDegrees);
	Controller->SetControlRotation(FMath::RInterpConstantTo(Controller->GetControlRotation(), Desired, DeltaTime, RotationSpeedDegrees));
}
