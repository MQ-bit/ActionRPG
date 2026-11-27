// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "ARHitCameraShake.generated.h"

/** Short, heavy jolt for a melee impact (pitch and a little roll). Values live in the constructor; tune them in a Blueprint child. */
UCLASS()
class ACTIONROGUELIKE_API UARHitCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UARHitCameraShake(const FObjectInitializer& ObjectInitializer);
};
