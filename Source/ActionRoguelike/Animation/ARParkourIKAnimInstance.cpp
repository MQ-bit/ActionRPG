// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/ARParkourIKAnimInstance.h"

#include "Character/ARParkourIKComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/ARParkour.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// Switches for comparing the extra IK against the plain animation (jitter and penetration checks); both on by default.
	// Off by default: it jitters the arms (measured 3-7x the plain animation). The push is judged from last frame's final pose, so an IK that does not hit its target exactly
	// skews the estimate and the error accumulates. A proper fix judges it on the rig's input pose.
	TAutoConsoleVariable<int32> CVarHandPush(TEXT("ar.Parkour.HandPush"), 0, TEXT("1 keeps the hands out of the parkour obstacle (jitters, see the source), 0 leaves them to the animation."));
	TAutoConsoleVariable<int32> CVarFootLift(TEXT("ar.Parkour.FootLift"), 1, TEXT("1 lifts the feet onto the ledge top of a parkour move, 0 leaves them to the animation."));

	// shortcut: mannequin bone names and the heights measured on it (ankle joint ~8.5 cm and ball joint ~0.5 cm above the sole on flat ground); another skeleton needs its own.
	const FName FootBone[2] = {TEXT("foot_l"), TEXT("foot_r")};
	const FName BallBone[2] = {TEXT("ball_l"), TEXT("ball_r")};
	constexpr float AnkleHeightCm = 8.5f;
	constexpr float BallHeightCm = 0.5f;
	constexpr float MaxFootLiftCm = 20.f;
	const FName HandBone[2] = {TEXT("hand_l"), TEXT("hand_r")};
	/** The hand is kept this far outside the obstacle: its thickness plus a margin, because the push shows one frame late. */
	constexpr float HandClearanceCm = 10.f;
	constexpr float HandPushSpeed = 80.f;
	// Full weight from a tiny push on: a partial weight moves the hand less than the push, which skews the estimate of where the animation has it and feeds back (jitter).
	constexpr float FullPushWeightCm = 0.25f;
	// Fast: the lift has to keep up with a foot crossing the edge in a few frames at 60 fps (a slow interpolation shows as a penetration).
	constexpr float FootLiftSpeed = 40.f;
	constexpr float LedgeMarginCm = 12.f;
	// The lift counts as a full IK weight from this many cm up (same reason as the hand push); below it the leg blends back to the animation.
	constexpr float FullWeightLiftCm = 0.25f;
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
	if (bActive)
	{
		FVector Target;
		IK->GetHand(0, Main, Target, WeightL);
		HandTargetL = FTransform(Target);
		IK->GetHand(1, Main, Target, WeightR);
		HandTargetR = FTransform(Target);
	}
	for (int32 Side = 0; Side < 2; ++Side)
	{
		UpdateFoot(Side, bActive ? IK : nullptr, DeltaSeconds);
		UpdateHandPush(Side, bActive ? IK : nullptr, DeltaSeconds);
	}
}

void UARParkourIKAnimInstance::UpdateHandPush(int32 Side, const UARParkourIKComponent* IK, float DeltaSeconds)
{
	FVector& Applied = HandPush[Side];
	float& Weight = Side == 0 ? WeightL : WeightR;
	FTransform& Target = Side == 0 ? HandTargetL : HandTargetR;

	FVector Wanted = FVector::ZeroVector;
	FVector Animated = FVector::ZeroVector;
	const USkeletalMeshComponent* Mesh = GetOwningComponent();
	const UPrimitiveComponent* Ledge = IK && CVarHandPush.GetValueOnGameThread() != 0 ? IK->GetLedge() : nullptr;
	if (Mesh && Ledge)
	{
		// The pose read here is last frame's, with last frame's push in it: taking that off gives where the animation has the hand.
		Animated = Mesh->GetSocketLocation(HandBone[Side]) - Applied;
		// shortcut: the obstacle counts as its world-aligned bounding box, like the feet; a rotated or sloped obstacle needs the real surface.
		const FBox Box = Ledge->Bounds.GetBox().ExpandBy(HandClearanceCm);
		if (Box.IsInside(Animated))
		{
			const double Exit[5] = {Animated.X - Box.Min.X, Box.Max.X - Animated.X, Animated.Y - Box.Min.Y, Box.Max.Y - Animated.Y, Box.Max.Z - Animated.Z};
			const FVector Out[5] = {FVector(-1, 0, 0), FVector(1, 0, 0), FVector(0, -1, 0), FVector(0, 1, 0), FVector(0, 0, 1)};
			int32 Nearest = 0;
			for (int32 Face = 1; Face < 5; ++Face)
			{
				Nearest = Exit[Face] < Exit[Nearest] ? Face : Nearest;
			}
			Wanted = Out[Nearest] * Exit[Nearest];
		}
	}
	Applied = FMath::VInterpTo(Applied, Wanted, DeltaSeconds, HandPushSpeed);

	// Blended with the ledge window by weight, so the target does not jump while the window ramps in or out.
	const float PushWeight = FMath::Clamp(static_cast<float>(Applied.Size()) / FullPushWeightCm, 0.f, 1.f);
	if (PushWeight > 0.f)
	{
		const FVector WindowTarget = Target.GetLocation();
		const float Total = Weight + PushWeight;
		Target = FTransform((WindowTarget * Weight + (Animated + Applied) * PushWeight) / Total);
		Weight = FMath::Max(Weight, PushWeight);
	}
}

void UARParkourIKAnimInstance::UpdateFoot(int32 Side, const UARParkourIKComponent* IK, float DeltaSeconds)
{
	float& Applied = FootLiftCm[Side];
	float& Weight = Side == 0 ? FootWeightL : FootWeightR;
	FTransform& Target = Side == 0 ? FootTargetL : FootTargetR;

	float Wanted = 0.f;
	const USkeletalMeshComponent* Mesh = GetOwningComponent();
	const UPrimitiveComponent* Ledge = IK && CVarFootLift.GetValueOnGameThread() != 0 ? IK->GetLedge() : nullptr;
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
