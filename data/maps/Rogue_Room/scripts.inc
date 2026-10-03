@ Grafted Rogue: every room of a run is this one map, rebuilt from the room's seed (src/rogue.c).
@ Rogue_OnTransition places the objects before they spawn; Rogue_OnLoad draws the trees, grass and
@ doors (it runs again after every battle, so the room is drawn the same way each time).

Rogue_Room_MapScripts::
	map_script MAP_SCRIPT_ON_TRANSITION, Rogue_Room_OnTransition
	map_script MAP_SCRIPT_ON_LOAD, Rogue_Room_OnLoad
	map_script MAP_SCRIPT_ON_FRAME_TABLE, Rogue_Room_OnFrame
	.byte 0

Rogue_Room_OnTransition:
	special Rogue_OnTransition
	end

Rogue_Room_OnLoad:
	special Rogue_OnLoad
	end

Rogue_Room_OnFrame:
	map_script_2 VAR_ROGUE_STATE, ROGUE_STATE_RUN_LOST, Rogue_Room_EventScript_RunLost
	map_script_2 VAR_ROGUE_STATE, ROGUE_STATE_RUN_WON, Rogue_Room_EventScript_RunWon
	map_script_2 VAR_ROGUE_INTRO, 0, Rogue_Room_EventScript_AskMeta
	map_script_2 VAR_TEMP_2, 0, Rogue_Room_EventScript_EnterRoom
	map_script_2 VAR_TEMP_4, 1, Rogue_Room_EventScript_AskFainted
	map_script_2 VAR_TEMP_5, 1, Rogue_Room_EventScript_CatchBonus
	.2byte 0

@ The Catch bonus unlock: a caught Pokémon left a graft piece (Rogue_OnBattleEnd)
Rogue_Room_EventScript_CatchBonus::
	lockall
	special Rogue_BufferCatchBonus
	playfanfare MUS_OBTAIN_ITEM
	msgbox Rogue_Room_Text_CatchBonus, MSGBOX_DEFAULT
	waitfanfare
	releaseall
	end

Rogue_Room_Text_CatchBonus:
	.string "The new POKéMON left a graft piece\n"
	.string "behind: {STR_VAR_1}!$"

@ A new save's first question
Rogue_Room_EventScript_AskMeta::
	lockall
	msgbox Rogue_Room_Text_AskMeta, MSGBOX_YESNO
	copyvar VAR_0x8004, VAR_RESULT
	special Rogue_SetMetaProgression
	goto_if_eq VAR_0x8004, NO, Rogue_Room_EventScript_MetaOff
	msgbox Rogue_Room_Text_MetaOn, MSGBOX_DEFAULT
	releaseall
	end
Rogue_Room_EventScript_MetaOff:
	msgbox Rogue_Room_Text_MetaOff, MSGBOX_DEFAULT
	releaseall
	end

Rogue_Room_EventScript_EnterRoom::
	setvar VAR_TEMP_2, 1
	goto_if_eq VAR_ROGUE_STATE, ROGUE_STATE_HUB, Rogue_Room_EventScript_EnterRoomNoPopUp
	special ShowMapNamePopup
Rogue_Room_EventScript_EnterRoomNoPopUp:
	special Rogue_GetRoomIntro
	goto_if_eq VAR_RESULT, FALSE, Rogue_Room_EventScript_EnterRoomEnd
	lockall
	msgbox gStringVar4, MSGBOX_DEFAULT
	releaseall
Rogue_Room_EventScript_EnterRoomEnd:
	end

@ After a battle: what to do with each Pokémon that fainted
Rogue_Room_EventScript_AskFainted::
	lockall
	setvar VAR_TEMP_4, 0
Rogue_Room_EventScript_AskFaintedLoop:
	special Rogue_FindFainted
	goto_if_eq VAR_RESULT, FALSE, Rogue_Room_EventScript_AskFaintedEnd
	message Rogue_Room_Text_AskFainted
	waitmessage
	special Rogue_PushFaintedChoices
	dynmultistack 0, 0, TRUE, 3, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	copyvar VAR_0x8005, VAR_RESULT
	special Rogue_ResolveFainted
	msgbox gStringVar4, MSGBOX_DEFAULT
	goto Rogue_Room_EventScript_AskFaintedLoop
