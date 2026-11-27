// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Character/AREnemyCharacter.h"
#include "Character/ARPlayerCharacter.h"
#include "Core/ARCombat.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/ARAbilitySystemComponent.h"
#include "GAS/ARGA_Block.h"
#include "GAS/ARGA_HitReact.h"
#include "GAS/ARGameplayTags.h"

#define AR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARHitDirectionTest, "ActionRoguelike.Combat.HitDirection", AR_TEST_FLAGS)
bool FARHitDirectionTest::RunTest(const FString& Parameters)
{
	using namespace ARCombat;
	const FVector Forward(1.f, 0.f, 0.f);
	TestTrue(TEXT("attacker in front"), HitDirection(Forward, FVector(100.f, 10.f, 0.f)) == EHitDirection::Front);
	TestTrue(TEXT("attacker behind"), HitDirection(Forward, FVector(-100.f, 10.f, 0.f)) == EHitDirection::Back);
	TestTrue(TEXT("height is ignored"), HitDirection(Forward, FVector(10.f, 0.f, -500.f)) == EHitDirection::Front);
	TestTrue(TEXT("exactly at the side counts as front"), HitDirection(Forward, FVector(0.f, 100.f, 0.f)) == EHitDirection::Front);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARResolveDefenseTest, "ActionRoguelike.Combat.ResolveDefense", AR_TEST_FLAGS)
bool FARResolveDefenseTest::RunTest(const FString& Parameters)
{
	using namespace ARCombat;
	const FVector Forward(1.f, 0.f, 0.f);
	const FVector InFront(100.f, 0.f, 0.f);
	TestTrue(TEXT("not blocking"), ResolveDefense(false, 1.f, 0.2f, Forward, InFront, 70.f) == EDefense::None);
	TestTrue(TEXT("blocked from the front"), ResolveDefense(true, 1.f, 0.2f, Forward, InFront, 70.f) == EDefense::Blocked);
	TestTrue(TEXT("parried at the start of the block"), ResolveDefense(true, 0.1f, 0.2f, Forward, InFront, 70.f) == EDefense::Parried);
	TestTrue(TEXT("parry window edge is still a parry"), ResolveDefense(true, 0.2f, 0.2f, Forward, InFront, 70.f) == EDefense::Parried);
	TestTrue(TEXT("hit from behind is not blocked"), ResolveDefense(true, 1.f, 0.2f, Forward, FVector(-100.f, 0.f, 0.f), 70.f) == EDefense::None);
	TestTrue(TEXT("hit from behind is not parried either"), ResolveDefense(true, 0.05f, 0.2f, Forward, FVector(-100.f, 0.f, 0.f), 70.f) == EDefense::None);
	const FVector At60 = FVector(FMath::Cos(FMath::DegreesToRadians(60.f)), FMath::Sin(FMath::DegreesToRadians(60.f)), 0.f) * 100.f;
	const FVector At80 = FVector(FMath::Cos(FMath::DegreesToRadians(80.f)), FMath::Sin(FMath::DegreesToRadians(80.f)), 0.f) * 100.f;
	TestTrue(TEXT("60 deg off is inside a 70 deg block"), ResolveDefense(true, 1.f, 0.2f, Forward, At60, 70.f) == EDefense::Blocked);
	TestTrue(TEXT("80 deg off is outside a 70 deg block"), ResolveDefense(true, 1.f, 0.2f, Forward, At80, 70.f) == EDefense::None);
	TestTrue(TEXT("a zero direction never blocks"), ResolveDefense(true, 1.f, 0.2f, Forward, FVector::ZeroVector, 70.f) == EDefense::None);
	return true;
}

namespace
{
	template <typename T>
	T* SpawnIn(FTestWorldWrapper& Wrapper, const FVector& Location)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return Wrapper.GetTestWorld()->SpawnActor<T>(Location, FRotator::ZeroRotator, Params);
	}

	void SendEvent(AActor* Target, const FGameplayTag& Tag, AActor* Instigator)
	{
		FGameplayEventData Event;
		Event.EventTag = Tag;
		Event.Instigator = Instigator;
		Event.Target = Target;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Target, Tag, Event);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARHitReactTriggerTest, "ActionRoguelike.Combat.HitReact.TriggeredByDamageAndCooldown", AR_TEST_FLAGS)
