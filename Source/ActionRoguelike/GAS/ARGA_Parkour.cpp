// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ARGA_Parkour.h"

#include "ActionRoguelike.h"
#include "AbilitySystemComponent.h"
#include "Character/ARParkourIKComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "MotionWarpingComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GAS/ARAbilityTask_ParkourMove.h"
#include "GAS/ARGameplayTags.h"

namespace
{
	/** Trace height for the wall in front; anything lower is a step the capsule walks over. */
	constexpr float KneeHeight = 40.f;
	/** The top surface is looked for from this height above the feet; taller things are not climbable anyway. */
	constexpr float TopProbeHeight = ARParkour::MantleMaxHeight + 100.f;
	constexpr float ClearanceProbeLength = 400.f;
	/** Obstacles deeper than this count as "very thick"; only the vault limit (150) matters. */
	constexpr float MaxThicknessProbe = 400.f;
}

UARGA_Parkour::UARGA_Parkour()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	SetAssetTags(FGameplayTagContainer(TAG_Ability_Parkour.GetTag()));
	ActivationOwnedTags.AddTag(TAG_State_Parkouring.GetTag());
	ActivationBlockedTags.AddTag(TAG_State_Attacking.GetTag());
}

bool UARGA_Parkour::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	FProbeResult Result;
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) && Probe(ActorInfo, Result);
}

void UARGA_Parkour::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	FProbeResult Result;
	if (!Probe(ActorInfo, Result) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	Avatar->SetActorRotation(Result.Facing);
	BeginCameraOverride(Avatar);

	if (!TryStartMontage(Result))
	{
		StartInterpolatedMove(Result);
	}
}

void UARGA_Parkour::StartInterpolatedMove(const FProbeResult& Result)
{
	const float Duration = Result.Move == ARParkour::EMove::Vault ? VaultDuration : MantleDuration;
	UARAbilityTask_ParkourMove* Move = UARAbilityTask_ParkourMove::ParkourMove(this, Result.Target, Result.ApexZ, Duration);
	Move->OnFinished.AddDynamic(this, &UARGA_Parkour::OnMoveFinished);
	Move->OnBlocked.AddDynamic(this, &UARGA_Parkour::OnMoveBlocked);
	Move->ReadyForActivation();
}

bool UARGA_Parkour::TryStartMontage(const FProbeResult& Result)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UMotionWarpingComponent* Warp = Character ? Character->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	if (Clips.IsEmpty() || !Warp || !ASC || !AnimInstance)
	{
		return false;
	}

	TArray<ARParkour::FClipRule> Rules;
	Rules.Reserve(Clips.Num());
	for (const FARParkourClip& Clip : Clips)
	{
		Rules.Add({Clip.Move == EARParkourMoveKind::Mantle ? ARParkour::EMove::Mantle : ARParkour::EMove::Vault,
			Clip.MinHeight, Clip.MaxHeight, static_cast<ARParkour::EVariant>(Clip.Variant)});
	}
	const int32 Index = ARParkour::PickClip(Rules, Result.Move, Result.Height, ARParkour::VariantForSpeed(Character->GetVelocity().Size2D()));
	UAnimMontage* Montage = Index != INDEX_NONE ? Clips[Index].Montage.LoadSynchronous() : nullptr;
	if (!Montage)
	{
		return false;
	}

	// The warp target is where the feet end up (the warp notify warps to the feet location).
	Warp->AddOrUpdateWarpTargetFromLocationAndRotation(ARParkour::WarpTargetName(), Result.Target - FVector(0.f, 0.f, Result.HalfHeight), Result.Facing);
	if (ASC->PlayMontage(this, CurrentActivationInfo, Montage, 1.f, NAME_None, Clips[Index].StartPosition) <= 0.f)
	{
		// Could not start: nothing else has been touched yet, so the interpolated move can take over.
		Warp->RemoveWarpTarget(ARParkour::WarpTargetName());
		return false;
	}

	bMontageMove = true;
	MontageTarget = Result.Target;
	SafeLocation = Character->GetActorLocation();
	IgnoredObstacle = Result.Obstacle;
	WarpComponent = Warp;

	// Root motion moves the capsule: no gravity, and the obstacle must not block the sweep while it passes over or along it.
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->SetMovementMode(MOVE_Flying);
	if (AActor* Obstacle = IgnoredObstacle.Get())
	{
		Character->GetCapsuleComponent()->IgnoreActorWhenMoving(Obstacle, true);
	}

	FOnMontageEnded Ended;
	Ended.BindUObject(this, &UARGA_Parkour::OnMontageEnded, ++MontagePlaybackId);
	AnimInstance->Montage_SetEndDelegate(Ended, Montage);

	BeginHandIK(Result, Clips[Index], Montage, Character);

	Character->GetWorld()->GetTimerManager().SetTimer(SafeLocationTimer, this, &UARGA_Parkour::SampleSafeLocation, 1.f / 30.f, true);
	return true;
}