Rogue_Room_EventScript_AskFaintedEnd:
	releaseall
	end

Rogue_Room_EventScript_RunLost::
	lockall
	setvar VAR_ROGUE_STATE, ROGUE_STATE_HUB
	msgbox Rogue_Room_Text_RunLost, MSGBOX_DEFAULT
	releaseall
	end

Rogue_Room_EventScript_RunWon::
	lockall
	setvar VAR_ROGUE_STATE, ROGUE_STATE_HUB
	playfanfare MUS_OBTAIN_BADGE
	msgbox Rogue_Room_Text_RunWon, MSGBOX_DEFAULT
	waitfanfare
	releaseall
	end

@ Doors: the triggers sit on the first tile of each opening in the north tree line
Rogue_Room_EventScript_Door0::
	setvar VAR_0x8004, 0
	goto Rogue_Room_EventScript_Door
	end

Rogue_Room_EventScript_Door1::
	setvar VAR_0x8004, 1
	goto Rogue_Room_EventScript_Door
	end

Rogue_Room_EventScript_Door2::
	setvar VAR_0x8004, 2
	goto Rogue_Room_EventScript_Door
	end

Rogue_Room_EventScript_Door::
	lockall
	special Rogue_CanLeaveRoom
	goto_if_eq VAR_RESULT, FALSE, Rogue_Room_EventScript_DoorBlocked
	setvar VAR_0x8005, 0
	setvar VAR_0x8006, 0
	setvar VAR_0x8007, 0
Rogue_Room_EventScript_TakeDoor::
	special Rogue_TakeDoor
	goto_if_eq VAR_RESULT, ROGUE_DOOR_ASK_MODE, Rogue_Room_EventScript_AskMode
	goto_if_eq VAR_RESULT, ROGUE_DOOR_ASK_CHALLENGE, Rogue_Room_EventScript_AskChallenge
	goto_if_eq VAR_RESULT, ROGUE_DOOR_CHOOSE_STARTER, Rogue_Room_EventScript_ChooseStarter
	goto_if_eq VAR_RESULT, ROGUE_DOOR_STARTED_RUN, Rogue_Room_EventScript_StartedRun
	goto_if_eq VAR_RESULT, ROGUE_DOOR_WON_RUN, Rogue_Room_EventScript_WonRun
	goto Rogue_Room_EventScript_Warp
	end

Rogue_Room_EventScript_AskMode::
	msgbox Rogue_Room_Text_AskHardMode, MSGBOX_YESNO
	setvar VAR_0x8005, 1
	goto_if_eq VAR_RESULT, NO, Rogue_Room_EventScript_TakeDoor
	setvar VAR_0x8005, 2
	goto Rogue_Room_EventScript_TakeDoor
	end

Rogue_Room_EventScript_AskChallenge::
	message Rogue_Room_Text_AskChallenge
	waitmessage
	special Rogue_PushChallengeChoices
	dynmultistack 0, 0, TRUE, 3, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	copyvar VAR_0x8006, VAR_RESULT
	goto_if_ne VAR_0x8006, ROGUE_CHALLENGE_MONOTYPE, Rogue_Room_EventScript_TakeDoor
	message Rogue_Room_Text_AskMonotype
	waitmessage
	special Rogue_PushTypeChoices
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	goto_if_eq VAR_RESULT, MULTI_B_PRESSED, Rogue_Room_EventScript_AskChallenge
	copyvar VAR_0x8007, VAR_RESULT
	goto Rogue_Room_EventScript_TakeDoor
	end

Rogue_Room_EventScript_ChooseStarter::
	msgbox Rogue_Room_Text_ChooseStarter, MSGBOX_DEFAULT
	special Rogue_PushStarterChoices
	dynmultistack 0, 0, TRUE, 4, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	special Rogue_GiveChosenStarter
	goto Rogue_Room_EventScript_StartedRun
	end

Rogue_Room_EventScript_StartedRun::
	playfanfare MUS_OBTAIN_ITEM
	msgbox Rogue_Room_Text_ReceivedStarter, MSGBOX_DEFAULT
	waitfanfare
	special Rogue_HasHeirloom
	goto_if_eq VAR_RESULT, FALSE, Rogue_Room_EventScript_Warp
	msgbox Rogue_Room_Text_ChooseHeirloom, MSGBOX_DEFAULT
	special Rogue_PushHeirloomChoices
	dynmultistack 0, 0, TRUE, 3, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	special Rogue_GiveHeirloom
	msgbox Rogue_Room_Text_GotHeirloom, MSGBOX_DEFAULT
	goto Rogue_Room_EventScript_Warp
	end

