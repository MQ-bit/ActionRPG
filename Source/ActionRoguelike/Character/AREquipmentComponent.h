// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/ARDurability.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AREquipmentComponent.generated.h"

class UAbilitySystemComponent;
class UARWeaponData;
class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FAROnWeaponChanged, UARWeaponData* /*NewWeapon, null when unarmed*/);
DECLARE_MULTICAST_DELEGATE(FAROnWeaponEvent);

/**
 * The player's held weapon: attaches its mesh to the character mesh, keeps its durability and hides it during parkour
 * (the hand IK grabs the ledge). The basic attack reads the equipped weapon when it starts.
 */
UCLASS(ClassGroup = (AR), meta = (BlueprintSpawnableComponent))
class ACTIONROGUELIKE_API UAREquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAREquipmentComponent();

	/** Equips the weapon (replacing the current one). Equipping the weapon that was last held keeps its durability. */
	bool Equip(UARWeaponData* Weapon);
	void Unequip();
	/** Equips DefaultWeapon, or unequips when something is held. */
	void ToggleDefaultWeapon();

	/** Called by the attack once per swing that hit an enemy. Returns true when the weapon broke now. */
	bool OnWeaponHit();
	void Repair(float Amount);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UARWeaponData* GetEquippedWeapon() const { return EquippedWeapon; }
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsBroken() const { return EquippedWeapon && Durability.IsBroken(); }
	/** 0..1 for the durability bar; 1 for a weapon without durability. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetDurabilityFraction() const { return Durability.Fraction(); }
	const FARDurability& GetDurability() const { return Durability; }
	UFUNCTION(BlueprintPure, Category = "Weapon")
	UStaticMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

	FAROnWeaponChanged OnEquippedWeaponChanged;
	FAROnWeaponEvent OnWeaponBroken;
	FAROnWeaponEvent OnWeaponRepaired;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Weapon the toggle input equips. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<UARWeaponData> DefaultWeapon;

private:
	void OnParkourTagChanged(const FGameplayTag Tag, int32 Count);
	void UpdateMeshVisibility();
	void DestroyWeaponMesh();

	UPROPERTY(Transient)
	TObjectPtr<UARWeaponData> EquippedWeapon;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	// shortcut: one durability record (the last held weapon); a real inventory keeps one per item (crafting, TaskBoard 7.9).
	UPROPERTY(Transient)
	TObjectPtr<UARWeaponData> DurabilityWeapon;
	FARDurability Durability;

	TWeakObjectPtr<UAbilitySystemComponent> TagASC;
	FDelegateHandle ParkourTagHandle;
};