bool FARHitReactTriggerTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!Wrapper.CreateTestWorld(EWorldType::Game) || !Wrapper.BeginPlayInTestWorld())
	{
		Wrapper.ForwardErrorMessages(this);
		return false;
	}
	AAREnemyCharacter* Enemy = SpawnIn<AAREnemyCharacter>(Wrapper, FVector(0.f, 0.f, 96.f));
	AAREnemyCharacter* Attacker = SpawnIn<AAREnemyCharacter>(Wrapper, FVector(200.f, 0.f, 96.f));
	UAbilitySystemComponent* ASC = Enemy ? Enemy->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
	if (!TestNotNull(TEXT("enemy ASC"), ASC) || !TestNotNull(TEXT("attacker"), Attacker))
	{
		return false;
	}
	ASC->GiveAbility(FGameplayAbilitySpec(UARGA_HitReact::StaticClass(), 1));
	const FGameplayTag Reacting = TAG_State_HitReact.GetTag();

	ASC->AddLooseGameplayTag(TAG_State_Parkouring.GetTag());
	SendEvent(Enemy, TAG_Event_Damaged.GetTag(), Attacker);
	TestFalse(TEXT("no reaction during a parkour move"), ASC->HasMatchingGameplayTag(Reacting));
	ASC->RemoveLooseGameplayTag(TAG_State_Parkouring.GetTag());

	SendEvent(Enemy, TAG_Event_Damaged.GetTag(), Attacker);
	TestTrue(TEXT("damage starts a reaction (no montage: timed fallback)"), ASC->HasMatchingGameplayTag(Reacting));

	const FGameplayTagContainer HitReactTags(TAG_Ability_HitReact.GetTag());
	ASC->CancelAbilities(&HitReactTags);
	TestFalse(TEXT("reaction ended"), ASC->HasMatchingGameplayTag(Reacting));
	SendEvent(Enemy, TAG_Event_Damaged.GetTag(), Attacker);
	TestFalse(TEXT("no new reaction right after one ended (cooldown, no stun lock)"), ASC->HasMatchingGameplayTag(Reacting));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARHitReactParriedTest, "ActionRoguelike.Combat.HitReact.TriggeredByParry", AR_TEST_FLAGS)
bool FARHitReactParriedTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!Wrapper.CreateTestWorld(EWorldType::Game) || !Wrapper.BeginPlayInTestWorld())
	{
		Wrapper.ForwardErrorMessages(this);
		return false;
	}
	AAREnemyCharacter* Enemy = SpawnIn<AAREnemyCharacter>(Wrapper, FVector(0.f, 0.f, 96.f));
	UAbilitySystemComponent* ASC = Enemy ? Enemy->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
	if (!TestNotNull(TEXT("enemy ASC"), ASC))
	{
		return false;
	}
	ASC->GiveAbility(FGameplayAbilitySpec(UARGA_HitReact::StaticClass(), 1));
	SendEvent(Enemy, TAG_Event_Parried.GetTag(), nullptr);
	TestTrue(TEXT("a parried attacker reacts"), ASC->HasMatchingGameplayTag(TAG_State_HitReact.GetTag()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARBlockHoldTest, "ActionRoguelike.Combat.Block.HoldReleaseAndLockout", AR_TEST_FLAGS)
bool FARBlockHoldTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!Wrapper.CreateTestWorld(EWorldType::Game) || !Wrapper.BeginPlayInTestWorld())
	{
		Wrapper.ForwardErrorMessages(this);
		return false;
	}
	AARPlayerCharacter* Player = SpawnIn<AARPlayerCharacter>(Wrapper, FVector(0.f, 0.f, 96.f));
	UARAbilitySystemComponent* ASC = Player ? Player->FindComponentByClass<UARAbilitySystemComponent>() : nullptr;
	if (!TestNotNull(TEXT("player ASC"), ASC))
	{
		return false;
	}
	ASC->GiveStartupAbility(UARGA_Block::StaticClass());
	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->MaxWalkSpeed = 600.f;
	const FGameplayTag Blocking = TAG_State_Blocking.GetTag();
	const FGameplayTag Input = TAG_InputTag_Block.GetTag();

	ASC->AddLooseGameplayTag(TAG_State_Parkouring.GetTag());
	ASC->AbilityInputTagPressed(Input);
	TestFalse(TEXT("no block during a parkour move"), ASC->HasMatchingGameplayTag(Blocking));
	ASC->RemoveLooseGameplayTag(TAG_State_Parkouring.GetTag());

	ASC->AbilityInputTagPressed(Input);
	TestTrue(TEXT("pressing blocks"), ASC->HasMatchingGameplayTag(Blocking));
	TestNotNull(TEXT("the hit code finds the running block"), UARGA_Block::FindActiveBlock(ASC));
	TestEqual(TEXT("walks while blocking"), Movement->MaxWalkSpeed, 300.f);

	ASC->AbilityInputTagReleased(Input);
	TestFalse(TEXT("releasing stops the block"), ASC->HasMatchingGameplayTag(Blocking));
	TestNull(TEXT("no running block after release"), UARGA_Block::FindActiveBlock(ASC));
	TestEqual(TEXT("speed restored"), Movement->MaxWalkSpeed, 600.f);

	ASC->AbilityInputTagPressed(Input);
	TestFalse(TEXT("an immediate re-block is locked out (no parry spamming)"), ASC->HasMatchingGameplayTag(Blocking));
	return true;
}

#endif
