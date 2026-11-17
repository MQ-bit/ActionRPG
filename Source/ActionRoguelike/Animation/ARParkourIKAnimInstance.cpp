// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/ARParkourIKAnimInstance.h"

#include "Character/ARParkourIKComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/ARParkour.h"

namespace
{
	// shortcut: mannequin bone names and the heights measured on it (ankle joint ~8.5 cm and ball joint ~0.5 cm above the sole on flat ground); another skeleton needs its own.
	const FName FootBone[2] = {TEXT("foot_l"), TEXT("foot_r")};
	const FName BallBone[2] = {TEXT("ball_l"), TEXT("ball_r")};
	constexpr float AnkleHeightCm = 8.5f;
	constexpr float BallHeightCm = 0.5f;
	constexpr float MaxFootLiftCm = 20.f;
	constexpr float FootLiftSpeed = 25.f;
	constexpr float LedgeMarginCm = 12.f;
	// The lift counts as a full IK weight from this many cm up; below it the leg blends back to the animation.
	constexpr float FullWeightLiftCm = 2.f;
}

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

	const bool bActive = IK && IK->IsActive() && Main;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		UpdateFoot(Side, bActive ? IK : nullptr, DeltaSeconds);
	}
	if (!bActive)
	{
		return;
	}

	FVector Target;
	IK->GetHand(0, Main, Target, WeightL);
	HandTargetL = FTransform(Target);
	IK->GetHand(1, Main, Target, WeightR);
	HandTargetR = FTransform(Target);
}

void UARParkourIKAnimInstance::UpdateFoot(int32 Side, const UARParkourIKComponent* IK, float DeltaSeconds)
{
	float& Applied = FootLiftCm[Side];
	float& Weight = Side == 0 ? FootWeightL : FootWeightR;
	FTransform& Target = Side == 0 ? FootTargetL : FootTargetR;

	float Wanted = 0.f;
	const USkeletalMeshComponent* Mesh = GetOwningComponent();
	const UPrimitiveComponent* Ledge = IK ? IK->GetLedge() : nullptr;
	if (Mesh && Ledge)
	{
		// The pose read here is last frame's, with last frame's lift in it: FootLift adds the lift on top of that.
		const FVector Foot = Mesh->GetSocketLocation(FootBone[Side]);
		const FVector Ball = Mesh->GetSocketLocation(BallBone[Side]);
		// shortcut: the ledge counts as its world-aligned bounding box; a rotated or sloped top needs the real surface.
		const FBox Box = Ledge->Bounds.GetBox();
		// The box is widened so the lift starts before the foot crosses the edge, not on the frame it already is inside.
		const FBox2D Over(FVector2D(Box.Min) - FVector2D(LedgeMarginCm), FVector2D(Box.Max) + FVector2D(LedgeMarginCm));
		if (Over.IsInside(FVector2D(Ball)) || Over.IsInside(FVector2D(Foot)))
		{
			const float SoleZ = FMath::Min(Foot.Z - AnkleHeightCm, Ball.Z - BallHeightCm);
			// A foot far below the top (dangling in front of the wall) is left to the animation, a partial lift would only distort it.
			if (Box.Max.Z - SoleZ + Applied <= MaxFootLiftCm)
			{
				Wanted = ARParkour::FootLift(Applied, SoleZ, Box.Max.Z, MaxFootLiftCm);
			}
		}
		Applied = FMath::FInterpTo(Applied, Wanted, DeltaSeconds, FootLiftSpeed);
		// The rig adds this offset to this frame's animated foot, so the foot keeps following the animation sideways.
		Target = FTransform(FVector(0.f, 0.f, Applied));
	}
	else
	{
		Applied = FMath::FInterpTo(Applied, 0.f, DeltaSeconds, FootLiftSpeed);
		Target = FTransform::Identity;
	}
	Weight = FMath::Clamp(Applied / FullWeightLiftCm, 0.f, 1.f);
}
