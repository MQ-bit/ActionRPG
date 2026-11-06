// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ARAnimNotify_AttackHit.generated.h"

/** Place on an attack montage at the hit frame. Sends Event.Attack.Hit to the montage owner. */
UCLASS(meta = (DisplayName = "AR Attack Hit"))
class ACTIONROGUELIKE_API UARAnimNotify_AttackHit : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override { return TEXT("AttackHit"); }
};
