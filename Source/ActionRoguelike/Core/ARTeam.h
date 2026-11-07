// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Team ids used with IGenericTeamAgentInterface. Kept as plain numbers so the rules can be unit tested. */
namespace ARTeam
{
	constexpr uint8 Player = 0;
	constexpr uint8 Enemy = 1;
	/** Same value as FGenericTeamId::NoTeam. */
	constexpr uint8 NoTeam = 255;

	/** Two different teams are hostile; a character without a team is hostile to nobody. */
	inline bool IsHostile(uint8 TeamA, uint8 TeamB)
	{
		return TeamA != NoTeam && TeamB != NoTeam && TeamA != TeamB;
	}
}
