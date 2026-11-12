// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ARAbilityTask_ParkourMove.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UARAbilityTask_ParkourMove::UARAbilityTask_ParkourMove()
{
	bTickingTask = true;
}

UARAbilityTask_ParkourMove* UARAbilityTask_ParkourMove::ParkourMove(UGameplayAbility* OwningAbility, const FVector& Target, float ApexZ, float Duration)
{
	UARAbilityTask_ParkourMove* Task = NewAbilityTask<UARAbilityTask_ParkourMove>(OwningAbility);
	Task->Target = Target;
	Task->ApexZ = ApexZ;
	Task->Duration = FMath::Max(Duration, 0.01f);
	return Task;
}

void UARAbilityTask_ParkourMove::Activate()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	if (!Character)
	{
		Finish(false);
		return;
	}

	const FVector Start = Character->GetActorLocation();
	const float Apex = FMath::Max3<double>(ApexZ, Start.Z, Target.Z);
	Waypoints[0] = Start;
	Waypoints[1] = FVector(Start.X, Start.Y, Apex);
	Waypoints[2] = FVector(Target.X, Target.Y, Apex);
	Waypoints[3] = Target;

	TotalLength = 0.f;
	for (int32 i = 0; i < 3; ++i)
	{
		SegmentLength[i] = FVector::Dist(Waypoints[i], Waypoints[i + 1]);
		TotalLength += SegmentLength[i];
	}

	// Custom mode with no custom physics: the movement component stops moving the character, this task does.
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Custom, 0);
}

FVector UARAbilityTask_ParkourMove::PositionAtDistance(float Distance) const
{
	for (int32 i = 0; i < 3; ++i)
	{
		if (Distance <= SegmentLength[i] || i == 2)
		{
			const float Fraction = SegmentLength[i] > KINDA_SMALL_NUMBER ? Distance / SegmentLength[i] : 1.f;
			return FMath::Lerp(Waypoints[i], Waypoints[i + 1], FMath::Clamp(Fraction, 0.f, 1.f));
		}
		Distance -= SegmentLength[i];
	}
	return Target;
}

void UARAbilityTask_ParkourMove::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	if (!Character)
	{
		Finish(false);
		return;
	}

	Elapsed += DeltaTime;
	const float Alpha = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);

	// Sweep to every corner crossed this frame before going further, so a long frame cannot cut across a corner.
	const float Goal = Alpha * TotalLength;
	const float Corners[2] = {SegmentLength[0], SegmentLength[0] + SegmentLength[1]};
	while (Travelled < Goal)
	{
		float Next = Goal;
		for (const float Corner : Corners)
		{
			if (Corner > Travelled + KINDA_SMALL_NUMBER && Corner < Next)
			{
				Next = Corner;
			}
		}

		FHitResult Hit;
		Character->SetActorLocation(PositionAtDistance(Next), true, &Hit);
		if (Hit.bBlockingHit)
		{
			Finish(false);
			return;
		}
		Travelled = Next;
	}

	if (Alpha >= 1.f)
	{
		Finish(true);
	}
}

void UARAbilityTask_ParkourMove::Finish(bool bArrived)
{
	if (bArrived)
	{
		if (ACharacter* Character = Cast<ACharacter>(GetAvatarActor()))
		{
			// Walking finds the floor on the next tick; the character is grounded when the ability ends.
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		}
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		(bArrived ? OnFinished : OnBlocked).Broadcast();
	}
	EndTask();
}

void UARAbilityTask_ParkourMove::OnDestroy(bool bInAbilityEnded)
{
	// Cut short (cancelled, blocked, dead): fall from the current, free position.
	if (const ACharacter* Character = Cast<ACharacter>(GetAvatarActor()))
	{
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		if (Movement && Movement->MovementMode == MOVE_Custom)
		{
			Movement->SetMovementMode(MOVE_Falling);
		}
	}

	Super::OnDestroy(bInAbilityEnded);
}
