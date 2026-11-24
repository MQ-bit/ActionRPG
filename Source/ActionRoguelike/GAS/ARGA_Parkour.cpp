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
#include "Character/ARPlayerCharacter.h"

namespace
{
	/** Trace height for the wall in front; anything lower is a step the capsule walks over. */
	constexpr float KneeHeight = 40.f;
	/** The top surface is looked for from this height above the feet; taller things are not climbable anyway. */
	constexpr float TopProbeHeight = ARParkour::MantleMaxHeight + 100.f;
	constexpr float ClearanceProbeLength = 400.f;
	/** A matched start leaves at least this much of the approach to play (s). */
	constexpr float MinMatchedApproachSeconds = 0.05f;
	/** The top depth is measured this far beyond what the capsule needs at the usual spot, so a clip can move its landing inward (cm). */
	constexpr float MantleDepthScanExtraCm = 40.f;
	/** The blend-out of a clip that ends early because the player moves (s). */
	constexpr float MoveCancelBlendSeconds = 0.25f;
	TAutoConsoleVariable<int32> CVarCloseWall(TEXT("ar.Parkour.CloseWall"), 2, TEXT("A tall mantle (150 cm and up) closer than 80 cm to the wall: 1 skips the move (a plain jump), 0 plays the clip from a late start (the body goes 10-20 cm into the wall), 2 (default) walks a step backwards first when standing or slow (else skips the move)."));
	/** Experiment (CloseWall 2): the wall distance the step back aims for (cm), the longest step (cm), the walking speed backwards (cm/s) and the speed above which there is no step back (cm/s). */
	constexpr float StepBackTargetWallCm = 85.f;
	constexpr float StepBackMaxCm = 40.f;
	constexpr float StepBackWalkSpeed = 170.f;
	constexpr float StepBackMaxSpeed = 150.f;
	TAutoConsoleVariable<int32> CVarMatchFeet(TEXT("ar.Parkour.MatchFeet"), 1, TEXT("1 compares the feet with the clip's feet when picking the start of a matched parkour move, 0 leaves them out."));
	/** Obstacles deeper than this count as "very thick"; only the vault limit (75) matters. */
	constexpr float MaxThicknessProbe = 400.f;
	/** Moving at least this fast (cm/s), a press that is too far from the wall waits until the character has run closer. */
	constexpr float BufferMinSpeed = 120.f;
	/**
	 * The approach warp absorbs up to this much (cm) more distance to the wall than the clip's reference distance. A wall farther
	 * than that is run up to first (moving) or not parkoured at all (standing).
	 */
	constexpr float GlideMaxCm = 60.f;
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
	const bool bBase = Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
	const bool bProbed = bBase && Probe(ActorInfo, Result);
	const bool bFar = bProbed && IsTooFar(Result, ActorInfo);
	if (bBase && (!bProbed || bFar))
	{
		UE_LOG(LogAR, Verbose, TEXT("[Parkour] cannot start: probe %s, too far %s (wall %.0f cm, height %.0f)"), bProbed ? TEXT("ok") : TEXT("failed"), bFar ? TEXT("yes") : TEXT("no"), bProbed ? Result.WallDistance : 0.f, bProbed ? Result.Height : 0.f);
	}
	else if (!bBase)
	{
		UE_LOG(LogAR, Verbose, TEXT("[Parkour] cannot start: the ability itself is blocked (tags, cost, already running)"));
	}
	return bProbed && !bFar
		&& !(ARParkour::MantleTooClose(Result.Move, Result.Height, Result.WallDistance)
			&& (CVarCloseWall.GetValueOnGameThread() == 1
				|| (CVarCloseWall.GetValueOnGameThread() == 2 && ActorInfo && ActorInfo->AvatarActor.IsValid() && ActorInfo->AvatarActor->GetVelocity().Size2D() > StepBackMaxSpeed)));
}

int32 UARGA_Parkour::PickClipIndex(const FProbeResult& Result, const ACharacter* Character) const
{
	TArray<ARParkour::FClipRule> Rules;
	Rules.Reserve(Clips.Num());
	for (const FARParkourClip& Clip : Clips)
	{
		Rules.Add({Clip.Move == EARParkourMoveKind::Mantle ? ARParkour::EMove::Mantle : ARParkour::EMove::Vault,
			Clip.MinHeight, Clip.MaxHeight, static_cast<ARParkour::EVariant>(Clip.Variant)});
	}
	return ARParkour::PickClip(Rules, Result.Move, Result.Height, ARParkour::VariantForSpeed(Character->GetVelocity().Size2D()));
}

