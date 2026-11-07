// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ARComboState.h"

int32 FARComboState::BeginStep(int32 InStepIndex, int32 InNumSteps)
{
	StepIndex = InStepIndex;
	NumSteps = InNumSteps;
	bWindowOpen = false;
	bNextStepQueued = false;
	bBlendingOut = false;
	return ++CurrentPlaybackId;
}

void FARComboState::Invalidate()
{
	++CurrentPlaybackId;
}

void FARComboState::OnHit()
{
	bWindowOpen = true;
}

FARComboState::EAction FARComboState::OnPressed()
{
	// Only after the current step's hit frame, and only if there is a next step.
	if (!bWindowOpen || !HasNextStep())
	{
		return EAction::None;
	}

	if (bBlendingOut)
	{
		// The queue point (blend-out start) has passed; chain right away instead of dropping the press.
		return EAction::PlayNextStep;
	}

	bNextStepQueued = true;
	return EAction::None;
}

FARComboState::EAction FARComboState::OnBlendingOut(int32 PlaybackId, bool bInterrupted)
{
	if (PlaybackId != CurrentPlaybackId)
	{
		return EAction::None;
	}

	if (bInterrupted)
	{
		return EAction::Cancel;
	}

	if (bNextStepQueued)
	{
		return EAction::PlayNextStep;
	}

	bBlendingOut = true;
	return EAction::None;
}

FARComboState::EAction FARComboState::OnEnded(int32 PlaybackId, bool bInterrupted)
{
	if (PlaybackId != CurrentPlaybackId)
	{
		return EAction::None;
	}

	return bInterrupted ? EAction::Cancel : EAction::End;
}
