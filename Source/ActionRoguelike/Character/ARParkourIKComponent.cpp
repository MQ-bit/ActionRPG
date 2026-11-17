// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/ARParkourIKComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Core/ARParkour.h"

UARParkourIKComponent::UARParkourIKComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UARParkourIKComponent::SetHand(int32 Side, const FVector& WorldTarget, float InWindowStart, float InWindowEnd)
{
	if (Side == 0 || Side == 1)
	{
		Targets[Side] = WorldTarget;
		WindowStart[Side] = InWindowStart;
		WindowEnd[Side] = InWindowEnd;
	}
}

void UARParkourIKComponent::Begin(UAnimMontage* InMontage, float InRamp)
{
	Montage = InMontage;
	Ramp = InRamp;
	bActive = InMontage != nullptr;
}

void UARParkourIKComponent::End()
{
	bActive = false;
	Ledge.Reset();
	Montage.Reset();
	for (int32 Side = 0; Side < 2; ++Side)
	{
		WindowStart[Side] = 0.f;
		WindowEnd[Side] = 0.f;
	}
}

void UARParkourIKComponent::GetHand(int32 Side, const UAnimInstance* MainAnimInstance, FVector& OutWorldTarget, float& OutWeight) const
{
	OutWeight = 0.f;
	OutWorldTarget = FVector::ZeroVector;

	UAnimMontage* PlayingMontage = Montage.Get();
	if (!bActive || (Side != 0 && Side != 1) || !MainAnimInstance || !PlayingMontage || !MainAnimInstance->Montage_IsPlaying(PlayingMontage))
	{
		return;
	}

	OutWorldTarget = Targets[Side];
	OutWeight = ARParkour::HandWeight(MainAnimInstance->Montage_GetPosition(PlayingMontage), WindowStart[Side], WindowEnd[Side], Ramp);
}