bool UARGA_Parkour::IsTooFar(const FProbeResult& Result, const FGameplayAbilityActorInfo* ActorInfo) const
{
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const int32 Index = Character ? PickClipIndex(Result, Character) : INDEX_NONE;
	if (Index == INDEX_NONE)
	{
		return false;
	}
	const float Excess = Result.WallDistance - Clips[Index].ReferenceDistanceCm;
	return Excess > GlideMaxCm;
}

bool UARGA_Parkour::ShouldBufferJump() const
{
	FProbeResult Result;
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	// Only a running character waits: a standing one too far from the wall just jumps.
	return Character && CurrentActorInfo && Character->GetVelocity().Size2D() >= BufferMinSpeed
		&& Probe(CurrentActorInfo, Result) && IsTooFar(Result, CurrentActorInfo);
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

	if (CVarCloseWall.GetValueOnGameThread() == 2 && ARParkour::MantleTooClose(Result.Move, Result.Height, Result.WallDistance))
	{
		BeginStepBack(Result);
		return;
	}
	BeginMove(Result);
}

void UARGA_Parkour::BeginMove(const FProbeResult& Result)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	Avatar->SetActorRotation(Result.Facing);
	BeginCameraOverride(Avatar, Result.Move == ARParkour::EMove::Mantle ? MantleCameraRaiseCm : 0.f);

	if (!TryStartMontage(Result))
	{
		StartInterpolatedMove(Result);
	}
}

void UARGA_Parkour::BeginStepBack(const FProbeResult& Result)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Character->SetActorRotation(Result.Facing);
	Movement->StopMovementImmediately();
	// Walk backwards facing the wall: the body must not turn toward the movement (the lock-on strafe does the same).
	bSavedOrientToMovement = Movement->bOrientRotationToMovement;
	SavedMaxWalkSpeed = Movement->MaxWalkSpeed;
	Movement->bOrientRotationToMovement = false;
	Movement->MaxWalkSpeed = StepBackWalkSpeed;
	bStepBackActive = true;
	StepBackTicks = 0;
	StepBackTotal = FMath::Clamp(StepBackTargetWallCm - Result.WallDistance, 0.f, StepBackMaxCm);
	StepBackFrom = Character->GetActorLocation();
	StepBackDirection = -Result.Inward.GetSafeNormal2D();
	UE_LOG(LogAR, Verbose, TEXT("[Parkour] step back %.0f cm walking %.0f cm/s (wall %.0f cm)"), StepBackTotal, StepBackWalkSpeed, Result.WallDistance);
	Character->GetWorld()->GetTimerManager().SetTimer(StepBackTimer, this, &UARGA_Parkour::StepBackTick, 0.001f, true);
}

void UARGA_Parkour::EndStepBack()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!bStepBackActive || !Character)
	{
		return;
	}
	bStepBackActive = false;
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->bOrientRotationToMovement = bSavedOrientToMovement;
	Movement->MaxWalkSpeed = SavedMaxWalkSpeed;
	Movement->StopMovementImmediately();
	Character->GetWorld()->GetTimerManager().ClearTimer(StepBackTimer);
}

