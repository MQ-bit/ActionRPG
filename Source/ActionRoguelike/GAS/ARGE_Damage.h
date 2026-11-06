// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ARGE_Damage.generated.h"

/** Instant Health change. Magnitude comes from the SetByCaller tag Data.Damage (negative = damage). */
UCLASS()
class ACTIONROGUELIKE_API UARGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UARGE_Damage();
};
