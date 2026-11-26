// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/AREquipmentComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Combat/ARWeaponData.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GAS/ARGameplayTags.h"

UAREquipmentComponent::UAREquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAREquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		TagASC = ASC;
		ParkourTagHandle = ASC->RegisterGameplayTagEvent(TAG_State_Parkouring.GetTag(), EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UAREquipmentComponent::OnParkourTagChanged);
	}
}

void UAREquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = TagASC.Get())
	{
		ASC->RegisterGameplayTagEvent(TAG_State_Parkouring.GetTag(), EGameplayTagEventType::NewOrRemoved).Remove(ParkourTagHandle);
	}
	ParkourTagHandle.Reset();
	TagASC.Reset();
	DestroyWeaponMesh();

	Super::EndPlay(EndPlayReason);
}

bool UAREquipmentComponent::Equip(UARWeaponData* Weapon)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Weapon || !Character || !Character->GetMesh())
	{
		return false;
	}
	DestroyWeaponMesh();

	EquippedWeapon = Weapon;
	if (DurabilityWeapon != Weapon)
	{
		DurabilityWeapon = Weapon;
		Durability.Init(Weapon->MaxDurability);
	}

	// Visual only: the hit comes from the attack's overlap query, so the mesh must not block or overlap anything.
	WeaponMesh = NewObject<UStaticMeshComponent>(GetOwner(), NAME_None, RF_Transient);
	WeaponMesh->SetStaticMesh(Weapon->Mesh);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
	WeaponMesh->SetSimulatePhysics(false);
	WeaponMesh->SetCanEverAffectNavigation(false);
	WeaponMesh->SetupAttachment(Character->GetMesh(), Weapon->AttachSocket);
	WeaponMesh->SetRelativeTransform(Weapon->AttachOffset);
	WeaponMesh->RegisterComponent();
	UpdateMeshVisibility();

	OnEquippedWeaponChanged.Broadcast(EquippedWeapon);
	return true;
}

void UAREquipmentComponent::Unequip()
{
	if (!EquippedWeapon)
	{
		return;
	}
	DestroyWeaponMesh();
	EquippedWeapon = nullptr;
	OnEquippedWeaponChanged.Broadcast(nullptr);
}

void UAREquipmentComponent::ToggleDefaultWeapon()
{
	if (EquippedWeapon)
	{
		Unequip();
	}
	else
	{
		Equip(DefaultWeapon);
	}
}

bool UAREquipmentComponent::OnWeaponHit()
{
	if (!EquippedWeapon || !Durability.Consume(EquippedWeapon->DurabilityPerHit))
	{
		return false;
	}
	OnWeaponBroken.Broadcast();
	return true;
}

void UAREquipmentComponent::Repair(float Amount)
{
	if (EquippedWeapon && Durability.Repair(Amount))
	{
		OnWeaponRepaired.Broadcast();
	}
}

void UAREquipmentComponent::OnParkourTagChanged(const FGameplayTag /*Tag*/, int32 /*Count*/)
{
	UpdateMeshVisibility();
}

void UAREquipmentComponent::UpdateMeshVisibility()
{
	if (WeaponMesh)
	{
		const UAbilitySystemComponent* ASC = TagASC.Get();
		WeaponMesh->SetVisibility(!(ASC && ASC->HasMatchingGameplayTag(TAG_State_Parkouring.GetTag())));
	}
}

void UAREquipmentComponent::DestroyWeaponMesh()
{
	if (WeaponMesh)
	{
		WeaponMesh->DestroyComponent();
		WeaponMesh = nullptr;
	}
}
