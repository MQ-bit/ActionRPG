// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/AREquipmentComponent.h"
#include "Character/ARPlayerCharacter.h"
#include "Combat/ARDurability.h"
#include "Combat/ARWeaponData.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GAS/ARGameplayTags.h"

#define AR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARDurabilityWearAndBreakTest, "ActionRoguelike.Weapon.Durability.WearAndBreak", AR_TEST_FLAGS)
bool FARDurabilityWearAndBreakTest::RunTest(const FString& Parameters)
{
	FARDurability D;
	D.Init(3.f);
	TestFalse(TEXT("first hit does not break"), D.Consume(1.f));
	TestFalse(TEXT("second hit does not break"), D.Consume(1.f));
	TestTrue(TEXT("third hit breaks"), D.Consume(1.f));
	TestTrue(TEXT("broken at 0"), D.IsBroken());
	TestFalse(TEXT("a broken weapon loses nothing and does not break again"), D.Consume(1.f));
	TestEqual(TEXT("stays at 0"), D.Current, 0.f);
	TestFalse(TEXT("negative amounts are ignored"), D.Consume(-5.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARDurabilityNoneTest, "ActionRoguelike.Weapon.Durability.NoDurabilityNeverBreaks", AR_TEST_FLAGS)
bool FARDurabilityNoneTest::RunTest(const FString& Parameters)
{
	FARDurability D;
	D.Init(0.f);
	TestFalse(TEXT("no durability never breaks"), D.Consume(100.f));
	TestFalse(TEXT("not broken"), D.IsBroken());
	TestEqual(TEXT("fraction of an unbreakable weapon is 1"), D.Fraction(), 1.f);
	D.Init(-4.f);
	TestFalse(TEXT("negative max counts as none"), D.HasDurability());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARDurabilityRepairTest, "ActionRoguelike.Weapon.Durability.Repair", AR_TEST_FLAGS)
bool FARDurabilityRepairTest::RunTest(const FString& Parameters)
{
	FARDurability D;
	D.Init(4.f);
	D.Consume(4.f);
	TestTrue(TEXT("repairing a broken weapon un-breaks it"), D.Repair(1.f));
	TestFalse(TEXT("not broken after repair"), D.IsBroken());
	TestFalse(TEXT("repairing a working weapon is not a 'repaired from broken' event"), D.Repair(1.f));
	D.Repair(100.f);
	TestEqual(TEXT("repair stops at the max"), D.Current, 4.f);
	TestFalse(TEXT("zero repair does nothing"), D.Repair(0.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARResolveAttackValuesTest, "ActionRoguelike.Weapon.ResolveAttackValues", AR_TEST_FLAGS)
bool FARResolveAttackValuesTest::RunTest(const FString& Parameters)
{
	UAnimMontage* FistMontage = NewObject<UAnimMontage>();
	UAnimMontage* BatMontage = NewObject<UAnimMontage>();
	FARAttackValues Unarmed;
	Unarmed.Montages = { FistMontage };
	Unarmed.Damage = 20.f;
	Unarmed.HitRange = 150.f;
	Unarmed.HitRadius = 100.f;

	UARWeaponData* Bat = NewObject<UARWeaponData>();
	Bat->ComboMontages = { BatMontage };
	Bat->Damage = 40.f;
	Bat->HitRange = 170.f;
	Bat->HitRadius = 90.f;
	Bat->BrokenDamageScale = 0.25f;

	const FARAttackValues NoWeapon = ARWeapon::ResolveAttackValues(Unarmed, nullptr, false);
	TestTrue(TEXT("unarmed keeps its montage"), NoWeapon.Montages.Num() == 1 && NoWeapon.Montages[0] == FistMontage);
	TestEqual(TEXT("unarmed keeps its damage"), NoWeapon.Damage, 20.f);

	const FARAttackValues Armed = ARWeapon::ResolveAttackValues(Unarmed, Bat, false);
	TestTrue(TEXT("the weapon combo replaces the unarmed one"), Armed.Montages.Num() == 1 && Armed.Montages[0] == BatMontage);
	TestEqual(TEXT("weapon damage"), Armed.Damage, 40.f);
	TestEqual(TEXT("weapon range"), Armed.HitRange, 170.f);
	TestEqual(TEXT("weapon radius"), Armed.HitRadius, 90.f);

	const FARAttackValues Broken = ARWeapon::ResolveAttackValues(Unarmed, Bat, true);
	TestEqual(TEXT("a broken weapon hits for the broken scale"), Broken.Damage, 10.f);
	TestTrue(TEXT("a broken weapon keeps its combo"), Broken.Montages.Num() == 1 && Broken.Montages[0] == BatMontage);

	const FARAttackValues Again = ARWeapon::ResolveAttackValues(Unarmed, nullptr, false);
	TestEqual(TEXT("unarmed after armed gets the unarmed damage back (defaults are never written)"), Again.Damage, 20.f);

	Bat->ComboMontages.Reset();
	const FARAttackValues NoMontage = ARWeapon::ResolveAttackValues(Unarmed, Bat, false);
	TestTrue(TEXT("a weapon without a combo falls back to the unarmed montages"), NoMontage.Montages.Num() == 1 && NoMontage.Montages[0] == FistMontage);
	TestEqual(TEXT("...but still uses the weapon damage"), NoMontage.Damage, 40.f);
	return true;
}

namespace
{
	struct FARWeaponFixture
	{
		FTestWorldWrapper WorldWrapper;
		AARPlayerCharacter* Player = nullptr;
		UAREquipmentComponent* Equipment = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
		UARWeaponData* Bat = nullptr;

		bool Init(FAutomationTestBase& Test)
		{
			if (!WorldWrapper.CreateTestWorld(EWorldType::Game) || !WorldWrapper.BeginPlayInTestWorld())
			{
				WorldWrapper.ForwardErrorMessages(&Test);
				return false;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = WorldWrapper.GetTestWorld()->SpawnActor<AARPlayerCharacter>(FVector(0.f, 0.f, 96.f), FRotator::ZeroRotator, Params);
			Equipment = Player ? Player->FindComponentByClass<UAREquipmentComponent>() : nullptr;
			ASC = Player ? Player->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
			Bat = NewObject<UARWeaponData>();
			Bat->MaxDurability = 2.f;
			Bat->DurabilityPerHit = 1.f;
			return Test.TestNotNull(TEXT("player"), Player) && Test.TestNotNull(TEXT("equipment component"), Equipment)
				&& Test.TestNotNull(TEXT("ASC"), ASC);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAREquipAttachTest, "ActionRoguelike.Weapon.Equipment.EquipAttachesAVisualOnlyMesh", AR_TEST_FLAGS)
bool FAREquipAttachTest::RunTest(const FString& Parameters)
{
	FARWeaponFixture F;
	if (!F.Init(*this))
	{
		return false;
	}
	TestTrue(TEXT("equip succeeds"), F.Equipment->Equip(F.Bat));
	UStaticMeshComponent* Mesh = F.Equipment->GetWeaponMesh();
	if (!TestNotNull(TEXT("weapon mesh created"), Mesh))
	{
		return false;
	}
	TestTrue(TEXT("attached to the character mesh"), Mesh->GetAttachParent() == F.Player->GetMesh());
	TestEqual(TEXT("at the weapon socket"), Mesh->GetAttachSocketName(), FName(TEXT("weapon_r")));
	TestTrue(TEXT("registered"), Mesh->IsRegistered());
	TestEqual(TEXT("no collision (visual only)"), Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("no overlap events"), Mesh->GetGenerateOverlapEvents());
	TestTrue(TEXT("visible"), Mesh->IsVisible());

	F.Equipment->Unequip();
	TestNull(TEXT("unequip clears the weapon"), F.Equipment->GetEquippedWeapon());
	TestNull(TEXT("unequip removes the mesh"), F.Equipment->GetWeaponMesh());
	TestTrue(TEXT("the old mesh is destroyed"), !IsValid(Mesh) || Mesh->IsBeingDestroyed());
	F.Equipment->Unequip();
	TestTrue(TEXT("a second unequip is harmless"), F.Equipment->GetWeaponMesh() == nullptr);
	TestFalse(TEXT("equipping nothing fails"), F.Equipment->Equip(nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAREquipHiddenDuringParkourTest, "ActionRoguelike.Weapon.Equipment.HiddenDuringParkour", AR_TEST_FLAGS)
bool FAREquipHiddenDuringParkourTest::RunTest(const FString& Parameters)
{
	FARWeaponFixture F;
	if (!F.Init(*this))
	{
		return false;
	}
	F.Equipment->Equip(F.Bat);
	F.ASC->AddLooseGameplayTag(TAG_State_Parkouring.GetTag());
	TestFalse(TEXT("hidden while parkouring"), F.Equipment->GetWeaponMesh()->IsVisible());
	F.ASC->RemoveLooseGameplayTag(TAG_State_Parkouring.GetTag());
	TestTrue(TEXT("visible again after the parkour"), F.Equipment->GetWeaponMesh()->IsVisible());
	F.ASC->AddLooseGameplayTag(TAG_State_Attacking.GetTag());
	TestTrue(TEXT("stays visible while attacking"), F.Equipment->GetWeaponMesh()->IsVisible());
	F.ASC->RemoveLooseGameplayTag(TAG_State_Attacking.GetTag());

	F.Equipment->Unequip();
	F.ASC->AddLooseGameplayTag(TAG_State_Parkouring.GetTag());
	F.Equipment->Equip(F.Bat);
	TestFalse(TEXT("equipped during a parkour: starts hidden"), F.Equipment->GetWeaponMesh()->IsVisible());
	F.ASC->RemoveLooseGameplayTag(TAG_State_Parkouring.GetTag());
	TestTrue(TEXT("shown when that parkour ends"), F.Equipment->GetWeaponMesh()->IsVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAREquipBreakAndRepairTest, "ActionRoguelike.Weapon.Equipment.BreakStaysEquippedAndRepairs", AR_TEST_FLAGS)
bool FAREquipBreakAndRepairTest::RunTest(const FString& Parameters)
{
	FARWeaponFixture F;
	if (!F.Init(*this))
	{
		return false;
	}
	int32 Broken = 0;
	int32 Repaired = 0;
	F.Equipment->OnWeaponBroken.AddLambda([&Broken]() { ++Broken; });
	F.Equipment->OnWeaponRepaired.AddLambda([&Repaired]() { ++Repaired; });

	TestFalse(TEXT("a hit without a weapon does nothing"), F.Equipment->OnWeaponHit());
	F.Equipment->Equip(F.Bat);
	TestFalse(TEXT("first hit"), F.Equipment->OnWeaponHit());
	TestTrue(TEXT("second hit breaks it"), F.Equipment->OnWeaponHit());
	TestFalse(TEXT("further hits do not break it again"), F.Equipment->OnWeaponHit());
	TestEqual(TEXT("broken event once"), Broken, 1);
	TestTrue(TEXT("broken"), F.Equipment->IsBroken());
	TestTrue(TEXT("a broken weapon stays equipped"), F.Equipment->GetEquippedWeapon() == F.Bat);
	TestNotNull(TEXT("and in the hand"), F.Equipment->GetWeaponMesh());

	F.Equipment->Repair(1.f);
	TestFalse(TEXT("repaired"), F.Equipment->IsBroken());
	TestEqual(TEXT("repaired event once"), Repaired, 1);

	F.Equipment->OnWeaponHit();
	F.Equipment->Unequip();
	F.Equipment->Equip(F.Bat);
	TestTrue(TEXT("re-equipping the same bat keeps its wear (broken again after the hit)"), F.Equipment->IsBroken());

	UARWeaponData* Other = NewObject<UARWeaponData>();
	Other->MaxDurability = 5.f;
	F.Equipment->Equip(Other);
	TestFalse(TEXT("another weapon starts fresh"), F.Equipment->IsBroken());
	TestEqual(TEXT("at its max"), F.Equipment->GetDurability().Current, 5.f);
	return true;
}

#endif
