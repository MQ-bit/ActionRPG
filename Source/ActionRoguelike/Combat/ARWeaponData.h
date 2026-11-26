// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ARWeaponData.generated.h"

class UAnimMontage;
class UStaticMesh;

UENUM(BlueprintType)
enum class EARWeaponType : uint8
{
	Melee,
	/** Reserved for the pistol; it will add its own abilities when it is designed. */
	Ranged,
};

/** One weapon: what is held in the hand and how the basic attack changes while it is equipped. */
UCLASS(BlueprintType)
class ACTIONROGUELIKE_API UARWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EARWeaponType WeaponType = EARWeaponType::Melee;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Mesh")
	TObjectPtr<UStaticMesh> Mesh;

	/** Bone or socket of the character mesh the weapon mesh is attached to. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Mesh")
	FName AttachSocket = TEXT("weapon_r");

	/** Grip correction relative to the socket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Mesh")
	FTransform AttachOffset;

	/** Combo used while equipped, one montage per step. Empty = the unarmed combo of the attack ability. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attack")
	TArray<TObjectPtr<UAnimMontage>> ComboMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attack", meta = (ClampMin = "0"))
	float Damage = 30.f;

	/** Distance from the avatar to the center of the hit sphere (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attack", meta = (ClampMin = "0"))
	float HitRange = 160.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attack", meta = (ClampMin = "0"))
	float HitRadius = 100.f;

	/** 0 = no durability. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Durability", meta = (ClampMin = "0"))
	float MaxDurability = 20.f;

	/** Durability lost per swing that hits at least one enemy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Durability", meta = (ClampMin = "0"))
	float DurabilityPerHit = 1.f;

	/** Damage multiplier while the weapon is broken (it stays in the hand until repaired). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Durability", meta = (ClampMin = "0", ClampMax = "1"))
	float BrokenDamageScale = 0.25f;
};

/** The values one basic attack activation uses. */
struct FARAttackValues
{
	TArray<TObjectPtr<UAnimMontage>> Montages;
	float Damage = 0.f;
	float HitRange = 0.f;
	float HitRadius = 0.f;
};

namespace ARWeapon
{
	/** Attack values for one activation: the ability's own values, overridden by the equipped weapon (if any). */
	inline FARAttackValues ResolveAttackValues(const FARAttackValues& Unarmed, const UARWeaponData* Weapon, bool bBroken)
	{
		FARAttackValues Out = Unarmed;
		if (!Weapon)
		{
			return Out;
		}
		if (!Weapon->ComboMontages.IsEmpty() && Weapon->ComboMontages[0])
		{
			Out.Montages = Weapon->ComboMontages;
		}
		Out.Damage = Weapon->Damage * (bBroken ? Weapon->BrokenDamageScale : 1.f);
		Out.HitRange = Weapon->HitRange;
		Out.HitRadius = Weapon->HitRadius;
		return Out;
	}
}
