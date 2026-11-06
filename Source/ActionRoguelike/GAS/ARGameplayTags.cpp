// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ARGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_Attack, "Ability.Attack", "Basic attack ability");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Attacking, "State.Attacking", "Owner is currently attacking");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_InputTag_Attack, "InputTag.Attack", "Input binding for the attack ability");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Attack_Hit, "Event.Attack.Hit", "Sent by the attack montage at the hit frame");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Damage, "Data.Damage", "SetByCaller damage magnitude (negative = damage)");