Rogue_Room_EventScript_WonRun::
	goto Rogue_Room_EventScript_Warp
	end

Rogue_Room_EventScript_Warp::
	special Rogue_WarpToRoom
	releaseall
	end

Rogue_Room_EventScript_DoorBlocked::
	msgbox gStringVar4, MSGBOX_DEFAULT
	closemessage
	applymovement OBJ_EVENT_ID_PLAYER, Rogue_Room_Movement_StepBack
	waitmovement 0
	releaseall
	end

Rogue_Room_Movement_StepBack:
	walk_down
	step_end

@ Room contents
Rogue_Room_EventScript_WildMon::
	lock
	special Rogue_PrepareWildMon
	dowildbattle
	removeobject VAR_LAST_TALKED
	call Rogue_Room_EventScript_CheckTeamCap
	release
	end

@ A catch past the team cap: choose someone to let go (the new catch included)
Rogue_Room_EventScript_CheckTeamCap::
	special Rogue_IsTeamOverCap
	goto_if_eq VAR_RESULT, FALSE, Rogue_Room_EventScript_CheckTeamCapEnd
	msgbox Rogue_Room_Text_TeamOverCap, MSGBOX_DEFAULT
	closemessage
Rogue_Room_EventScript_ChooseRelease::
	special ChoosePartyMon
	goto_if_ge VAR_0x8004, PARTY_SIZE, Rogue_Room_EventScript_ChooseRelease
	bufferpartymonnick STR_VAR_1, VAR_0x8004
	msgbox Rogue_Room_Text_ConfirmRelease, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Rogue_Room_EventScript_ChooseRelease
	special Rogue_ReleasePartyMon
	msgbox Rogue_Room_Text_Released, MSGBOX_DEFAULT
	closemessage
	goto Rogue_Room_EventScript_CheckTeamCap
Rogue_Room_EventScript_CheckTeamCapEnd:
	return

Rogue_Room_EventScript_ItemBall::
	special Rogue_PrepareItemBall
	callstd STD_FIND_ITEM
	end

Rogue_Room_EventScript_Trainer::
	trainerbattle_single TRAINER_ROGUE_TRAINER, Rogue_Room_Text_TrainerIntro, Rogue_Room_Text_TrainerDefeat, Rogue_Room_EventScript_TrainerDefeated
	msgbox Rogue_Room_Text_TrainerPostBattle, MSGBOX_AUTOCLOSE
	end

Rogue_Room_EventScript_TrainerDefeated::
	special Rogue_OpenDoors
	msgbox Rogue_Room_Text_TrainerPostBattle, MSGBOX_DEFAULT
	release
	end

@ The small trees blocking a guarded room's doors
Rogue_Room_EventScript_Thicket::
	msgbox Rogue_Room_Text_Thicket, MSGBOX_SIGN
	end

Rogue_Room_EventScript_Boss::
	trainerbattle_single TRAINER_ROGUE_BOSS, Rogue_Room_Text_BossIntro, Rogue_Room_Text_BossDefeat, Rogue_Room_EventScript_BossDefeated
	msgbox Rogue_Room_Text_BossPostBattle, MSGBOX_AUTOCLOSE
	end

Rogue_Room_EventScript_BossDefeated::
	special Rogue_OpenDoors
	special Rogue_OnBossDefeated
	playfanfare MUS_OBTAIN_BADGE
	msgbox gStringVar4, MSGBOX_DEFAULT
	waitfanfare
	release
	end

Rogue_Room_EventScript_Shop::
	lock
	faceplayer
	message gText_HowMayIServeYou
	waitmessage
	pokemart Rogue_Room_Mart
	msgbox gText_PleaseComeAgain, MSGBOX_DEFAULT
	release
	end

	.align 2