void UARGA_Parkour::StepBackTick()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !IsActive())
	{
		return;
	}
	const float Travelled = static_cast<float>(FVector::DotProduct(Character->GetActorLocation() - StepBackFrom, StepBackDirection));
	// Ends when the distance is walked, or after 1.5 s (something in the way). The timer runs several times per frame (it catches up), so every frame gets the input (a frame without it brakes).
	if (Travelled < StepBackTotal - 1.f && ++StepBackTicks < 1500)
	{
		Character->GetCharacterMovement()->AddInputVector(StepBackDirection, true);
		return;
	}
	EndStepBack();
	FProbeResult Fresh;
	if (Probe(CurrentActorInfo, Fresh))
	{
		BeginMove(Fresh);
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
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

bool UARGA_Parkour::MatchStart(const FARParkourClip& Clip, UAnimMontage* Montage, const FProbeResult& Result, const ACharacter* Character,
	float& OutStart, float& OutReference, float& OutCost, float& OutFootError) const
{
	if (!Montage || !Character || Clip.ApproachEnd <= MinMatchedApproachSeconds)
	{
		return false;
	}

	const FAnimExtractContext Context(0.0, true);
	auto Travel = [&](float From, float To)
	{
		return To > From ? static_cast<float>(Montage->ExtractRootMotionFromTrackRange(From, To, Context).GetTranslation().Size2D()) : 0.f;
	};
	// The approach ends with the capsule AtApproachEnd from the wall whatever the start time, so only the part of the approach played changes.
	const float ReferenceAtZero = Clip.ReferenceDistanceCm + Travel(0.f, Clip.StartPosition);
	const float AtApproachEnd = ReferenceAtZero - Travel(0.f, Clip.ApproachEnd);
	constexpr float Step = 1.f / 30.f;
	TArray<ARParkour::FStartCandidate> Candidates;
	int32 Frame = 0;
	for (float Time = 0.f; Time <= Clip.ApproachEnd - MinMatchedApproachSeconds; Time += Step, ++Frame)
	{
		ARParkour::FStartCandidate Candidate;
		Candidate.Time = Time;
		Candidate.Remaining = Travel(Time, Clip.ApproachEnd);
		Candidate.Speed = Travel(Time, Time + Step) / Step * Clip.PlayRate;
		if (Clip.FootPose.IsValidIndex(Frame))
		{
			Candidate.Feet = Clip.FootPose[Frame];
			Candidate.bHasFeet = true;
		}
		Candidates.Add(Candidate);
	}

	// The character's feet now, the way the clip's feet are stored: in the move direction from the capsule, height above the floor.
	FVector4 Query(0.0, 0.0, 0.0, 0.0);
	bool bHasQuery = false;
	const USkeletalMeshComponent* Mesh = Character->GetMesh();
	if (Mesh && Mesh->DoesSocketExist(TEXT("foot_l")) && Mesh->DoesSocketExist(TEXT("foot_r")))
	{
		const FVector Forward = Result.Inward.GetSafeNormal2D();
		const FVector Origin = Character->GetActorLocation();
		const double Floor = Origin.Z - Result.HalfHeight;
		const FVector Left = Mesh->GetSocketLocation(TEXT("foot_l"));
		const FVector Right = Mesh->GetSocketLocation(TEXT("foot_r"));
		Query = FVector4(FVector::DotProduct(Left - Origin, Forward), Left.Z - Floor, FVector::DotProduct(Right - Origin, Forward), Right.Z - Floor);
		bHasQuery = true;
	}

	const int32 Best = ARParkour::ChooseStart(Candidates, Result.WallDistance - AtApproachEnd, Character->GetVelocity().Size2D(), &OutCost,
		bHasQuery && CVarMatchFeet.GetValueOnGameThread() != 0 ? &Query : nullptr);
	if (Best == INDEX_NONE)
	{
		return false;
	}
	OutStart = Candidates[Best].Time;
	OutReference = ReferenceAtZero - Travel(0.f, OutStart);
	// The mean difference between the character's feet and the clip's feet at the picked start (cm), -1 when not known: what the blend has to bridge.
	OutFootError = -1.f;
	if (bHasQuery && Candidates[Best].bHasFeet)
	{
		const FVector4 Diff = Query - Candidates[Best].Feet;
		OutFootError = static_cast<float>(FMath::Sqrt((Diff.X * Diff.X + Diff.Y * Diff.Y + Diff.Z * Diff.Z + Diff.W * Diff.W) / 4.0));
	}
	return true;
}

int32 UARGA_Parkour::BestMatchedClip(const FProbeResult& Result, const ACharacter* Character, int32 Index) const
{
	int32 Best = Index;
	float BestCost = MAX_flt;
	for (int32 Other = 0; Other < Clips.Num(); ++Other)
	{
		const FARParkourClip& Candidate = Clips[Other];
		const ARParkour::EMove Move = Candidate.Move == EARParkourMoveKind::Mantle ? ARParkour::EMove::Mantle : ARParkour::EMove::Vault;
		if (!Candidate.bMatchStart || Move != Result.Move || Result.Height < Candidate.MinHeight || Result.Height > Candidate.MaxHeight)
		{
			continue;
		}
		float Start = 0.f;
		float Reference = 0.f;
		float Cost = 0.f;
		float FootError = 0.f;
		if (MatchStart(Candidate, Candidate.Montage.LoadSynchronous(), Result, Character, Start, Reference, Cost, FootError) && Cost < BestCost)
		{
			Best = Other;
			BestCost = Cost;
		}
	}
	return Best;
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

	int32 Index = PickClipIndex(Result, Character);
	if (Index != INDEX_NONE && Clips[Index].bMatchStart)
	{
		Index = BestMatchedClip(Result, Character, Index);
	}
	if (Index == INDEX_NONE || !Clips[Index].Montage.LoadSynchronous())
	{
		return false;
	}

	return PlayClip(Result, Index);
}

bool UARGA_Parkour::PlayClip(const FProbeResult& Result, int32 Index)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UMotionWarpingComponent* Warp = Character ? Character->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	UAnimMontage* Montage = Clips.IsValidIndex(Index) ? Clips[Index].Montage.LoadSynchronous() : nullptr;
	if (!Montage || !Warp || !ASC || !AnimInstance)
	{
		return false;
	}

	const FARParkourClip& Clip = Clips[Index];
	float StartPosition = Clip.StartPosition;
	float ReferenceDistance = Clip.ReferenceDistanceCm;

	// A tall mantle very close to the wall starts late in the clip (the start match skips the approach it has no room for), also for the clips that are not matched otherwise.
	if (Clip.bMatchStart || ARParkour::MantleTooClose(Result.Move, Result.Height, Result.WallDistance))
	{
		float MatchedStart = 0.f;
		float MatchedReference = 0.f;
		float MatchCost = 0.f;
		float FootError = -1.f;
		if (MatchStart(Clip, Montage, Result, Character, MatchedStart, MatchedReference, MatchCost, FootError))
		{
			StartPosition = MatchedStart;
			ReferenceDistance = MatchedReference;
			UE_LOG(LogAR, Verbose, TEXT("[Parkour] match: start %.2f s, cost %.2f, feet %.1f cm off"), MatchedStart, MatchCost, FootError);
		}
	}

	// The approach window of the montage warps the root to where the clip has it when the approach ends, measured from the wall:
	// the clip's reference distance minus the root motion it plays until then. Whatever the real distance, the climb part of the
	// clip then starts from the geometry it was made for.
	if (Clip.ApproachEnd > StartPosition)
	{
		const FAnimExtractContext Context(0.0, true);
		const float Travel = Montage->ExtractRootMotionFromTrackRange(StartPosition, Clip.ApproachEnd, Context).GetTranslation().Size2D();
		const FVector Forward = Result.Inward.GetSafeNormal2D();
		// Closer to the wall than the clip's approach ends: the capsule holds its place instead of backing away from the wall.
		const float AtEnd = FMath::Min(ReferenceDistance - Travel, Result.WallDistance);
		// The warp scales the root travel of the approach by (distance to cover now) / (distance the clip covers): 1 plays it as authored.
		const float Needed = Result.WallDistance - AtEnd;
		const float ClipSpeed = Montage->ExtractRootMotionFromTrackRange(StartPosition, StartPosition + 1.f / 30.f, Context).GetTranslation().Size2D() * 30.f;
		UE_LOG(LogAR, Verbose, TEXT("[Parkour] approach: start %.2f s, %.0f cm to cover, the clip covers %.0f (scale %.2f), clip speed %.0f, speed now %.0f"),
			StartPosition, Needed, Travel, Travel > UE_KINDA_SMALL_NUMBER ? Needed / Travel : 0.f, ClipSpeed, Character->GetVelocity().Size2D());
		const FVector Feet(Result.FrontEdgePoint.X - Forward.X * AtEnd, Result.FrontEdgePoint.Y - Forward.Y * AtEnd, Character->GetActorLocation().Z - Result.HalfHeight);
		Warp->AddOrUpdateWarpTargetFromLocationAndRotation(ARParkour::ApproachWarpTargetName(), Feet, Result.Facing);
	}

	// The back-ledge window warps the body to where the clip has it while the hands are on the top (GASP's BackLedge): along the move
	// direction measured from the front edge, in height the clip's rise plus the difference between this obstacle's height and the
	// authored one. The depth of the obstacle only matters after that, for the target window.
	if (Clip.BackLedgeEnd > Clip.ApproachEnd && Clip.AuthoredHeightCm > 0.f)
	{
		const FAnimExtractContext Context(0.0, true);
		const FVector Moved = Montage->ExtractRootMotionFromTrackRange(StartPosition, Clip.BackLedgeEnd, Context).GetTranslation();
		const FVector Forward = Result.Inward.GetSafeNormal2D();
		const float Past = static_cast<float>(Moved.Size2D()) - ReferenceDistance;
		const FVector Feet(Result.FrontEdgePoint.X + Forward.X * Past, Result.FrontEdgePoint.Y + Forward.Y * Past,
			Character->GetActorLocation().Z - Result.HalfHeight + static_cast<float>(Moved.Z) + (Result.Height - Clip.AuthoredHeightCm));
		Warp->AddOrUpdateWarpTargetFromLocationAndRotation(ARParkour::BackLedgeWarpTargetName(), Feet, Result.Facing);
	}

	// GASP-style: where the unwarped rest of the clip ends once it is registered on the ledge (the same registration as the back-ledge target above,
	// at the end of the clip). The capsule must fit there, on the top; if not the landing warp below is used.
	bool bRigid = false;
	FVector RigidCenter = FVector::ZeroVector;
	if (Clip.bRigidAfterLedge && Clip.BackLedgeEnd > Clip.ApproachEnd && Clip.AuthoredHeightCm > 0.f)
	{
		const FAnimExtractContext Context(0.0, true);
		const FVector MovedEnd = Montage->ExtractRootMotionFromTrackRange(StartPosition, Montage->GetPlayLength(), Context).GetTranslation();
		const FVector Forward = Result.Inward.GetSafeNormal2D();
		const float PastEnd = static_cast<float>(MovedEnd.Size2D()) - ReferenceDistance;
		const float FloorZ = Character->GetActorLocation().Z - Result.HalfHeight;
		RigidCenter = FVector(Result.FrontEdgePoint.X + Forward.X * PastEnd, Result.FrontEdgePoint.Y + Forward.Y * PastEnd,
			FloorZ + static_cast<float>(MovedEnd.Z) + (Result.Height - Clip.AuthoredHeightCm) + Result.HalfHeight);
		FHitResult Support;
		const FVector Up(0.f, 0.f, Result.HalfHeight);
		const bool bSupported = Character->GetWorld()->LineTraceSingleByChannel(Support, RigidCenter + Up, RigidCenter - Up - FVector(0.f, 0.f, 20.f), ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(ARParkourRigidEnd), false, Character))
			&& Support.ImpactNormal.Z >= 0.7f && FMath::Abs(Support.ImpactPoint.Z + Result.HalfHeight - RigidCenter.Z) <= 12.f;
		const bool bFitsTop = Result.Move != ARParkour::EMove::Mantle || PastEnd + Character->GetCapsuleComponent()->GetScaledCapsuleRadius() <= Result.TopDepth;
		bRigid = bSupported && bFitsTop && !CapsuleBlockedAt(RigidCenter);
		if (bRigid)
		{
			// The clip's own end height is a few cm off the real top: the last window warps only that height (x/y stay as the clip made them),
			// otherwise the capsule drops to the floor in one frame when the montage ends.
			RigidCenter.Z = Support.ImpactPoint.Z + Result.HalfHeight + 2.f; // the same clearance as the probe's targets
		}
		UE_LOG(LogAR, Verbose, TEXT("[Parkour] rigid end: %.0f cm past the edge, supported %d, fits the top %d (top depth %.0f) -> %s"), PastEnd, bSupported, bFitsTop, Result.TopDepth, bRigid ? TEXT("rigid") : TEXT("landing warp"));
	}

	UE_LOG(LogAR, Verbose, TEXT("[Parkour] clip %d wall %.0f cm (reference %.0f): start %.2f s%s speed %.0f"), Index, Result.WallDistance, ReferenceDistance, StartPosition,
		Clip.bMatchStart ? TEXT(" (matched)") : TEXT(""), Character->GetVelocity().Size2D());
	// Where the capsule ends up: the probed target, moved along the move direction by the clip's offset when that spot is free (the
	// arrival check and the snap use the same spot, so the offset never causes a pop).
	FVector Landing = Result.Target;
	if (!FMath::IsNearlyZero(Clip.WarpOffsetForwardCm))
	{
		const FVector Shifted = Landing + Result.Inward.GetSafeNormal2D() * Clip.WarpOffsetForwardCm;
		// The probe only checked the floor under the original target: the moved spot must be free and have walkable floor of about the
		// same height under it (not the edge of a platform or a gap), or the landing stays where it was probed.
		FHitResult Support;
		const FVector Up(0.f, 0.f, Result.HalfHeight);
		const bool bSupported = Character->GetWorld()->LineTraceSingleByChannel(Support, Shifted + Up, Shifted - Up - FVector(0.f, 0.f, 20.f), ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(ARParkourLanding), false, Character))
			&& Support.ImpactNormal.Z >= 0.7f && FMath::Abs(Support.ImpactPoint.Z + Result.HalfHeight - Landing.Z) <= 12.f;
		// On a mantle top the moved capsule must still fit on the top: its far side stays on it.
		const bool bFitsTop = Result.Move != ARParkour::EMove::Mantle
			|| FVector::DotProduct(Shifted - Result.FrontEdgePoint, Result.Inward.GetSafeNormal2D()) + Character->GetCapsuleComponent()->GetScaledCapsuleRadius() <= Result.TopDepth;
		if (bSupported && bFitsTop && !CapsuleBlockedAt(Shifted))
		{
			Landing = Shifted;
		}
	}
	if (bRigid)
	{
		// The clip plays on as made after the ledge windows (x/y), only its end height is eased to the real top.
		Landing = RigidCenter;
		Warp->AddOrUpdateWarpTargetFromLocationAndRotation(ARParkour::WarpTargetName(), Landing - FVector(0.f, 0.f, Result.HalfHeight), Result.Facing);
	}
	else
	{
		// The warp target is where the feet end up (the warp notify warps to the feet location).
		Warp->AddOrUpdateWarpTargetFromLocationAndRotation(ARParkour::WarpTargetName(), Landing - FVector(0.f, 0.f, Result.HalfHeight - Clip.WarpOffsetZCm), Result.Facing);
	}
	if (ASC->PlayMontage(this, CurrentActivationInfo, Montage, Clip.PlayRate, NAME_None, StartPosition) <= 0.f)
	{
		// Could not start: nothing else has been touched yet, so the interpolated move can take over.
		Warp->RemoveWarpTarget(ARParkour::WarpTargetName());
		Warp->RemoveWarpTarget(ARParkour::ApproachWarpTargetName());
		Warp->RemoveWarpTarget(ARParkour::BackLedgeWarpTargetName());
		return false;
	}

	bMontageMove = true;
	MontageTarget = Landing;
	SafeLocation = Character->GetActorLocation();
	IgnoredObstacle = Result.Obstacle;
	WarpComponent = Warp;

	// Root motion moves the capsule: no gravity, and the obstacle must not block the sweep while it passes over or along it.
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->SetMovementMode(MOVE_Flying);
	// Flying has no braking: when the root motion ends the capsule would keep the last velocity (a slide of 40 cm and more).
	SavedBrakingDecelerationFlying = Movement->BrakingDecelerationFlying;
	Movement->BrakingDecelerationFlying = 8000.f;
	if (AActor* Obstacle = IgnoredObstacle.Get())
	{
		Character->GetCapsuleComponent()->IgnoreActorWhenMoving(Obstacle, true);
	}

	FOnMontageEnded Ended;
	Ended.BindUObject(this, &UARGA_Parkour::OnMontageEnded, ++MontagePlaybackId);
	AnimInstance->Montage_SetEndDelegate(Ended, Montage);
	FOnMontageBlendingOutStarted BlendingOut;
	BlendingOut.BindUObject(this, &UARGA_Parkour::OnMontageBlendingOut, MontagePlaybackId);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendingOut, Montage);

	BeginHandIK(Result, Clips[Index], Montage, Character);

	Character->GetWorld()->GetTimerManager().SetTimer(SafeLocationTimer, this, &UARGA_Parkour::SampleSafeLocation, 1.f / 30.f, true);
	ActiveMontage = Montage;
	ActiveMoveCancelFrom = Clip.MoveCancelFrom;
	if (Clip.MoveCancelFrom > 0.f)
	{
		Character->GetWorld()->GetTimerManager().SetTimer(MoveCancelTimer, this, &UARGA_Parkour::CheckMoveCancel, 1.f / 30.f, true);
	}
	return true;
}

