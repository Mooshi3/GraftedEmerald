@ The player's base between runs (src/rogue.c): Prof. Birch answers questions, the scientist trades BP
@ for unlocks, and owned unlocks show as decorations (Rogue_DecorateHome). The exit starts a run.
Rogue_Home_MapScripts::
	map_script MAP_SCRIPT_ON_LOAD, Rogue_Home_OnLoad
	map_script MAP_SCRIPT_ON_FRAME_TABLE, Rogue_Home_OnFrame
	.byte 0

Rogue_Home_OnLoad:
	special Rogue_DecorateHome
	end

Rogue_Home_OnFrame:
	map_script_2 VAR_ROGUE_STATE, ROGUE_STATE_RUN_LOST, Rogue_Room_EventScript_RunLost
	map_script_2 VAR_ROGUE_STATE, ROGUE_STATE_RUN_WON, Rogue_Room_EventScript_RunWon
	map_script_2 VAR_ROGUE_INTRO, 0, Rogue_Room_EventScript_AskMeta
	map_script_2 VAR_ROGUE_ACHIEVE_SHOW, 1, Rogue_Room_EventScript_Achievement
	.2byte 0

Rogue_Home_EventScript_Exit::
	lockall
	msgbox Rogue_Home_Text_Leave, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Rogue_Home_EventScript_Stay
	setvar VAR_0x8004, 1
	goto Rogue_Room_EventScript_Door
	end

Rogue_Home_EventScript_Stay:
	closemessage
	applymovement OBJ_EVENT_ID_PLAYER, Common_Movement_WalkUp
	waitmovement 0
	releaseall
	end

@ Prof. Birch: how the game works, a topic at a time
Rogue_Home_EventScript_Birch::
	lock
	faceplayer
	msgbox Rogue_Home_Text_BirchHello, MSGBOX_DEFAULT
Rogue_Home_EventScript_BirchTopics:
	special Rogue_PushHelpTopics
	message Rogue_Home_Text_BirchAsk
	waitmessage
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	goto_if_eq VAR_RESULT, MULTI_B_PRESSED, Rogue_Home_EventScript_BirchBye
	special Rogue_GetHelpText
	goto_if_eq VAR_RESULT, FALSE, Rogue_Home_EventScript_BirchBye
	goto_if_eq VAR_RESULT, 2, Rogue_Home_EventScript_Achievements
	msgbox gStringVar4, MSGBOX_DEFAULT
	goto Rogue_Home_EventScript_BirchTopics

@ The achievements: done ones are marked; picking one shows how to get it and its reward
Rogue_Home_EventScript_Achievements:
	special Rogue_PushAchievements
	message Rogue_Home_Text_BirchAchievements
	waitmessage
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	goto_if_eq VAR_RESULT, MULTI_B_PRESSED, Rogue_Home_EventScript_BirchTopics
	special Rogue_GetAchievementText
	msgbox gStringVar4, MSGBOX_DEFAULT
	goto Rogue_Home_EventScript_Achievements

Rogue_Home_EventScript_BirchBye:
	msgbox Rogue_Home_Text_BirchBye, MSGBOX_DEFAULT
	release
	end

Rogue_Home_Text_BirchAchievements:
	.string "Each of these pays BP once.$"

Rogue_Home_Text_Leave:
	.string "Head out on a run?$"

Rogue_Home_Text_BirchHello:
	.string "PROF. BIRCH: Hello, {PLAYER}!\n"
	.string "Ask me anything.$"

Rogue_Home_Text_BirchAsk:
	.string "What do you want to know about?$"

Rogue_Home_Text_BirchBye:
	.string "Good luck out there!$"