Rogue_Room_Mart:
	.2byte ITEM_POKE_BALL
	.2byte ITEM_GREAT_BALL
	.2byte ITEM_PATCH_BALL
	.2byte ITEM_GRAFT_BALL
	.2byte ITEM_SPLICE_BALL
	.2byte ITEM_GRAFT_SPLICER
	.2byte ITEM_POTION
	.2byte ITEM_SUPER_POTION
	.2byte ITEM_REVIVE
	.2byte ITEM_FULL_HEAL
	.2byte ITEM_UNGRAFTER
	pokemartlistend

Rogue_Room_EventScript_Nurse::
	lock
	faceplayer
	msgbox Rogue_Room_Text_NurseHeal, MSGBOX_DEFAULT
	closemessage
	special HealPlayerParty
	playfanfare MUS_HEAL
	waitfanfare
	msgbox Rogue_Room_Text_NurseDone, MSGBOX_DEFAULT
	release
	end

Rogue_Room_EventScript_UnlockShop::
	lock
	faceplayer
	special Rogue_BufferBP
	msgbox Rogue_Room_Text_UnlockShopIntro, MSGBOX_DEFAULT
Rogue_Room_EventScript_UnlockShopLoop::
	special Rogue_PushUnlockChoices
	goto_if_eq VAR_RESULT, 0, Rogue_Room_EventScript_UnlockShopSoldOut
	dynmultistack 0, 0, FALSE, 5, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	goto_if_eq VAR_RESULT, MULTI_B_PRESSED, Rogue_Room_EventScript_UnlockShopEnd
	copyvar VAR_0x8004, VAR_RESULT
	special Rogue_DescribeUnlock
	msgbox gStringVar4, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Rogue_Room_EventScript_UnlockShopLoop
	special Rogue_BuyUnlock
	goto_if_eq VAR_RESULT, FALSE, Rogue_Room_EventScript_UnlockShopNoBP
	playfanfare MUS_OBTAIN_ITEM
	msgbox gStringVar4, MSGBOX_DEFAULT
	waitfanfare
	goto Rogue_Room_EventScript_UnlockShopLoop
Rogue_Room_EventScript_UnlockShopNoBP:
	msgbox gStringVar4, MSGBOX_DEFAULT
	goto Rogue_Room_EventScript_UnlockShopLoop
Rogue_Room_EventScript_UnlockShopSoldOut:
	msgbox Rogue_Room_Text_UnlockShopSoldOut, MSGBOX_DEFAULT
Rogue_Room_EventScript_UnlockShopEnd:
	release
	end

Rogue_Room_EventScript_GiftPieces::
	lock
	faceplayer
	goto_if_eq VAR_TEMP_3, 1, Rogue_Room_EventScript_GiftPiecesDone
	msgbox Rogue_Room_Text_GiftIntro, MSGBOX_DEFAULT
	special Rogue_GiveGiftPieces
	setvar VAR_TEMP_3, 1
	playfanfare MUS_OBTAIN_ITEM
	msgbox Rogue_Room_Text_GiftGot, MSGBOX_DEFAULT
	waitfanfare
	release
	end
Rogue_Room_EventScript_GiftPiecesDone:
	msgbox Rogue_Room_Text_GiftDone, MSGBOX_DEFAULT
	release
	end

Rogue_Room_EventScript_Merchant::
	lock
	faceplayer
	msgbox Rogue_Room_Text_MerchantIntro, MSGBOX_DEFAULT
	pokemart Rogue_Room_MerchantMart
	msgbox gText_PleaseComeAgain, MSGBOX_DEFAULT
	release
	end

	.align 2
Rogue_Room_MerchantMart:
	.2byte ITEM_SPLICE_BALL
	.2byte ITEM_ULTRA_BALL
	.2byte ITEM_RARE_CANDY
	.2byte ITEM_ABILITY_CAPSULE
	.2byte ITEM_LEFTOVERS
	.2byte ITEM_LIFE_ORB
	.2byte ITEM_CHOICE_BAND
	.2byte ITEM_CHOICE_SPECS
	.2byte ITEM_FOCUS_SASH
	.2byte ITEM_EVIOLITE
	pokemartlistend

Rogue_Room_EventScript_Guide::
	msgbox Rogue_Room_Text_Guide, MSGBOX_NPC
	end

