// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Combo bookkeeping for UARGA_BasicAttack, kept free of UObjects, the ASC and montages so it can be unit tested.
 * The ability reports events (hit frame, input, montage callbacks) and plays whatever action comes back.
 * Every montage callback carries the playback id of the step that registered it; callbacks of a replaced step
 * or of an ended ability are ignored, even when the same montage asset is played again.
 */
struct FARComboState
{
	enum class EAction : uint8
	{
		None,
		/** Start the step at GetNextStepIndex() (call BeginStep for it). */
		PlayNextStep,
		/** Combo finished normally. */
		End,
		/** Combo was interrupted. */
		Cancel,
	};

	/** Starts a step and returns its playback id. The step's montage callbacks must pass this id back. */
	int32 BeginStep(int32 InStepIndex, int32 InNumSteps);

	/** Makes every callback issued so far stale. Call when the ability ends. */
	void Invalidate();

	/** The current step reached its hit frame: the input window for the next step opens. */
	void OnHit();

	/** Attack was pressed while the combo is active. */
	EAction OnPressed();

	EAction OnBlendingOut(int32 PlaybackId, bool bInterrupted);
	EAction OnEnded(int32 PlaybackId, bool bInterrupted);

	int32 GetStepIndex() const { return StepIndex; }
	int32 GetNextStepIndex() const { return StepIndex + 1; }

private:
	bool HasNextStep() const { return StepIndex + 1 < NumSteps; }

	int32 NumSteps = 0;
	int32 StepIndex = 0;
	int32 CurrentPlaybackId = 0;
	bool bWindowOpen = false;
	bool bNextStepQueued = false;
	/** The step started blending out with nothing queued; a late press then starts the next step directly. */
	bool bBlendingOut = false;
};
