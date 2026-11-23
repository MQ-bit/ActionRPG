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
	TAutoConsoleVariable<int32> CVarFootWall(TEXT("ar.Parkour.FootWall"), 1, TEXT("1 puts the feet against the wall face while the hands hold the ledge, 0 leaves them dangling."));
	TAutoConsoleVariable<int32> CVarFootLift(TEXT("ar.Parkour.FootLift"), 1, TEXT("1 lifts the feet onto the ledge top of a parkour move, 0 leaves them to the animation."));

	// shortcut: mannequin bone names (the heights of the joints above the sole are properties, set per character in the Blueprint).
	const FName FootBone[2] = {TEXT("foot_l"), TEXT("foot_r")};
	const FName BallBone[2] = {TEXT("ball_l"), TEXT("ball_r")};
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
	/** In the last part of the montage a foot hovering above the ledge top (the retargeted clips end with the feet up to 13 cm above it) is pulled down onto it, by at most this much. */
	constexpr float MaxFootPlantCm = 15.f;
	/** Feet within this distance of the wall face (and below the top) are put against it, at FootWallRadiusCm from the face (the ball joint is about that far above the sole). */
	constexpr float FootWallReachCm = 60.f;
	constexpr float FootWallRadiusCm = 6.f;
	/** The ankle joint is this far from the wall face when the foot is against it (cm): the ankle is about as high as the calf is thick. */
	constexpr float FootWallAnkleRadiusCm = 9.f;
	/** cm per second (a constant speed: a rate-style interpolation with a speed this high moves the whole way in one frame, a pop of 40 cm). */
	constexpr float FootWallSpeed = 150.f;
	/** The feet only go to the wall while a hand holds the ledge with at least this weight. */
	constexpr float FootWallHandWeight = 0.3f;
	constexpr float FootPlantPhaseSeconds = 0.5f;
	/** A foot counts as over the top surface when it is this far inside the box outline. */
	constexpr float FootPlantInsetCm = 10.f;
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
	FVector& Wall = FootWallCm[Side];
	float& Weight = Side == 0 ? FootWeightL : FootWeightR;
	FTransform& Target = Side == 0 ? FootTargetL : FootTargetR;

	float Wanted = 0.f;
	FVector WallWanted = FVector::ZeroVector;
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
			// How far below the top the animated foot is without any lift (negative: it hovers above the top).
			const float Below = Box.Max.Z - SoleZ + Applied;
			// A foot over the top surface (not just at its edge) is pulled down onto it all through the move: with the body crossing the
			// ledge the retargeted clips keep the feet 15-20 cm above it, which reads as floating. In the last part of the montage any
			// foot over the box counts.
			const FBox2D Inner(FVector2D(Box.Min) + FVector2D(FootPlantInsetCm), FVector2D(Box.Max) - FVector2D(FootPlantInsetCm));
			const bool bOverTop = Inner.bIsValid && Inner.IsInside(FVector2D(Ball));
			const float Lowest = (bOverTop || IK->InFinalPhase(Mesh->GetAnimInstance(), FootPlantPhaseSeconds)) ? -MaxFootPlantCm : 0.f;
			if (Below <= MaxFootLiftCm && Below >= Lowest)
			{
				Wanted = ARParkour::FootLift(Applied, SoleZ, Box.Max.Z, MaxFootLiftCm, Lowest);
			}
		}

		// The feet go against the wall face instead of dangling in front of it (while a hand holds the ledge) and are never inside it (always:
		// the leg swings up into the wall before a hand has reached the ledge).
		const FVector Inward = IK->GetInward();
		const bool bHandsHold = FMath::Max(WeightL, WeightR) > FootWallHandWeight;
		if (CVarFootWall.GetValueOnGameThread() != 0 && !Inward.IsNearlyZero())
		{
			// The front face of the box: perpendicular to the dominant axis of the move direction.
			const bool bAlongX = FMath::Abs(Inward.X) >= FMath::Abs(Inward.Y);
			const double FaceCoord = bAlongX ? (Inward.X > 0.0 ? Box.Min.X : Box.Max.X) : (Inward.Y > 0.0 ? Box.Min.Y : Box.Max.Y);
			const FVector Axis = bAlongX ? FVector(FMath::Sign(Inward.X), 0.0, 0.0) : FVector(0.0, FMath::Sign(Inward.Y), 0.0);
			const double BallCoord = bAlongX ? Ball.X : Ball.Y;
			const double AnkleCoord = bAlongX ? Foot.X : Foot.Y;
			const double AcrossCoord = bAlongX ? Ball.Y : Ball.X;
			const double AcrossMin = bAlongX ? Box.Min.Y : Box.Min.X;
			const double AcrossMax = bAlongX ? Box.Max.Y : Box.Max.X;
			// Gap between the ball joint and the face along the move direction: positive = in front of the wall, negative = inside it.
			const double Direction = bAlongX ? FMath::Sign(Inward.X) : FMath::Sign(Inward.Y);
			const float GapBall = static_cast<float>((FaceCoord - BallCoord) * Direction);
			// The ankle is thicker than the ball joint: the foot stands against the wall by whichever of the two is nearer to it.
			const float GapAnkle = static_cast<float>((FaceCoord - AnkleCoord) * Direction) - (FootWallAnkleRadiusCm - FootWallRadiusCm);
			const float Gap = FMath::Min(GapBall, GapAnkle);
			// A foot that has gone past the back face (the landing of a vault) is behind the obstacle, not in front of or inside it.
			const double BackCoord = bAlongX ? (Inward.X > 0.0 ? Box.Max.X : Box.Min.X) : (Inward.Y > 0.0 ? Box.Max.Y : Box.Min.Y);
			const bool bBeforeBack = (BackCoord - BallCoord) * Direction > 0.0;
			const bool bOnTheFace = bBeforeBack && AcrossCoord >= AcrossMin - LedgeMarginCm && AcrossCoord <= AcrossMax + LedgeMarginCm
				&& Ball.Z < Box.Max.Z - 20.f && Ball.Z > Box.Min.Z + 25.f;
			if (bOnTheFace && ((bHandsHold && Gap <= FootWallReachCm) || Gap < FootWallRadiusCm))
			{
				WallWanted = Wall + Axis * (Gap - FootWallRadiusCm);
				// A foot in the wall comes out at once (a swinging leg is in it for only a few frames, the speed limit below would be too slow);
				// only the pull towards the wall is smoothed.
				if (Gap < FootWallRadiusCm)
				{
					Wall = WallWanted;
				}
			}
		}
		Applied = FMath::FInterpTo(Applied, Wanted, DeltaSeconds, FootLiftSpeed);
		Wall = FMath::VInterpConstantTo(Wall, WallWanted, DeltaSeconds, FootWallSpeed);
		// The rig adds this offset to this frame's animated foot in component space (it reads the variable without converting it from
		// the world), so the foot keeps following the animation otherwise: the world offset is turned into the mesh's space here.
		Target = FTransform(Mesh->GetComponentTransform().InverseTransformVectorNoScale(FVector(Wall.X, Wall.Y, Applied)));
	}
	else
	{
		Applied = FMath::FInterpTo(Applied, 0.f, DeltaSeconds, FootLiftSpeed);
		Wall = FMath::VInterpConstantTo(Wall, FVector::ZeroVector, DeltaSeconds, FootWallSpeed);
		Target = FTransform::Identity;
	}
	Weight = FMath::Clamp(FMath::Max(FMath::Abs(Applied), static_cast<float>(Wall.Size())) / FullWeightLiftCm, 0.f, 1.f);
}
