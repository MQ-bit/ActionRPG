// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/ARRigUnit_ObstaclePush.h"

#include "Character/ARParkourIKComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Core/ARParkour.h"
#include "HAL/IConsoleManager.h"
#include "Rigs/RigHierarchy.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ARRigUnit_ObstaclePush)

namespace
{
	TAutoConsoleVariable<int32> CVarBodyPush(TEXT("ar.Parkour.BodyPush"), 1, TEXT("1 keeps limbs and the trunk out of the parkour obstacle (control rig unit AR Obstacle Push), 0 leaves them to the animation."));
}

FRigUnit_ARObstaclePush::FRigUnit_ARObstaclePush()
{
	// shortcut: mannequin bone names (the Survival character shares them); another skeleton edits the Segments pin of the node instead.
	Segments = {
		{TEXT("thigh_l"), TEXT("calf_l"), 9.f, true}, {TEXT("calf_l"), TEXT("foot_l"), 6.5f},
		{TEXT("thigh_r"), TEXT("calf_r"), 9.f, true}, {TEXT("calf_r"), TEXT("foot_r"), 6.5f},
		{TEXT("upperarm_l"), TEXT("lowerarm_l"), 5.f}, {TEXT("lowerarm_l"), TEXT("hand_l"), 4.f},
		{TEXT("upperarm_r"), TEXT("lowerarm_r"), 5.f}, {TEXT("lowerarm_r"), TEXT("hand_r"), 4.f},
		{TEXT("spine_02"), TEXT("spine_03"), 18.f, true}, {TEXT("spine_03"), TEXT("neck_01"), 18.f, true},
		{TEXT("neck_01"), TEXT("head"), 9.f},
	};
}