void UARGA_Parkour::OnMontageBlendingOut(UAnimMontage* /*Montage*/, bool bInterrupted, int32 PlaybackId)
{
	// A montage cut short by something else is handled when it ends (OnMontageEnded cancels the move).
	if (bInterrupted || !bMontageMove || PlaybackId != MontagePlaybackId || !IsActive())
	{
		return;
	}
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character || FVector::Dist(Character->GetActorLocation(), MontageTarget) > ArrivalTolerance)
	{
		return; // not on the target: the end handler decides what to do
	}

	// On the target. The ability goes on until the montage is over (ending it would cut the montage and its blend-out), but the
	// character is a walking one from here on, so the anim blueprint blends the montage into the ground pose instead of the in-air pose.
	if (AActor* Obstacle = IgnoredObstacle.Get())
	{
		Character->GetCapsuleComponent()->IgnoreActorWhenMoving(Obstacle, false);
	}
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->SetMovementMode(MOVE_Walking);
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

void UARGA_Parkour::CheckMoveCancel()
{
	const AARPlayerCharacter* Player = Cast<AARPlayerCharacter>(GetAvatarActorFromActorInfo());
	UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	UAnimMontage* Montage = ActiveMontage.Get();
	if (!bMontageMove || !IsActive() || !Player || !AnimInstance || !Montage || !AnimInstance->Montage_IsPlaying(Montage))
	{
		return;
	}
	const float Distance = static_cast<float>(FVector::Dist(Player->GetActorLocation(), MontageTarget));
	if (!ARParkour::MoveCancelAllowed(AnimInstance->Montage_GetPosition(Montage), ActiveMoveCancelFrom, Player->IsMoveInputHeld(), Distance))
	{
		return;
	}

	UE_LOG(LogAR, Verbose, TEXT("[Parkour] %s: movement input at %.2f s, %.1f cm from the target -> the move ends early"), *GetNameSafe(Player), AnimInstance->Montage_GetPosition(Montage), Distance);
	// The capsule is on the target: blend the rest of the clip out and give the character back (the cleanup in EndAbility stands it on the target).
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->CurrentMontageStop(MoveCancelBlendSeconds);
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
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
	EndStepBack();
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
	IK->SetLedge(Top, Result.Inward);
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

void UARGA_Parkour::BeginCameraOverride(AActor* Avatar, float RaiseCm)
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
		SavedTargetOffset = Boom->TargetOffset;
	}
	Boom->GetWorld()->GetTimerManager().ClearTimer(CameraRestoreTimer);

	Boom->bDoCollisionTest = false;
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = CameraLagSpeed;
	// The lag smooths the offset like any movement of the pivot: the camera glides up and back.
	Boom->TargetOffset = SavedTargetOffset + FVector(0.f, 0.f, RaiseCm);
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
	// The lag stays on until the camera has glided back (it is restored with the collision test, below).
	Boom->TargetOffset = SavedTargetOffset;
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
	if ((!bBlocked || CameraRestoreElapsed >= MaxCameraRestoreSeconds) && CameraRestoreElapsed >= MinCameraRestoreSeconds)
	{
		Boom->bEnableCameraLag = bSavedCameraLag;
		Boom->CameraLagSpeed = SavedLagSpeed;
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
		World->GetTimerManager().ClearTimer(MoveCancelTimer);
	}
	ActiveMontage.Reset();

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
		Warp->RemoveWarpTarget(ARParkour::ApproachWarpTargetName());
		Warp->RemoveWarpTarget(ARParkour::BackLedgeWarpTargetName());
	}
	WarpComponent.Reset();
	EndHandIK();

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->Velocity = FVector::ZeroVector;
	Movement->BrakingDecelerationFlying = SavedBrakingDecelerationFlying;
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
				// The capsule is wider than the point checked: floor a little to both sides and ahead, at about the same height, or it ends on an edge.
				const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward);
				const FVector Around[] = {Side * Radius * 0.7f, -Side * Radius * 0.7f, Forward * Radius * 0.7f};
				for (const FVector& Offset : Around)
				{
					FHitResult Foot;
					const FVector At = Spot + Offset;
					if (!Trace(Foot, FVector(At.X, At.Y, TopZ + 20.f), FVector(At.X, At.Y, FeetZ - 100.f)) || Foot.ImpactNormal.Z < 0.7f
						|| FMath::Abs(Foot.ImpactPoint.Z - Support.ImpactPoint.Z) > 20.f)
					{
						Obstacle.bLandingBehind = false;
						break;
					}
				}
			}
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
	float MantleTopDepth = 0.f;
	switch (Move)
	{
	case ARParkour::EMove::Vault:
		Target = Landing;
		ApexZ = ClearTopZ + HalfHeight + 5.f;
		break;
	case ARParkour::EMove::Mantle:
	{
		// How deep the top is, walked in 2.5 cm steps from the front edge until it ends (another component, a wall, or a step of more than 6 cm). Only as far
		// as the capsule needs: the centre goes Radius + 10 cm in, and the far edge should still be a radius away from it.
		const float FullInset = Radius + 10.f;
		float TopDepth = 0.f;
		float LastZ = TopZ;
		for (float Step = 2.5f; Step <= FullInset + Radius + MantleDepthScanExtraCm; Step += 2.5f)
		{
			const FVector At = Wall.ImpactPoint + Forward * Step;
			FHitResult Sample;
			if (!Trace(Sample, FVector(At.X, At.Y, FeetZ + TopProbeHeight), FVector(At.X, At.Y, FeetZ - 10.f)) || Sample.GetComponent() != Wall.GetComponent()
				|| Sample.ImpactNormal.Z < 0.7f || FMath::Abs(Sample.ImpactPoint.Z - LastZ) > 6.f)
			{
				break;
			}
			LastZ = Sample.ImpactPoint.Z;
			// The edge lies somewhere between this sample and the next: count half a step.
			TopDepth = Step + 1.25f;
		}
		// A top the capsule does not fit on hangs over the far edge (a foot in the air): stand in the middle of it, or do not climb at all.
		if (TopDepth < ARParkour::MantleMinTopDepthRadii * Radius)
		{
			return false;
		}
		const float Inset = TopDepth >= FullInset + Radius ? FullInset : TopDepth * 0.5f;
		MantleTopDepth = TopDepth;

		// The capsule centre must stand on this obstacle: a narrow pillar or the far side of a slope has no support here.
		const FVector Spot = Wall.ImpactPoint + Forward * Inset;
		FHitResult Support;
		if (!Trace(Support, FVector(Spot.X, Spot.Y, FeetZ + TopProbeHeight), FVector(Spot.X, Spot.Y, FeetZ - 10.f))
			|| Support.GetComponent() != Wall.GetComponent() || Support.ImpactNormal.Z < 0.7f
			|| Support.ImpactPoint.Z - FeetZ > ARParkour::MantleMaxHeight + 50.f)
		{
			return false;
		}
		// And as wide as the capsule needs: top to both sides of the spot (a narrow beam or a corner has the capsule hanging over its side).
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward);
		for (const float Sign : {-1.f, 1.f})
		{
			const FVector At = Spot + Side * (Sign * Radius * ARParkour::MantleSideSupportRadii);
			FHitResult Beside;
			if (!Trace(Beside, FVector(At.X, At.Y, FeetZ + TopProbeHeight), FVector(At.X, At.Y, FeetZ - 10.f)) || Beside.GetComponent() != Wall.GetComponent()
				|| Beside.ImpactNormal.Z < 0.7f || FMath::Abs(Beside.ImpactPoint.Z - Support.ImpactPoint.Z) > 6.f)
			{
				return false;
			}
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
	Out.WallDistance = static_cast<float>(FVector::DotProduct(Wall.ImpactPoint - Origin, Forward));
	Out.TopDepth = MantleTopDepth;
	return true;
}
