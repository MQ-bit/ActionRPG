// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GAS/ARComboState.h"

#define AR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

#define AR_COMBO_TEST(ClassName, PrettyName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, "ActionRoguelike.Combo." PrettyName, AR_TEST_FLAGS) \
	bool ClassName::RunTest(const FString& Parameters)

namespace
{
	using EAction = FARComboState::EAction;
	constexpr int32 NumSteps = 3;
}

AR_COMBO_TEST(FARComboPressBeforeHitIgnoredTest, "PressBeforeHitIsIgnored")
{
	FARComboState State;
	const int32 Id = State.BeginStep(0, NumSteps);

	TestTrue(TEXT("press before the hit frame does nothing"), State.OnPressed() == EAction::None);
	TestTrue(TEXT("blend-out without a queued step does not chain"), State.OnBlendingOut(Id, false) == EAction::None);
	TestTrue(TEXT("the combo ends when the montage ends"), State.OnEnded(Id, false) == EAction::End);
	return true;
}

AR_COMBO_TEST(FARComboQueueThenChainTest, "PressAfterHitQueuesAndChainsOnBlendOut")
{
	FARComboState State;
	const int32 Id = State.BeginStep(0, NumSteps);

	State.OnHit();
	TestTrue(TEXT("press inside the window is queued, not played yet"), State.OnPressed() == EAction::None);
	TestTrue(TEXT("blend-out starts the queued next step"), State.OnBlendingOut(Id, false) == EAction::PlayNextStep);
	TestEqual(TEXT("next step is step 2"), State.GetNextStepIndex(), 1);
	return true;
}

AR_COMBO_TEST(FARComboDoublePressTest, "RepeatedPressesQueueOnlyOneStep")
{
	FARComboState State;
	const int32 Id = State.BeginStep(0, NumSteps);

	State.OnHit();
	State.OnPressed();
	State.OnPressed();
	TestTrue(TEXT("first blend-out chains"), State.OnBlendingOut(Id, false) == EAction::PlayNextStep);

	const int32 NextId = State.BeginStep(1, NumSteps);
	TestTrue(TEXT("the new step starts with an empty queue and a closed window"), State.OnBlendingOut(NextId, false) == EAction::None);
	return true;
}

AR_COMBO_TEST(FARComboLatePressTest, "PressAfterBlendOutChainsImmediately")
{
	FARComboState State;
	const int32 Id = State.BeginStep(0, NumSteps);

	State.OnHit();
	TestTrue(TEXT("blend-out with nothing queued waits"), State.OnBlendingOut(Id, false) == EAction::None);
	TestTrue(TEXT("a late press starts the next step directly"), State.OnPressed() == EAction::PlayNextStep);
	return true;
}

AR_COMBO_TEST(FARComboLastStepTest, "LastStepIgnoresPress")
{
	FARComboState State;
	const int32 Id = State.BeginStep(NumSteps - 1, NumSteps);

	State.OnHit();
	TestTrue(TEXT("no next step: press is ignored"), State.OnPressed() == EAction::None);
	TestTrue(TEXT("blend-out does not chain"), State.OnBlendingOut(Id, false) == EAction::None);
	TestTrue(TEXT("late press is ignored too"), State.OnPressed() == EAction::None);
	TestTrue(TEXT("the combo ends"), State.OnEnded(Id, false) == EAction::End);
	return true;
}

AR_COMBO_TEST(FARComboWindowResetsPerStepTest, "InputWindowClosesWhenNextStepStarts")
{
	FARComboState State;
	State.BeginStep(0, NumSteps);
	State.OnHit();

	const int32 Id2 = State.BeginStep(1, NumSteps);
	TestTrue(TEXT("press before step 2 reaches its hit frame is ignored"), State.OnPressed() == EAction::None);
	TestTrue(TEXT("so nothing is queued at blend-out"), State.OnBlendingOut(Id2, false) == EAction::None);
	return true;
}

AR_COMBO_TEST(FARComboInterruptedTest, "InterruptedMontageCancelsCombo")
{
	FARComboState State;
	const int32 Id = State.BeginStep(0, NumSteps);

	TestTrue(TEXT("interrupted blend-out cancels"), State.OnBlendingOut(Id, true) == EAction::Cancel);
	TestTrue(TEXT("interrupted end cancels"), State.OnEnded(Id, true) == EAction::Cancel);
	return true;
}

AR_COMBO_TEST(FARComboStaleStepTest, "CallbacksOfReplacedStepAreIgnored")
{
	FARComboState State;
	const int32 Id1 = State.BeginStep(0, NumSteps);
	const int32 Id2 = State.BeginStep(1, NumSteps);

	TestTrue(TEXT("old step end is ignored"), State.OnEnded(Id1, false) == EAction::None);
	TestTrue(TEXT("old step interruption is ignored"), State.OnBlendingOut(Id1, true) == EAction::None);
	TestTrue(TEXT("current step end still ends the combo"), State.OnEnded(Id2, false) == EAction::End);
	return true;
}

AR_COMBO_TEST(FARComboReactivationTest, "CallbacksOfEndedAbilityDoNotAffectNextActivation")
{
	// Review finding: after cancel + quick re-activation the old queued end event used to match by montage asset.
	FARComboState State;
	const int32 OldId = State.BeginStep(0, NumSteps);
	State.Invalidate();

	const int32 NewId = State.BeginStep(0, NumSteps);
	TestTrue(TEXT("new activation gets a different playback id"), NewId != OldId);
	TestTrue(TEXT("stale interrupted end of the old activation is ignored"), State.OnEnded(OldId, true) == EAction::None);
	TestTrue(TEXT("stale blend-out of the old activation is ignored"), State.OnBlendingOut(OldId, true) == EAction::None);
	TestTrue(TEXT("the new activation still ends normally"), State.OnEnded(NewId, false) == EAction::End);
	return true;
}

AR_COMBO_TEST(FARComboInvalidatedTest, "InvalidateIgnoresAllPendingCallbacks")
{
	FARComboState State;
	const int32 Id = State.BeginStep(0, NumSteps);
	State.Invalidate();

	TestTrue(TEXT("end after invalidate is ignored"), State.OnEnded(Id, false) == EAction::None);
	TestTrue(TEXT("blend-out after invalidate is ignored"), State.OnBlendingOut(Id, false) == EAction::None);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
