// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ARGA_Parkour.h"

#include "ActionRoguelike.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
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

	const float Duration = Result.Move == ARParkour::EMove::Vault ? VaultDuration : MantleDuration;
	UARAbilityTask_ParkourMove* Move = UARAbilityTask_ParkourMove::ParkourMove(this, Result.Target, Result.ApexZ, Duration);
	Move->OnFinished.AddDynamic(this, &UARGA_Parkour::OnMoveFinished);
	Move->OnBlocked.AddDynamic(this, &UARGA_Parkour::OnMoveBlocked);
	Move->ReadyForActivation();
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
		const FVector Far = Inside + Forward * MaxThicknessProbe;
		FHitResult Back;
		if (Trace(Back, FVector(Far.X, Far.Y, ProbeZ), FVector(Inside.X, Inside.Y, ProbeZ))
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
	Out.Facing = FRotator(0.f, (-Wall.ImpactNormal).Rotation().Yaw, 0.f);
	return true;
}