FRigUnit_ARObstaclePush_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
	if (!Hierarchy)
	{
		return;
	}
	if (Applied.Num() != Segments.Num())
	{
		Applied.Init(FVector::ZeroVector, Segments.Num());
	}
	const float DeltaTime = ExecuteContext.GetDeltaTime();

	// The obstacle of the running parkour move (none outside one: everything eases back to the animation).
	const AActor* Owner = ExecuteContext.GetOwningActor();
	if (!Owner && ExecuteContext.GetOwningComponent())
	{
		Owner = ExecuteContext.GetOwningComponent()->GetOwner();
	}
	const UARParkourIKComponent* IK = Owner ? Owner->FindComponentByClass<UARParkourIKComponent>() : nullptr;
	const UPrimitiveComponent* Ledge = IK && IK->IsActive() ? IK->GetLedge() : nullptr;
	FBox Box = Ledge ? Ledge->Bounds.GetBox() : FBox(ForceInit);
	bool bBoxValid = Ledge && Box.IsValid && (Box.Max - Box.Min).GetMin() > 0.1 && CVarBodyPush.GetValueOnAnyThread() != 0;
	if (bBoxValid)
	{
		LastBoxMin = Box.Min;
		LastBoxMax = Box.Max;
		HoldRemaining = HoldSeconds;
	}
	else if (HoldRemaining > 0.f && CVarBodyPush.GetValueOnAnyThread() != 0)
	{
		// The move has let go of the obstacle but its last frames are still next to it.
		HoldRemaining -= DeltaTime;
		Box = FBox(LastBoxMin, LastBoxMax);
		bBoxValid = true;
	}

	// The body first: the deepest trunk or hip point moves the whole body (the pelvis) out of the obstacle, the limbs are judged on the moved body afterwards.
	const FRigElementKey BodyKey(BodyBone, ERigElementType::Bone);
	if (MaxBodyPushCm > 0.f && Hierarchy->Contains(BodyKey))
	{
		FVector BodyWanted = FVector::ZeroVector;
		if (bBoxValid && Weight > 0.f)
		{
			float Deepest = 0.f;
			FVector DeepestNormal = FVector::ZeroVector;
			for (const FARObstaclePushSegment& Segment : Segments)
			{
				const FRigElementKey Key(Segment.Parent, ERigElementType::Bone);
				if (!Segment.bMovesBody || !Hierarchy->Contains(Key))
				{
					continue;
				}
				// The whole segment counts (the knee end of a thigh is where it goes in, not the hip joint).
				const FRigElementKey ChildOfBody(Segment.Child, ERigElementType::Bone);
				if (!Hierarchy->Contains(ChildOfBody))
				{
					continue;
				}
				FVector Normal;
				const float Depth = ARParkour::SegmentPenetration(ExecuteContext.ToWorldSpace(Hierarchy->GetGlobalTransform(Key).GetLocation()),
					ExecuteContext.ToWorldSpace(Hierarchy->GetGlobalTransform(ChildOfBody).GetLocation()), Segment.Radius, Box, Normal);
				if (Depth > Deepest)
				{
					Deepest = Depth;
					DeepestNormal = Normal;
				}
			}
			BodyWanted = DeepestNormal * FMath::Min(Deepest, MaxBodyPushCm) * Weight;
		}
		BodyApplied = FMath::VInterpConstantTo(BodyApplied, BodyWanted, DeltaTime, PushSpeed);
		if (BodyApplied.SizeSquared() > 0.0001)
		{
			FTransform BodyTransform = Hierarchy->GetGlobalTransform(BodyKey);
			const FVector Here = BodyTransform.GetLocation();
			const FVector Moved = ExecuteContext.ToVMSpace(ExecuteContext.ToWorldSpace(Here) + BodyApplied);
			BodyTransform.SetLocation(Moved);
			Hierarchy->SetGlobalTransform(BodyKey, BodyTransform, false, true);
		}
	}

	// From the trunk outwards (the order of Segments): a swung parent moves everything below it, the next segment is judged afterwards.
	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		const FARObstaclePushSegment& Segment = Segments[Index];
		const FRigElementKey ParentKey(Segment.Parent, ERigElementType::Bone);
		const FRigElementKey ChildKey(Segment.Child, ERigElementType::Bone);
		// The trunk and the thighs are handled by moving the body; swinging them would only move other parts of them into the obstacle.
		if (Segment.bMovesBody || !Hierarchy->Contains(ParentKey) || !Hierarchy->Contains(ChildKey))
		{
			continue;
		}

		FTransform ParentTransform = Hierarchy->GetGlobalTransform(ParentKey);
		const FVector ChildLocation = Hierarchy->GetGlobalTransform(ChildKey).GetLocation();
		const FVector ParentWorld = ExecuteContext.ToWorldSpace(ParentTransform.GetLocation());
		const FVector ChildWorld = ExecuteContext.ToWorldSpace(ChildLocation);

		FVector Wanted = FVector::ZeroVector;
		if (bBoxValid && Weight > 0.f)
		{
			FVector Normal;
			const float Depth = ARParkour::SegmentPenetration(ParentWorld, ChildWorld, Segment.Radius, Box, Normal);
			if (Depth > 0.f)
			{
				Wanted = Normal * FMath::Min(Depth, MaxPushCm) * Weight;
			}
		}
		FVector& Push = Applied[Index];
		Push = FMath::VInterpConstantTo(Push, Wanted, DeltaTime, PushSpeed);
		if (Push.SizeSquared() < 0.0001)
		{
			continue;
		}

		// Swing the parent so the child ends up Push further out: the rotation between the old and the new direction, in rig space.
		const FVector NewChildRig = ExecuteContext.ToVMSpace(ChildWorld + Push);
		const FVector OldDirection = (ChildLocation - ParentTransform.GetLocation()).GetSafeNormal();
		const FVector NewDirection = (NewChildRig - ParentTransform.GetLocation()).GetSafeNormal();
		if (OldDirection.IsNearlyZero() || NewDirection.IsNearlyZero())
		{
			continue;
		}
		ParentTransform.SetRotation((FQuat::FindBetweenNormals(OldDirection, NewDirection) * ParentTransform.GetRotation()).GetNormalized());
		Hierarchy->SetGlobalTransform(ParentKey, ParentTransform, false, true);
	}
}
