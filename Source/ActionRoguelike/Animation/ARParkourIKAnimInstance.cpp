// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/ARParkourIKAnimInstance.h"

#include "Character/ARParkourIKComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UARParkourIKAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	WeightL = 0.f;
	WeightR = 0.f;

	const AActor* Owner = GetOwningActor();
	const UARParkourIKComponent* IK = Owner ? Owner->FindComponentByClass<UARParkourIKComponent>() : nullptr;
	const USkeletalMeshComponent* Mesh = GetOwningComponent();
	// The post-process instance is not the mesh's main instance; the montage plays in the main one.
	const UAnimInstance* Main = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!IK || !IK->IsActive() || !Main)
	{
		return;
	}

	FVector Target;
	IK->GetHand(0, Main, Target, WeightL);
	HandTargetL = FTransform(Target);
	IK->GetHand(1, Main, Target, WeightR);
	HandTargetR = FTransform(Target);
}