Rogue_Room_Text_AskMeta:
	.string "Before you start: do you want\n"
	.string "meta-progression?\p"
	.string "Bosses pay BP you can spend on\n"
	.string "unlocks that carry over to every\l"
	.string "run after.\p"
	.string "Without it, everything is unlocked\n"
	.string "from the start. This can't be\l"
	.string "changed later. Use meta-progression?$"

Rogue_Room_Text_MetaOn:
	.string "Meta-progression is on. The scientist\n"
	.string "here trades BP for unlocks.$"

Rogue_Room_Text_MetaOff:
	.string "Meta-progression is off. Everything\n"
	.string "is unlocked. Good luck!$"

Rogue_Room_Text_AskChallenge:
	.string "Take on a challenge?\n"
	.string "Bosses pay half again in BP.$"

Rogue_Room_Text_AskMonotype:
	.string "Which type? Only Pokémon of it can\n"
	.string "battle. Your starter gets it grafted.$"

Rogue_Room_Text_AskHardMode:
	.string "Take on HARD mode?\n"
	.string "Eight floors and the ELITE FOUR.$"

Rogue_Room_Text_ChooseStarter:
	.string "Choose your partner!$"

Rogue_Room_Text_ChooseHeirloom:
	.string "Pick a graft piece to bring along.$"

Rogue_Room_Text_GotHeirloom:
	.string "{STR_VAR_1} was added to the\n"
	.string "GRAFT BAG.$"

Rogue_Room_Text_UnlockShopIntro:
	.string "I trade BP for things that help\n"
	.string "every run after this one.\p"
	.string "You have {STR_VAR_1} BP.$"

Rogue_Room_Text_UnlockShopSoldOut:
	.string "You've unlocked everything I have!$"

Rogue_Room_Text_GiftIntro:
	.string "Ah, a traveler! I've got pieces\n"
	.string "I'll never use. Here, take these.$"

Rogue_Room_Text_GiftGot:
	.string "{PLAYER} got {STR_VAR_1}\n"
	.string "and {STR_VAR_2}!$"

Rogue_Room_Text_GiftDone:
	.string "Put them to good use!$"

Rogue_Room_Text_MerchantIntro:
	.string "Rare goods, for those who wander\n"
	.string "this far. Have a look.$"

Rogue_Room_Text_ReceivedStarter:
	.string "{PLAYER} received {STR_VAR_1}!$"

Rogue_Room_Text_TeamOverCap:
	.string "Your team can't take more than\n"
	.string "this. Choose one to let go.$"

Rogue_Room_Text_ConfirmRelease:
	.string "Let {STR_VAR_1} go?$"

Rogue_Room_Text_Released:
	.string "{STR_VAR_1} went back to the wild.$"

Rogue_Room_Text_AskFainted:
	.string "{STR_VAR_1} fainted.\n"
	.string "What will you do?$"

Rogue_Room_Text_RunLost:
	.string "Your team is gone…\n"
	.string "The run is over.\p"
	.string "Go through the gate to try again.$"

Rogue_Room_Text_RunWon:
	.string "You beat every boss!\n"
	.string "The run is won!$"

Rogue_Room_Text_TrainerIntro:
	.string "Nobody gets past me\n"
	.string "without a battle!$"

Rogue_Room_Text_TrainerDefeat:
	.string "Fine, go on through.$"

Rogue_Room_Text_TrainerPostBattle:
	.string "The way is clear. Go on.$"

Rogue_Room_Text_Thicket:
	.string "Thick brush blocks the path.\n"
	.string "It'll clear once the trainer here\l"
	.string "is beaten.$"

Rogue_Room_Text_BossIntro:
	.string "So you made it this far.\n"
	.string "Let's see what your team is made of!$"

Rogue_Room_Text_BossDefeat:
	.string "You've earned the way forward.$"

Rogue_Room_Text_BossPostBattle:
	.string "Keep going. It only gets harder.$"

Rogue_Room_Text_NurseHeal:
	.string "You look tired.\n"
	.string "Let me heal your team.$"

Rogue_Room_Text_NurseDone:
	.string "Your team is back in shape.\n"
	.string "Good luck out there!$"

Rogue_Room_Text_Guide:
	.string "Every room has a few doors. The sign\n"
	.string "over each one tells what's behind it.\p"
	.string "Pokémon that faint are gone for good.\n"
	.string "Lose your whole team and the run ends.\p"
	.string "Go through the gate when you're ready!$"
