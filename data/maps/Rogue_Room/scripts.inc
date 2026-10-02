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
	map_script_2 VAR_TEMP_2, 0, Rogue_Room_EventScript_EnterRoom
	.2byte 0

Rogue_Room_EventScript_EnterRoom::
	setvar VAR_TEMP_2, 1
	special Rogue_GetRoomIntro
	goto_if_eq VAR_RESULT, FALSE, Rogue_Room_EventScript_EnterRoomEnd
	lockall
	msgbox gStringVar4, MSGBOX_DEFAULT
	releaseall
Rogue_Room_EventScript_EnterRoomEnd:
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
	special Rogue_TakeDoor
	goto_if_eq VAR_RESULT, ROGUE_DOOR_STARTED_RUN, Rogue_Room_EventScript_StartedRun
	goto_if_eq VAR_RESULT, ROGUE_DOOR_WON_RUN, Rogue_Room_EventScript_WonRun
	goto Rogue_Room_EventScript_Warp
	end

Rogue_Room_EventScript_StartedRun::
	playfanfare MUS_OBTAIN_ITEM
	msgbox Rogue_Room_Text_ReceivedStarter, MSGBOX_DEFAULT
	waitfanfare
	goto Rogue_Room_EventScript_Warp
	end

Rogue_Room_EventScript_WonRun::
	goto Rogue_Room_EventScript_Warp
	end

Rogue_Room_EventScript_Warp::
	warp MAP_ROGUE_ROOM, ROGUE_ENTRANCE_X, ROGUE_ENTRANCE_Y
	waitstate
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
	waitstate
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
	trainerbattle_single TRAINER_ROGUE_TRAINER, Rogue_Room_Text_TrainerIntro, Rogue_Room_Text_TrainerDefeat
	msgbox Rogue_Room_Text_TrainerPostBattle, MSGBOX_AUTOCLOSE
	end

Rogue_Room_EventScript_Boss::
	trainerbattle_single TRAINER_ROGUE_BOSS, Rogue_Room_Text_BossIntro, Rogue_Room_Text_BossDefeat, Rogue_Room_EventScript_BossDefeated
	msgbox Rogue_Room_Text_BossPostBattle, MSGBOX_AUTOCLOSE
	end

Rogue_Room_EventScript_BossDefeated::
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

Rogue_Room_EventScript_Guide::
	msgbox Rogue_Room_Text_Guide, MSGBOX_NPC
	end

Rogue_Room_Text_ReceivedStarter:
	.string "{PLAYER} received {STR_VAR_1}!$"

Rogue_Room_Text_TeamOverCap:
	.string "Your team can't take more than\n"
	.string "this. Choose one to let go.$"

Rogue_Room_Text_ConfirmRelease:
	.string "Let {STR_VAR_1} go?$"

Rogue_Room_Text_Released:
	.string "{STR_VAR_1} went back to the wild.$"

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
	.string "The doors are open now.$"

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
