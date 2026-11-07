// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ARGameMode.generated.h"

class AARCharacterBase;

/** The default pawn is assigned in a data-only Blueprint subclass. Reopens the level a few seconds after the player dies. */
UCLASS()
class ACTIONROGUELIKE_API AARGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Seconds between the player's death and the level reopening. */
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	float RestartDelay = 3.f;

private:
	void HandlePlayerDied(AARCharacterBase* Player);
	void RestartLevel();

	FTimerHandle RestartTimer;
};