void UARGA_Parkour::OnMontageEnded(UAnimMontage* /*Montage*/, bool bInterrupted, int32 PlaybackId)
{
	if (!bMontageMove || PlaybackId != MontagePlaybackId || !IsActive())
	{
		return;
	}

	if (bInterrupted)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// The end event comes from the pose update, before the movement update applies that tick's root motion. Check the
	// arrival and clean up on the next tick, when the capsule has taken the last of the warped motion.
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (UWorld* World = Avatar ? Avatar->GetWorld() : nullptr)
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, PlaybackId]()
		{
			FinishMontageMove(PlaybackId);
		}));
	}
}

void UARGA_Parkour::FinishMontageMove(int32 PlaybackId)
{
	if (!bMontageMove || PlaybackId != MontagePlaybackId || !IsActive())
	{
		return;
	}

	// A finished montage is not an arrival: the capsule may have slid along something. Check where it really is.
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	// The warp leaves a few cm of error (the blend-out weakens the last root motion) and the capsule may touch the surface it
	// stands on, so the overlap test uses a slightly smaller capsule. An arrival is snapped onto the target, which the probe
	// has already checked to be free.
	const float Distance = Avatar ? FVector::Dist(Avatar->GetActorLocation(), MontageTarget) : -1.f;
	// The target was free when the move started; something may have moved in since, so check it again with the full capsule.
	const bool bArrived = Avatar && Distance <= ArrivalTolerance && !CapsuleBlockedAt(Avatar->GetActorLocation(), ArrivalInset)
		&& !CapsuleBlockedAt(MontageTarget);
	UE_LOG(LogAR, Verbose, TEXT("[Parkour] %s: montage ended %.1f cm from the target -> %s"),
		*GetNameSafe(Avatar), Distance, bArrived ? TEXT("arrived") : TEXT("not arrived"));
	if (bArrived)
	{
		GetAvatarActorFromActorInfo()->SetActorLocation(MontageTarget, false, nullptr, ETeleportType::TeleportPhysics);
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bArrived);
}

void UARGA_Parkour::SampleSafeLocation()
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (bMontageMove && Avatar && !CapsuleBlockedAt(Avatar->GetActorLocation()))
	{
		SafeLocation = Avatar->GetActorLocation();
	}
}

bool UARGA_Parkour::CapsuleBlockedAt(const FVector& Location, float Inset) const
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !Character->GetWorld())
	{
		return false;
	}
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ARParkourSafe), false, Character);
	return Character->GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() - Inset, Capsule->GetScaledCapsuleHalfHeight() - Inset), Params);
}

void UARGA_Parkour::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	CleanUpMontageMove(!bWasCancelled);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// After the base class: it clears every timer of this ability, which would delete the camera restore timer set here.
	EndCameraOverride();
}

void UARGA_Parkour::BeginHandIK(const FProbeResult& Result, const FARParkourClip& Clip, UAnimMontage* Montage, ACharacter* Character)
{
	UARParkourIKComponent* IK = Character ? Character->FindComponentByClass<UARParkourIKComponent>() : nullptr;
	UPrimitiveComponent* Top = Result.ObstacleComponent.Get();
	if (!IK || !Top)
	{
		return;
	}

	// A clip without hand windows (the vaults) only gets the feet.
	const float Windows[2][2] = {{Clip.LeftHandStart, Clip.LeftHandEnd}, {Clip.RightHandStart, Clip.RightHandEnd}};
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (Windows[Side][1] <= Windows[Side][0])
		{
			continue;
		}

		// A hand target must lie on the obstacle's own top surface: found by tracing that component, moved further onto the
		// top once if it misses (near a corner), and left out if the top is not there.
		const float InwardTries[2] = {HandInwardCm, HandInwardCm + 20.f};
		for (const float Inward : InwardTries)
		{
			FVector Target = ARParkour::HandCandidate(Result.FrontEdgePoint, Result.Inward, Side == 0 ? -1 : 1, HandLateralCm, Inward);
			FHitResult Hit;
			if (Top->LineTraceComponent(Hit, Target + FVector(0.f, 0.f, 30.f), Target - FVector(0.f, 0.f, 30.f), FCollisionQueryParams()) && Hit.ImpactNormal.Z >= 0.7f)
			{
				Target.Z = Hit.ImpactPoint.Z + HandHeightCm;
				IK->SetHand(Side, Target, Windows[Side][0], Windows[Side][1]);
				break;
			}
		}
	}

	// Always started: the feet use the ledge even when no hand window is set.
	IK->SetLedge(Top);
	IK->Begin(Montage, Clip.HandRamp);
	HandIKComponent = IK;
}

