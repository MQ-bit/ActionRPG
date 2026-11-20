// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ARLockOnMarkerWidget.generated.h"

/** The lock-on marker above a locked enemy: a red triangle. Built in code, so it needs no widget asset. */
UCLASS()
class ACTIONROGUELIKE_API UARLockOnMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
