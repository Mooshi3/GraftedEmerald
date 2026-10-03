@ Inside a Rogue room's building; the exit returns to the room (a dynamic warp, set by Rogue_OnTransition)
Rogue_Center_MapScripts::
	.byte 0

@ The Favorite unlock: make one of the team a starter choice for every run
Rogue_Center_EventScript_Pokefan::
	lock
	faceplayer
	special Rogue_GetFavorite
	goto_if_eq VAR_RESULT, FALSE, Rogue_Center_EventScript_PokefanChat
	goto_if_eq VAR_0x8005, FALSE, Rogue_Center_EventScript_PokefanAsk
	msgbox Rogue_Center_Text_CurrentFavorite, MSGBOX_DEFAULT
Rogue_Center_EventScript_PokefanAsk:
	msgbox Rogue_Center_Text_PickFavorite, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Rogue_Center_EventScript_PokefanDecline
	msgbox Rogue_Center_Text_WhichOne, MSGBOX_DEFAULT
	special ChoosePartyMon
	goto_if_ge VAR_0x8004, PARTY_SIZE, Rogue_Center_EventScript_PokefanDecline
	special Rogue_SetFavorite
	msgbox Rogue_Center_Text_FavoriteSet, MSGBOX_DEFAULT
	release
	end

Rogue_Center_EventScript_PokefanDecline:
	msgbox Rogue_Center_Text_PokefanDecline, MSGBOX_DEFAULT
	release
	end

Rogue_Center_EventScript_PokefanChat:
	msgbox Rogue_Center_Text_PokefanChat, MSGBOX_DEFAULT
	release
	end

Rogue_Center_Text_PokefanChat:
	.string "Every TRAINER has a favorite\n"
	.string "POKéMON. Do you have one?$"

Rogue_Center_Text_CurrentFavorite:
	.string "Your favorite is {STR_VAR_1}!\n"
	.string "It'll wait for you at every start.$"

Rogue_Center_Text_PickFavorite:
	.string "Is one of your POKéMON here\n"
	.string "your new favorite?$"

Rogue_Center_Text_WhichOne:
	.string "Which one?$"

Rogue_Center_Text_FavoriteSet:
	.string "{STR_VAR_1}! A fine choice!\p"
	.string "Next run, you can pick a young\n"
	.string "{STR_VAR_1} as your starter.$"

Rogue_Center_Text_PokefanDecline:
	.string "Oh, that's fine. Take your time.$"
