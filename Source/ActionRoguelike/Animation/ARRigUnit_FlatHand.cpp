// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/ARRigUnit_FlatHand.h"

#include "Character/ARParkourIKComponent.h"
#include "Rigs/RigHierarchy.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ARRigUnit_FlatHand)

namespace
{
	/** A right-handed frame (X forward, Y across, Z normal) from a forward and a normal that are not quite perpendicular. */
	bool MakeFrame(const FVector& Forward, const FVector& Normal, FMatrix& OutFrame)
	{
		const FVector X = Forward.GetSafeNormal();
		const FVector Z = (Normal - X * FVector::DotProduct(Normal, X)).GetSafeNormal();
		if (X.IsNearlyZero() || Z.IsNearlyZero())
		{
			return false;
		}
		const FVector Y = FVector::CrossProduct(Z, X); // Z = X x Y  =>  Y = Z x X
		OutFrame = FMatrix(X, Y, Z, FVector::ZeroVector);
		return true;
	}
}

FRigUnit_ARFlatHandRotation_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	Result = Animated;
	URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
	const float DeltaTime = ExecuteContext.GetDeltaTime();
	SmoothedWeight = FMath::FInterpConstantTo(SmoothedWeight, FMath::Clamp(Weight, 0.f, 1.f), DeltaTime, FMath::Max(WeightSpeed, 0.01f));
	if (!Hierarchy || SmoothedWeight <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// The obstacle direction of the running move: the fingers point into it.
	const AActor* Owner = ExecuteContext.GetOwningActor();
	if (!Owner && ExecuteContext.GetOwningComponent())
	{
		Owner = ExecuteContext.GetOwningComponent()->GetOwner();
	}
	const UARParkourIKComponent* IK = Owner ? Owner->FindComponentByClass<UARParkourIKComponent>() : nullptr;
	FVector Inward = IK ? IK->GetInward() : FVector::ZeroVector;
	if (Inward.IsNearlyZero())
	{
		return;
	}

	const FRigElementKey Hand(HandBone, ERigElementType::Bone);
	const FRigElementKey Middle(MiddleBone, ERigElementType::Bone);
	const FRigElementKey Index(IndexBone, ERigElementType::Bone);
	const FRigElementKey Pinky(PinkyBone, ERigElementType::Bone);
	if (!Hierarchy->Contains(Hand) || !Hierarchy->Contains(Middle) || !Hierarchy->Contains(Index) || !Hierarchy->Contains(Pinky))
	{
		return;
	}

	// The hand's own axes, from the initial pose: the finger direction and the knuckle plane, in the hand's frame.
	const FTransform HandInitial = Hierarchy->GetGlobalTransform(Hand, true);
	const FVector Finger = HandInitial.InverseTransformVector(Hierarchy->GetGlobalTransform(Middle, true).GetLocation() - HandInitial.GetLocation());
	const FVector Across = HandInitial.InverseTransformVector(Hierarchy->GetGlobalTransform(Index, true).GetLocation() - Hierarchy->GetGlobalTransform(Pinky, true).GetLocation());
	FMatrix Local;
	if (!MakeFrame(Finger, FVector::CrossProduct(Finger.GetSafeNormal(), Across.GetSafeNormal()), Local))
	{
		return;
	}

	// The flat hand in rig space: fingers along the obstacle direction (horizontal), the palm normal up (left hand) or down (right).
	Inward.Z = 0.0;
	const FVector Origin = FVector::ZeroVector;
	const FVector ForwardRig = (ExecuteContext.ToVMSpace(Inward.GetSafeNormal()) - ExecuteContext.ToVMSpace(Origin)).GetSafeNormal();
	const FVector UpRig = (ExecuteContext.ToVMSpace(FVector(0.0, 0.0, bPalmNormalUp ? 1.0 : -1.0)) - ExecuteContext.ToVMSpace(Origin)).GetSafeNormal();
	FMatrix Desired;
	if (!MakeFrame(ForwardRig, UpRig, Desired))
	{
		return;
	}

	// Rotation that turns the hand's own frame into the desired one.
	const FQuat Flat = (FQuat(Desired) * FQuat(Local).Inverse()).GetNormalized();
	Result = FQuat::Slerp(Animated, Flat, SmoothedWeight).GetNormalized();
}
