@ The rest stop between floors (src/rogue.c): a nurse, a basics mart, a free prize and a move tutor.
@ Stepping on the exit mat leads into the next floor (or the finale).
Rogue_Stop_MapScripts::
	map_script MAP_SCRIPT_ON_FRAME_TABLE, Rogue_Stop_OnFrame
	.byte 0

Rogue_Stop_OnFrame:
	map_script_2 VAR_TEMP_2, 0, Rogue_Room_EventScript_EnterRoom
	.2byte 0

Rogue_Stop_EventScript_Exit::
	lockall
	special Rogue_BufferNextFloor
	msgbox gStringVar4, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Rogue_Stop_EventScript_Stay
	setvar VAR_0x8004, 1
	goto Rogue_Room_EventScript_Door
	end

Rogue_Stop_EventScript_Stay:
	closemessage
	applymovement OBJ_EVENT_ID_PLAYER, Common_Movement_WalkUp
	waitmovement 0
	releaseall
	end

Rogue_Stop_EventScript_Mart::
	lock
	faceplayer
	message gText_HowMayIServeYou
	waitmessage
	pokemart Rogue_Stop_Mart
	msgbox gText_PleaseComeAgain, MSGBOX_DEFAULT
	release
	end

	.align 2
Rogue_Stop_Mart:
	.2byte ITEM_POKE_BALL
	.2byte ITEM_GREAT_BALL
	.2byte ITEM_POTION
	.2byte ITEM_SUPER_POTION
	.2byte ITEM_ANTIDOTE
	.2byte ITEM_PARALYZE_HEAL
	.2byte ITEM_AWAKENING
	.2byte ITEM_NONE

@ One free prize per rest stop: three offers from different shelves
Rogue_Stop_EventScript_Prize::
	lock
	faceplayer
	special Rogue_HasTakenPrize
	goto_if_eq VAR_RESULT, TRUE, Rogue_Stop_EventScript_PrizeTaken
	msgbox Rogue_Stop_Text_PickPrize, MSGBOX_DEFAULT
	special Rogue_PushPrizeChoices
	dynmultistack 0, 0, FALSE, 3, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	goto_if_eq VAR_RESULT, MULTI_B_PRESSED, Rogue_Stop_EventScript_PrizeLater
	special Rogue_GivePrize
	playfanfare MUS_OBTAIN_ITEM
	msgbox Rogue_Stop_Text_GotPrize, MSGBOX_DEFAULT
	waitfanfare
	goto_if_eq VAR_0x8005, FALSE, Rogue_Stop_EventScript_PrizeDone
	msgbox Rogue_Stop_Text_GotMegaRing, MSGBOX_DEFAULT
Rogue_Stop_EventScript_PrizeDone:
	release
	end

Rogue_Stop_EventScript_PrizeLater:
	msgbox Rogue_Stop_Text_PrizeLater, MSGBOX_DEFAULT
	release
	end

Rogue_Stop_EventScript_PrizeTaken:
	msgbox Rogue_Stop_Text_PrizeTaken, MSGBOX_DEFAULT
	release
	end

@ A few random tutor moves the team can learn: money teaches one now; BP also lets every Pokémon of
@ that family relearn it in later runs
Rogue_Stop_EventScript_Tutor::
	lock
	faceplayer
	special Rogue_PushTutorMoves
	goto_if_eq VAR_RESULT, 0, Rogue_Stop_EventScript_TutorNothing
	msgbox Rogue_Stop_Text_TutorIntro, MSGBOX_DEFAULT
	dynmultistack 0, 0, FALSE, 3, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	goto_if_eq VAR_RESULT, MULTI_B_PRESSED, Rogue_Stop_EventScript_TutorDecline
	special Rogue_SelectTutorMove
	msgbox Rogue_Stop_Text_TutorPrice, MSGBOX_DEFAULT
	special Rogue_PushTutorPayments
	dynmultistack 0, 0, FALSE, 3, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	goto_if_eq VAR_RESULT, MULTI_B_PRESSED, Rogue_Stop_EventScript_TutorDecline
	copyvar VAR_0x8006, VAR_RESULT
	special Rogue_CanPayTutor
	goto_if_eq VAR_RESULT, FALSE, Rogue_Stop_EventScript_TutorCantPay
	msgbox Rogue_Stop_Text_TutorWhichMon, MSGBOX_DEFAULT
	special ChooseMonForMoveTutor
	lock
	faceplayer
	goto_if_eq VAR_RESULT, FALSE, Rogue_Stop_EventScript_TutorDecline
	special Rogue_PayTutor
	msgbox gStringVar4, MSGBOX_DEFAULT
	release
	end

Rogue_Stop_EventScript_TutorNothing:
	msgbox Rogue_Stop_Text_TutorNothing, MSGBOX_DEFAULT
	release
	end

Rogue_Stop_EventScript_TutorDecline:
	msgbox Rogue_Stop_Text_TutorDecline, MSGBOX_DEFAULT
	release
	end

Rogue_Stop_EventScript_TutorCantPay:
	msgbox Rogue_Stop_Text_TutorCantPay, MSGBOX_DEFAULT
	release
	end

Rogue_Stop_Text_PickPrize:
	.string "Well done on clearing the floor!\n"
	.string "Please, pick one prize. It's free!$"

Rogue_Stop_Text_GotPrize:
	.string "{PLAYER} received {STR_VAR_1}!$"

Rogue_Stop_Text_GotMegaRing:
	.string "A MEGA STONE needs a MEGA RING…\n"
	.string "Here, take this one too!\p"
	.string "{PLAYER} received a MEGA RING!$"

Rogue_Stop_Text_PrizeLater:
	.string "Take your time. Your prize will\n"
	.string "be waiting.$"

Rogue_Stop_Text_PrizeTaken:
	.string "Good luck on the next floor!\n"
	.string "There'll be another prize after it.$"

Rogue_Stop_Text_TutorIntro:
	.string "I teach moves. Today I can teach\n"
	.string "your POKéMON one of these.$"

Rogue_Stop_Text_TutorPrice:
	.string "{STR_VAR_1}! For ¥{STR_VAR_2}, I'll teach it\n"
	.string "once.\p"
	.string "For {STR_VAR_3} BP, every POKéMON of\n"
	.string "that kind can relearn it in later\l"
	.string "runs too.$"

Rogue_Stop_Text_TutorWhichMon:
	.string "Which POKéMON should learn it?$"

Rogue_Stop_Text_TutorNothing:
	.string "Hm… None of your POKéMON can learn\n"
	.string "anything I teach. Come again!$"

Rogue_Stop_Text_TutorDecline:
	.string "Another time, then.$"

Rogue_Stop_Text_TutorCantPay:
	.string "You can't pay for that, I'm afraid.$"
