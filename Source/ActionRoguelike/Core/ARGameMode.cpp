// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ARGameMode.h"

#include "Character/ARCharacterBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

void AARGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	if (AARCharacterBase* Player = NewPlayer ? Cast<AARCharacterBase>(NewPlayer->GetPawn()) : nullptr)
	{
		Player->OnDied.AddUObject(this, &AARGameMode::HandlePlayerDied);
	}
}

void AARGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RestartTimer);
	Super::EndPlay(EndPlayReason);
}

void AARGameMode::HandlePlayerDied(AARCharacterBase* Player)
{
	// shortcut: reopens the level, no respawn or checkpoint; upgrade when a run/save flow exists.
	GetWorldTimerManager().SetTimer(RestartTimer, this, &AARGameMode::RestartLevel, RestartDelay);
}

void AARGameMode::RestartLevel()
{
	// The name without the PIE prefix, so it also works in the editor.
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}
