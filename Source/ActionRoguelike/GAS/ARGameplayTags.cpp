// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ARGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_Attack, "Ability.Attack", "Basic attack ability");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_Parkour, "Ability.Parkour", "Vault or mantle over an obstacle in front");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Parkouring, "State.Parkouring", "Owner is vaulting or mantling");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Attacking, "State.Attacking", "Owner is currently attacking");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Dead, "State.Dead", "Owner is dead; abilities cannot be activated");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_LockedOn, "State.LockedOn", "Owner holds a lock-on target");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_InputTag_Attack, "InputTag.Attack", "Input binding for the attack ability");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Attack_Hit, "Event.Attack.Hit", "Sent by the attack montage at the hit frame");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Damage, "Data.Damage", "SetByCaller damage magnitude (negative = damage)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_HitReact, "Ability.HitReact", "Short reaction after taking damage or being parried");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_Block, "Ability.Block", "Hold to block, parry at the start of the block");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_HitReact, "State.HitReact", "Owner is reacting to a hit");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Blocking, "State.Blocking", "Owner is blocking");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_InputTag_Block, "InputTag.Block", "Input binding for the block ability (held)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Damaged, "Event.Damaged", "Sent to a target that took damage; Instigator = attacker");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Parried, "Event.Parried", "Sent to an attacker whose hit was parried; Instigator = the parrying target");