void UARGA_Parkour::EndHandIK()
{
	if (UARParkourIKComponent* IK = HandIKComponent.Get())
	{
		IK->End();
	}
	HandIKComponent.Reset();
}

void UARGA_Parkour::BeginCameraOverride(AActor* Avatar)
{
	USpringArmComponent* Boom = Avatar ? Avatar->FindComponentByClass<USpringArmComponent>() : nullptr;
	if (!Boom)
	{
		return;
	}
	if (!OverriddenBoom.IsValid())
	{
		// Not a second move while the collision test is still waiting to come back: keep the original values.
		OverriddenBoom = Boom;
		bSavedCollisionTest = Boom->bDoCollisionTest;
		bSavedCameraLag = Boom->bEnableCameraLag;
		SavedLagSpeed = Boom->CameraLagSpeed;
	}
	Boom->GetWorld()->GetTimerManager().ClearTimer(CameraRestoreTimer);

	Boom->bDoCollisionTest = false;
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = CameraLagSpeed;
}

void UARGA_Parkour::EndCameraOverride()
{
	USpringArmComponent* Boom = OverriddenBoom.Get();
	if (!Boom)
	{
		OverriddenBoom.Reset();
		return;
	}

	// The lag was for the move. The collision test comes back once the camera's spot is free of geometry: right after a vault
	// the player stands just behind the wall and the camera belongs on its other side, so restoring it at once would pull
	// the camera in to a few cm from the character.
	Boom->bEnableCameraLag = bSavedCameraLag;
	Boom->CameraLagSpeed = SavedLagSpeed;
	CameraRestoreElapsed = 0.f;
	Boom->GetWorld()->GetTimerManager().SetTimer(CameraRestoreTimer, this, &UARGA_Parkour::TryRestoreCameraCollision, 0.1f, true);
}

void UARGA_Parkour::TryRestoreCameraCollision()
{
	USpringArmComponent* Boom = OverriddenBoom.Get();
	if (!Boom)
	{
		return;
	}
	UWorld* World = Boom->GetWorld();
	CameraRestoreElapsed += 0.1f;

	// Where the boom puts the camera: it follows the control rotation (GetTargetRotation), not the component's own rotation.
	const FRotator Rotation = Boom->GetTargetRotation();
	const FVector Pivot = Boom->GetComponentLocation() + Boom->TargetOffset;
	const FVector Desired = Pivot - Rotation.Vector() * Boom->TargetArmLength + Rotation.RotateVector(Boom->SocketOffset);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ARParkourCamera), false, Boom->GetOwner());
	const bool bBlocked = World->SweepSingleByChannel(Hit, Pivot, Desired, FQuat::Identity, Boom->ProbeChannel,
		FCollisionShape::MakeSphere(Boom->ProbeSize), Params);
	if (!bBlocked || CameraRestoreElapsed >= MaxCameraRestoreSeconds)
	{
		UE_LOG(LogAR, Verbose, TEXT("[Parkour] camera collision restored after %.1f s (%s)"), CameraRestoreElapsed, bBlocked ? TEXT("time limit") : TEXT("path clear"));
		Boom->bDoCollisionTest = bSavedCollisionTest;
		OverriddenBoom.Reset();
		World->GetTimerManager().ClearTimer(CameraRestoreTimer);
	}
}

void UARGA_Parkour::CleanUpMontageMove(bool bArrived)
{
	if (!bMontageMove)
	{
		return;
	}
	bMontageMove = false;
	++MontagePlaybackId; // montage callbacks still queued from this move must not touch a later one

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}
	if (UWorld* World = Character->GetWorld())
	{
		World->GetTimerManager().ClearTimer(SafeLocationTimer);
	}

	// Cut short: stop the montage first so no root motion is left to move the capsule, then leave any geometry it is inside.
	if (!bArrived)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->CurrentMontageStop(0.f);
		}
		if (CapsuleBlockedAt(Character->GetActorLocation()))
		{
			Character->SetActorLocation(SafeLocation, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	if (AActor* Obstacle = IgnoredObstacle.Get())
	{
		Character->GetCapsuleComponent()->IgnoreActorWhenMoving(Obstacle, false);
	}
	IgnoredObstacle.Reset();
	if (UMotionWarpingComponent* Warp = WarpComponent.Get())
	{
		Warp->RemoveWarpTarget(ARParkour::WarpTargetName());
	}
	WarpComponent.Reset();
	EndHandIK();

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->Velocity = FVector::ZeroVector;
	Movement->SetMovementMode(bArrived ? MOVE_Walking : MOVE_Falling);
}

void UARGA_Parkour::OnMoveFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UARGA_Parkour::OnMoveBlocked()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

bool UARGA_Parkour::Probe(const FGameplayAbilityActorInfo* ActorInfo, FProbeResult& Out) const
{
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Movement || !World || !Movement->IsMovingOnGround())
	{
		return false;
	}

	const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Origin = Character->GetActorLocation();
	const FVector Forward = Character->GetActorForwardVector().GetSafeNormal2D();
	const float FeetZ = Origin.Z - HalfHeight;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ARParkourProbe), false, Character);
	auto Trace = [&](FHitResult& Hit, const FVector& From, const FVector& To)
	{
		return World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params);
	};
	auto Fits = [&](const FVector& Center)
	{
		return !World->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);
	};

	// Capsule centre height for standing on a surface point: on a slope the rounded bottom sits higher than on flat ground.
	auto CenterZOn = [&](const FHitResult& Support)
	{
		return Support.ImpactPoint.Z + HalfHeight - Radius + Radius / FMath::Max(Support.ImpactNormal.Z, 0.7f) + 2.f;
	};

	// 1) A near-vertical surface in front. Floors and ramps are not obstacles.
	FHitResult Wall;
	const FVector WallStart(Origin.X, Origin.Y, FeetZ + KneeHeight);
	if (!Trace(Wall, WallStart, WallStart + Forward * DetectDistance) || FMath::Abs(Wall.ImpactNormal.Z) > 0.5f)
	{
		return false;
	}

	// 2) The top surface, found from above a little inside the obstacle. Starting inside geometry means it is far too tall.
	const FVector Inside = Wall.ImpactPoint + Forward * 5.f;
	FHitResult Top;
	if (!Trace(Top, FVector(Inside.X, Inside.Y, FeetZ + TopProbeHeight), FVector(Inside.X, Inside.Y, FeetZ - 10.f)) || Top.bStartPenetrating)
	{
		return false;
	}
	// A slab or ceiling above the obstacle can be hit first; the top must belong to the same component as the wall.
	if (Top.GetComponent() != Wall.GetComponent())
	{
		return false;
	}
	const float TopZ = Top.ImpactPoint.Z;

	ARParkour::FObstacle Obstacle;
	Obstacle.Height = TopZ - FeetZ;
	Obstacle.SurfaceSlopeDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Top.ImpactNormal.Z, -1.f, 1.f)));

	FHitResult Ceiling;
	const FVector TopPoint = Top.ImpactPoint + FVector(0.f, 0.f, 2.f);
	Obstacle.ClearanceAbove = Trace(Ceiling, TopPoint, TopPoint + FVector(0.f, 0.f, ClearanceProbeLength)) ? Ceiling.Distance : ClearanceProbeLength;

	// 3) How deep the obstacle is: trace from beyond the far side back to the wall; the first hit is the back face.
	// Anything the trace does not resolve to this obstacle counts as very thick, so it is never vaulted.
	FVector Landing = FVector::ZeroVector;
	// The highest top surface the capsule has to clear: the near edge, or the back edge when the top rises.
	float ClearTopZ = TopZ;
	Obstacle.Thickness = MaxThicknessProbe;
	{
		const float ProbeZ = TopZ - 5.f;

		// Start just past this obstacle's own bounds, so another obstacle close behind it is not taken for part of it.
		const FBox Bounds = Wall.GetComponent()->Bounds.GetBox();
		float FarDistance = 0.f;
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector P((Corner & 1) ? Bounds.Max.X : Bounds.Min.X, (Corner & 2) ? Bounds.Max.Y : Bounds.Min.Y, (Corner & 4) ? Bounds.Max.Z : Bounds.Min.Z);
			FarDistance = FMath::Max(FarDistance, FVector::DotProduct(P - Inside, Forward));
		}
		FarDistance += 5.f;

		const FVector Far = Inside + Forward * FarDistance;
		FHitResult Back;
		if (FarDistance <= MaxThicknessProbe
			&& Trace(Back, FVector(Far.X, Far.Y, ProbeZ), FVector(Inside.X, Inside.Y, ProbeZ))
			&& !Back.bStartPenetrating && Back.GetComponent() == Wall.GetComponent()
			// A sloped top can be hit instead of the back face; only a near-vertical face pointing away counts.
			&& FMath::Abs(Back.ImpactNormal.Z) <= 0.5f && FVector::DotProduct(Back.ImpactNormal, Forward) > 0.f)
		{
			Obstacle.Thickness = FVector::DotProduct(Back.ImpactPoint - Wall.ImpactPoint, Forward);

			const FVector Edge = Back.ImpactPoint - Forward * 5.f;
			FHitResult EdgeTop;
			if (Trace(EdgeTop, FVector(Edge.X, Edge.Y, FeetZ + TopProbeHeight), FVector(Edge.X, Edge.Y, FeetZ - 10.f))
				&& EdgeTop.GetComponent() == Wall.GetComponent())
			{
				ClearTopZ = FMath::Max(TopZ, EdgeTop.ImpactPoint.Z);
			}

			// The vault landing: walkable floor where the capsule actually ends up, one radius behind the back face.
			const FVector Spot = Back.ImpactPoint + Forward * (Radius + 5.f);
			FHitResult Support;
			Obstacle.bLandingBehind = Trace(Support, FVector(Spot.X, Spot.Y, TopZ + 20.f), FVector(Spot.X, Spot.Y, FeetZ - 100.f))
				&& Support.ImpactNormal.Z >= 0.7f;
			if (Obstacle.bLandingBehind)
			{
				Landing = FVector(Spot.X, Spot.Y, CenterZOn(Support));
			}
		}
	}

	const ARParkour::EMove Move = ARParkour::Classify(Obstacle);
	UE_LOG(LogAR, Verbose, TEXT("[Parkour] %s: height %.0f thickness %.0f clearance %.0f slope %.0f landing %d -> %d"),
		*GetNameSafe(Character), Obstacle.Height, Obstacle.Thickness, Obstacle.ClearanceAbove, Obstacle.SurfaceSlopeDegrees,
		Obstacle.bLandingBehind ? 1 : 0, (int32)Move);

	// 4) Where the capsule ends up; the spot must be free.
	FVector Target;
	float ApexZ;
	switch (Move)
	{
	case ARParkour::EMove::Vault:
		Target = Landing;
		ApexZ = ClearTopZ + HalfHeight + 5.f;
		break;
	case ARParkour::EMove::Mantle:
	{
		// The capsule centre must stand on this obstacle: a narrow pillar or the far side of a slope has no support here.
		const FVector Spot = Wall.ImpactPoint + Forward * (Radius + 10.f);
		FHitResult Support;
		if (!Trace(Support, FVector(Spot.X, Spot.Y, FeetZ + TopProbeHeight), FVector(Spot.X, Spot.Y, FeetZ - 10.f))
			|| Support.GetComponent() != Wall.GetComponent() || Support.ImpactNormal.Z < 0.7f
			|| Support.ImpactPoint.Z - FeetZ > ARParkour::MantleMaxHeight + 50.f)
		{
			return false;
		}
		Target = FVector(Spot.X, Spot.Y, CenterZOn(Support));
		// A top that falls away from the player is crossed at the near edge's height, not the destination's.
		ApexZ = FMath::Max(Target.Z, TopZ + HalfHeight + 5.f);
		break;
	}
	default:
		return false;
	}
	if (!Fits(Target))
	{
		return false;
	}

	Out.Move = Move;
	Out.Target = Target;
	Out.ApexZ = ApexZ;
	Out.Height = Obstacle.Height;
	Out.HalfHeight = HalfHeight;
	Out.Obstacle = Wall.GetActor();
	Out.FrontEdgePoint = FVector(Wall.ImpactPoint.X, Wall.ImpactPoint.Y, TopZ);
	Out.Inward = Forward;
	Out.ObstacleComponent = Wall.GetComponent();
	Out.Facing = FRotator(0.f, (-Wall.ImpactNormal).Rotation().Yaw, 0.f);
	return true;
}
