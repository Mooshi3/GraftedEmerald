BattleFrontier_ExchangeServiceCorner_MapScripts::
	.byte 0

.macro setitemandprice item:req, price:req
	setvar VAR_0x8009, \item
	setvar VAR_0x8008, \price
.endm

BattleFrontier_ExchangeServiceCorner_EventScript_ClerkWelcome::
	msgbox BattleFrontier_ExchangeServiceCorner_Text_WelcomePleaseChoosePrize, MSGBOX_DEFAULT
	special ShowBattlePointsWindow
	return

BattleFrontier_ExchangeServiceCorner_EventScript_ClerkGoodbye::
	msgbox BattleFrontier_ExchangeServiceCorner_Text_ThankYouVisitWithPoints, MSGBOX_DEFAULT
	special CloseBattlePointsWindow
	release
	end

BattleFrontier_ExchangeServiceCorner_EventScript_TryPurchasePrize::
	specialvar VAR_TEMP_1, GetFrontierBattlePoints
	goto_if_ge VAR_TEMP_1, VAR_0x8008, BattleFrontier_ExchangeServiceCorner_EventScript_TryGivePrize
	msgbox BattleFrontier_ExchangeServiceCorner_Text_DontHaveEnoughPoints, MSGBOX_DEFAULT
	goto_if_eq VAR_TEMP_2, EXCHANGE_CORNER_DECOR1_CLERK, BattleFrontier_ExchangeServiceCorner_EventScript_ChooseDecor1
	goto_if_eq VAR_TEMP_2, EXCHANGE_CORNER_DECOR2_CLERK, BattleFrontier_ExchangeServiceCorner_EventScript_ChooseDecor2
	goto_if_eq VAR_TEMP_2, EXCHANGE_CORNER_VITAMIN_CLERK, BattleFrontier_ExchangeServiceCorner_EventScript_ChooseVitamin
	goto BattleFrontier_ExchangeServiceCorner_EventScript_ChooseHoldItem
	end

BattleFrontier_ExchangeServiceCorner_EventScript_TryGivePrize::
	goto_if_eq VAR_TEMP_2, EXCHANGE_CORNER_DECOR1_CLERK, BattleFrontier_ExchangeServiceCorner_EventScript_TryGiveDecor
	goto_if_eq VAR_TEMP_2, EXCHANGE_CORNER_DECOR2_CLERK, BattleFrontier_ExchangeServiceCorner_EventScript_TryGiveDecor
	goto BattleFrontier_ExchangeServiceCorner_EventScript_TryGiveItem
	end

BattleFrontier_ExchangeServiceCorner_EventScript_TryGiveDecor::
	checkdecorspace VAR_0x8009
	goto_if_eq VAR_RESULT, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_NoRoomForDecor
	copyvar VAR_0x8004, VAR_0x8008
	special TakeFrontierBattlePoints
	adddecoration VAR_0x8009
	special UpdateBattlePointsWindow
	playse SE_SHOP
	msgbox BattleFrontier_ExchangeServiceCorner_Text_WellSendItToPC, MSGBOX_DEFAULT
	goto_if_eq VAR_TEMP_2, EXCHANGE_CORNER_DECOR1_CLERK, BattleFrontier_ExchangeServiceCorner_EventScript_ChooseDecor1
	goto BattleFrontier_ExchangeServiceCorner_EventScript_ChooseDecor2
	end

BattleFrontier_ExchangeServiceCorner_EventScript_NoRoomForDecor::
	msgbox BattleFrontier_ExchangeServiceCorner_Text_PCIsFull, MSGBOX_DEFAULT
	special CloseBattlePointsWindow
	release
	end

BattleFrontier_ExchangeServiceCorner_EventScript_TryGiveItem::
	checkitemspace VAR_0x8009
	goto_if_eq VAR_RESULT, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_BagFull
	copyvar VAR_0x8004, VAR_0x8008
	special TakeFrontierBattlePoints
	additem VAR_0x8009
	special UpdateBattlePointsWindow
	playse SE_SHOP
	msgbox BattleFrontier_ExchangeServiceCorner_Text_HereIsYourPrize, MSGBOX_DEFAULT
	goto_if_eq VAR_TEMP_2, EXCHANGE_CORNER_VITAMIN_CLERK, BattleFrontier_ExchangeServiceCorner_EventScript_ChooseVitamin
	goto BattleFrontier_ExchangeServiceCorner_EventScript_ChooseHoldItem
	end

BattleFrontier_ExchangeServiceCorner_EventScript_BagFull::
	msgbox BattleFrontier_ExchangeServiceCorner_Text_DontHaveSpaceToHoldIt, MSGBOX_DEFAULT
	special CloseBattlePointsWindow
	release
	end

BattleFrontier_ExchangeServiceCorner_EventScript_DecorClerk1::
	lock
	faceplayer
	setvar VAR_TEMP_2, EXCHANGE_CORNER_DECOR1_CLERK
	call BattleFrontier_ExchangeServiceCorner_EventScript_ClerkWelcome
	goto BattleFrontier_ExchangeServiceCorner_EventScript_ChooseDecor1
	end

BattleFrontier_ExchangeServiceCorner_EventScript_ChooseDecor1::
	setvar VAR_0x8004, SCROLL_MULTI_BF_EXCHANGE_CORNER_DECOR_VENDOR_1
	special ShowFrontierExchangeCornerItemIconWindow
	special ShowScrollableMultichoice
	special CloseFrontierExchangeCornerItemIconWindow
	switch VAR_RESULT
	case 0, BattleFrontier_ExchangeServiceCorner_EventScript_KissPoster
	case 1, BattleFrontier_ExchangeServiceCorner_EventScript_KissCushion
	case 2, BattleFrontier_ExchangeServiceCorner_EventScript_SmoochumDoll
	case 3, BattleFrontier_ExchangeServiceCorner_EventScript_TogepiDoll
	case 4, BattleFrontier_ExchangeServiceCorner_EventScript_MeowthDoll
	case 5, BattleFrontier_ExchangeServiceCorner_EventScript_ClefairyDoll
	case 6, BattleFrontier_ExchangeServiceCorner_EventScript_DittoDoll
	case 7, BattleFrontier_ExchangeServiceCorner_EventScript_CyndaquilDoll
	case 8, BattleFrontier_ExchangeServiceCorner_EventScript_ChikoritaDoll
	case 9, BattleFrontier_ExchangeServiceCorner_EventScript_TotodileDoll
	case 10, BattleFrontier_ExchangeServiceCorner_EventScript_ClerkGoodbye
	case MULTI_B_PRESSED, BattleFrontier_ExchangeServiceCorner_EventScript_ClerkGoodbye
	end

BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor1::
	bufferdecorationname STR_VAR_1, VAR_0x8009
	msgbox BattleFrontier_ExchangeServiceCorner_Text_ConfirmPurchase, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, BattleFrontier_ExchangeServiceCorner_EventScript_ChooseDecor1
	goto BattleFrontier_ExchangeServiceCorner_EventScript_TryPurchasePrize
	end

BattleFrontier_ExchangeServiceCorner_EventScript_KissPoster::
	setitemandprice DECOR_KISS_POSTER, 16
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor1

BattleFrontier_ExchangeServiceCorner_EventScript_KissCushion::
	setitemandprice DECOR_KISS_CUSHION, 32
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor1

BattleFrontier_ExchangeServiceCorner_EventScript_SmoochumDoll::
	setitemandprice DECOR_SMOOCHUM_DOLL, 32
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor1

BattleFrontier_ExchangeServiceCorner_EventScript_TogepiDoll::
	setitemandprice DECOR_TOGEPI_DOLL, 48
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor1

BattleFrontier_ExchangeServiceCorner_EventScript_MeowthDoll::
	setitemandprice DECOR_MEOWTH_DOLL, 48
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor1

BattleFrontier_ExchangeServiceCorner_EventScript_ClefairyDoll::
	setitemandprice DECOR_CLEFAIRY_DOLL, 48
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor1

BattleFrontier_ExchangeServiceCorner_EventScript_DittoDoll::
	setitemandprice DECOR_DITTO_DOLL, 48
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor1

BattleFrontier_ExchangeServiceCorner_EventScript_CyndaquilDoll::
	setitemandprice DECOR_CYNDAQUIL_DOLL, 80
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor1

BattleFrontier_ExchangeServiceCorner_EventScript_ChikoritaDoll::
	setitemandprice DECOR_CHIKORITA_DOLL, 80
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor1

BattleFrontier_ExchangeServiceCorner_EventScript_TotodileDoll::
	setitemandprice DECOR_TOTODILE_DOLL, 80
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor1

BattleFrontier_ExchangeServiceCorner_EventScript_DecorClerk2::
	lock
	faceplayer
	setvar VAR_TEMP_2, EXCHANGE_CORNER_DECOR2_CLERK
	call BattleFrontier_ExchangeServiceCorner_EventScript_ClerkWelcome
	goto BattleFrontier_ExchangeServiceCorner_EventScript_ChooseDecor2
	end

BattleFrontier_ExchangeServiceCorner_EventScript_ChooseDecor2::
	setvar VAR_0x8004, SCROLL_MULTI_BF_EXCHANGE_CORNER_DECOR_VENDOR_2
	special ShowFrontierExchangeCornerItemIconWindow
	special ShowScrollableMultichoice
	special CloseFrontierExchangeCornerItemIconWindow
	switch VAR_RESULT
	case 0, BattleFrontier_ExchangeServiceCorner_EventScript_LaprasDoll
	case 1, BattleFrontier_ExchangeServiceCorner_EventScript_SnorlaxDoll
	case 2, BattleFrontier_ExchangeServiceCorner_EventScript_VenusaurDoll
	case 3, BattleFrontier_ExchangeServiceCorner_EventScript_CharizardDoll
	case 4, BattleFrontier_ExchangeServiceCorner_EventScript_BlastoiseDoll
	case 5, BattleFrontier_ExchangeServiceCorner_EventScript_ClerkGoodbye
	case MULTI_B_PRESSED, BattleFrontier_ExchangeServiceCorner_EventScript_ClerkGoodbye
	end

BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor2::
	bufferdecorationname STR_VAR_1, VAR_0x8009
	msgbox BattleFrontier_ExchangeServiceCorner_Text_ConfirmPurchase, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, BattleFrontier_ExchangeServiceCorner_EventScript_ChooseDecor2
	goto BattleFrontier_ExchangeServiceCorner_EventScript_TryPurchasePrize
	end

BattleFrontier_ExchangeServiceCorner_EventScript_LaprasDoll::
	setitemandprice DECOR_LAPRAS_DOLL, 128
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor2

BattleFrontier_ExchangeServiceCorner_EventScript_SnorlaxDoll::
	setitemandprice DECOR_SNORLAX_DOLL, 128
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor2

BattleFrontier_ExchangeServiceCorner_EventScript_VenusaurDoll::
	setitemandprice DECOR_VENUSAUR_DOLL, 256
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor2

BattleFrontier_ExchangeServiceCorner_EventScript_CharizardDoll::
	setitemandprice DECOR_CHARIZARD_DOLL, 256
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor2

BattleFrontier_ExchangeServiceCorner_EventScript_BlastoiseDoll::
	setitemandprice DECOR_BLASTOISE_DOLL, 256
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseDecor2

BattleFrontier_ExchangeServiceCorner_EventScript_VitaminClerk::
	lock
	faceplayer
	setvar VAR_TEMP_2, EXCHANGE_CORNER_VITAMIN_CLERK
	call BattleFrontier_ExchangeServiceCorner_EventScript_ClerkWelcome
	goto BattleFrontier_ExchangeServiceCorner_EventScript_ChooseVitamin
	end

BattleFrontier_ExchangeServiceCorner_EventScript_ChooseVitamin::
	setvar VAR_0x8004, SCROLL_MULTI_BF_EXCHANGE_CORNER_VITAMIN_VENDOR
	special ShowFrontierExchangeCornerItemIconWindow
	special ShowScrollableMultichoice
	special CloseFrontierExchangeCornerItemIconWindow
	switch VAR_RESULT
	case 0, BattleFrontier_ExchangeServiceCorner_EventScript_Protein
	case 1, BattleFrontier_ExchangeServiceCorner_EventScript_Calcium
	case 2, BattleFrontier_ExchangeServiceCorner_EventScript_Iron
	case 3, BattleFrontier_ExchangeServiceCorner_EventScript_Zinc
	case 4, BattleFrontier_ExchangeServiceCorner_EventScript_Carbos
	case 5, BattleFrontier_ExchangeServiceCorner_EventScript_HPUp
	case 6, BattleFrontier_ExchangeServiceCorner_EventScript_BottleCap
	case 7, BattleFrontier_ExchangeServiceCorner_EventScript_GoldBottleCap
	case 8, BattleFrontier_ExchangeServiceCorner_EventScript_ClerkGoodbye
	case MULTI_B_PRESSED, BattleFrontier_ExchangeServiceCorner_EventScript_ClerkGoodbye
	end

BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseVitamin::
	bufferitemname STR_VAR_1, VAR_0x8009
	msgbox BattleFrontier_ExchangeServiceCorner_Text_ConfirmPurchase, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, BattleFrontier_ExchangeServiceCorner_EventScript_ChooseVitamin
	goto BattleFrontier_ExchangeServiceCorner_EventScript_TryPurchasePrize
	end

BattleFrontier_ExchangeServiceCorner_EventScript_Protein::
	setitemandprice ITEM_PROTEIN, 1
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseVitamin

BattleFrontier_ExchangeServiceCorner_EventScript_Calcium::
	setitemandprice ITEM_CALCIUM, 1
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseVitamin

BattleFrontier_ExchangeServiceCorner_EventScript_Iron::
	setitemandprice ITEM_IRON, 1
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseVitamin

BattleFrontier_ExchangeServiceCorner_EventScript_Zinc::
	setitemandprice ITEM_ZINC, 1
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseVitamin

BattleFrontier_ExchangeServiceCorner_EventScript_Carbos::
	setitemandprice ITEM_CARBOS, 1
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseVitamin

BattleFrontier_ExchangeServiceCorner_EventScript_HPUp::
	setitemandprice ITEM_HP_UP, 1
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseVitamin

@ GRAFT: Bottle Caps for the hyper trainer
BattleFrontier_ExchangeServiceCorner_EventScript_BottleCap::
	setitemandprice ITEM_BOTTLE_CAP, 16
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseVitamin

BattleFrontier_ExchangeServiceCorner_EventScript_GoldBottleCap::
	setitemandprice ITEM_GOLD_BOTTLE_CAP, 48
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseVitamin


BattleFrontier_ExchangeServiceCorner_EventScript_HoldItemClerk::
	lock
	faceplayer
	setvar VAR_TEMP_2, EXCHANGE_CORNER_HOLD_ITEM_CLERK
	call BattleFrontier_ExchangeServiceCorner_EventScript_ClerkWelcome
	goto BattleFrontier_ExchangeServiceCorner_EventScript_ChooseHoldItem
	end

BattleFrontier_ExchangeServiceCorner_EventScript_ChooseHoldItem::
	setvar VAR_0x8004, SCROLL_MULTI_BF_EXCHANGE_CORNER_HOLD_ITEM_VENDOR
	special ShowFrontierExchangeCornerItemIconWindow
	special ShowScrollableMultichoice
	special CloseFrontierExchangeCornerItemIconWindow
	switch VAR_RESULT
	case 0, BattleFrontier_ExchangeServiceCorner_EventScript_Leftovers
	case 1, BattleFrontier_ExchangeServiceCorner_EventScript_WhiteHerb
	case 2, BattleFrontier_ExchangeServiceCorner_EventScript_QuickClaw
	case 3, BattleFrontier_ExchangeServiceCorner_EventScript_MentalHerb
	case 4, BattleFrontier_ExchangeServiceCorner_EventScript_Brightpowder
	case 5, BattleFrontier_ExchangeServiceCorner_EventScript_ChoiceBand
	case 6, BattleFrontier_ExchangeServiceCorner_EventScript_KingsRock
	case 7, BattleFrontier_ExchangeServiceCorner_EventScript_FocusBand
	case 8, BattleFrontier_ExchangeServiceCorner_EventScript_ScopeLens
	case 9, BattleFrontier_ExchangeServiceCorner_EventScript_ClerkGoodbye
	case MULTI_B_PRESSED, BattleFrontier_ExchangeServiceCorner_EventScript_ClerkGoodbye
	end

BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseHoldItem::
	bufferitemname STR_VAR_1, VAR_0x8009
	msgbox BattleFrontier_ExchangeServiceCorner_Text_ConfirmPurchase, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, BattleFrontier_ExchangeServiceCorner_EventScript_ChooseHoldItem
	goto BattleFrontier_ExchangeServiceCorner_EventScript_TryPurchasePrize
	end

BattleFrontier_ExchangeServiceCorner_EventScript_Leftovers::
	setitemandprice ITEM_LEFTOVERS, 48
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseHoldItem

BattleFrontier_ExchangeServiceCorner_EventScript_WhiteHerb::
	setitemandprice ITEM_WHITE_HERB, 48
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseHoldItem

BattleFrontier_ExchangeServiceCorner_EventScript_QuickClaw::
	setitemandprice ITEM_QUICK_CLAW, 48
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseHoldItem

BattleFrontier_ExchangeServiceCorner_EventScript_MentalHerb::
	setitemandprice ITEM_MENTAL_HERB, 48
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseHoldItem

BattleFrontier_ExchangeServiceCorner_EventScript_Brightpowder::
	setitemandprice ITEM_BRIGHT_POWDER, 64
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseHoldItem

BattleFrontier_ExchangeServiceCorner_EventScript_ChoiceBand::
	setitemandprice ITEM_CHOICE_BAND, 64
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseHoldItem

BattleFrontier_ExchangeServiceCorner_EventScript_KingsRock::
	setitemandprice ITEM_KINGS_ROCK, 64
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseHoldItem

BattleFrontier_ExchangeServiceCorner_EventScript_FocusBand::
	setitemandprice ITEM_FOCUS_BAND, 64
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseHoldItem

BattleFrontier_ExchangeServiceCorner_EventScript_ScopeLens::
	setitemandprice ITEM_SCOPE_LENS, 64
	goto BattleFrontier_ExchangeServiceCorner_EventScript_PurchaseHoldItem

BattleFrontier_ExchangeServiceCorner_EventScript_Man::
	msgbox BattleFrontier_ExchangeServiceCorner_Text_GoGetYourOwnDoll, MSGBOX_NPC
	end

BattleFrontier_ExchangeServiceCorner_EventScript_Sailor::
	msgbox BattleFrontier_ExchangeServiceCorner_Text_ItemsWillGetMonTougher, MSGBOX_NPC
	end

BattleFrontier_ExchangeServiceCorner_EventScript_PokefanF::
	lock
	msgbox BattleFrontier_ExchangeServiceCorner_Text_GetYouAnythingYouWant, MSGBOX_DEFAULT
	release
	end

BattleFrontier_ExchangeServiceCorner_EventScript_RichBoy::
	msgbox BattleFrontier_ExchangeServiceCorner_Text_WishIHadAllDolls, MSGBOX_NPC
	end

BattleFrontier_ExchangeServiceCorner_EventScript_Girl::
	msgbox BattleFrontier_ExchangeServiceCorner_Text_MoreBattlePointsForRecord, MSGBOX_NPC
	end

BattleFrontier_ExchangeServiceCorner_Text_WelcomePleaseChoosePrize:
	.string "Hello, this is the EXCHANGE SERVICE\n"
	.string "CORNER.\p"
	.string "We exchange the Battle Points you\n"
	.string "have collected for lovely prizes.\p"
	.string "Please choose a prize from this list.$"

@ Unused
BattleFrontier_ExchangeServiceCorner_Text_PleaseChoosePrize:
	.string "Please choose a prize from this list.$"

BattleFrontier_ExchangeServiceCorner_Text_ConfirmPurchase:
	.string "You've chosen the {STR_VAR_1}.\n"
	.string "Is that correct?$"

BattleFrontier_ExchangeServiceCorner_Text_WellSendItToPC:
	.string "Thank you!\n"
	.string "We'll send it to your PC at home.$"

BattleFrontier_ExchangeServiceCorner_Text_HereIsYourPrize:
	.string "Here is your prize!$"

BattleFrontier_ExchangeServiceCorner_Text_DontHaveEnoughPoints:
	.string "I'm so sorry…\n"
	.string "You don't have enough Battle Points…$"

BattleFrontier_ExchangeServiceCorner_Text_PCIsFull:
	.string "I'm so sorry…\n"
	.string "Your PC appears to be full…$"

BattleFrontier_ExchangeServiceCorner_Text_DontHaveSpaceToHoldIt:
	.string "I'm so sorry…\n"
	.string "You don't have space to hold it…$"

BattleFrontier_ExchangeServiceCorner_Text_ThankYouVisitWithPoints:
	.string "Thank you very much.\p"
	.string "Please visit us when you have\n"
	.string "saved up Battle Points.$"

BattleFrontier_ExchangeServiceCorner_Text_WishIHadAllDolls:
	.string "Oh, they're so nice!\n"
	.string "I wish I had them!\l"
	.string "Cute cushions!\l"
	.string "Big plush DOLLS!\l"
	.string "Little plush DOLLS!\l"
	.string "I wish I had them all!$"

BattleFrontier_ExchangeServiceCorner_Text_GetYouAnythingYouWant:
	.string "Leave it to your mommy!\p"
	.string "If it's anything you want, I'll go\n"
	.string "through anything to get it, honey!$"

BattleFrontier_ExchangeServiceCorner_Text_ItemsWillGetMonTougher:
	.string "If I can get hold of the items here,\n"
	.string "my POKéMON will get tougher.\p"
	.string "You bet they will!\n"
	.string "No question about it!$"

BattleFrontier_ExchangeServiceCorner_Text_GoGetYourOwnDoll:
	.string "Hah?\n"
	.string "What are you gawking at?\p"
	.string "I don't like you staring at my plush\n"
	.string "doll like that.\p"
	.string "If you want it, go get one yourself!\n"
	.string "Isn't that right, SMOOCHUM?$"

BattleFrontier_ExchangeServiceCorner_Text_MoreBattlePointsForRecord:
	.string "Did you know?\p"
	.string "If you stretch your record at any of\n"
	.string "the BATTLE facilities, they start\l"
	.string "giving you more and more Battle Points.$"

BattleFrontier_ExchangeServiceCorner_Text_KissPosterDesc::
	.string "A large poster with a SMOOCHUM print.$"

BattleFrontier_ExchangeServiceCorner_Text_KissCushionDesc::
	.string "A SMOOCHUM cushion.\n"
	.string "Place it on a mat or a desk.$"

BattleFrontier_ExchangeServiceCorner_Text_SmoochumDollDesc::
	.string "A SMOOCHUM DOLL.\n"
	.string "Place it on a mat or a desk.$"

BattleFrontier_ExchangeServiceCorner_Text_TogepiDollDesc::
	.string "A TOGEPI DOLL.\n"
	.string "Place it on a mat or a desk.$"

BattleFrontier_ExchangeServiceCorner_Text_MeowthDollDesc::
	.string "A MEOWTH DOLL.\n"
	.string "Place it on a mat or a desk.$"

BattleFrontier_ExchangeServiceCorner_Text_ClefairyDollDesc::
	.string "A CLEFAIRY DOLL.\n"
	.string "Place it on a mat or a desk.$"

BattleFrontier_ExchangeServiceCorner_Text_DittoDollDesc::
	.string "A DITTO DOLL.\n"
	.string "Place it on a mat or a desk.$"

BattleFrontier_ExchangeServiceCorner_Text_CyndaquilDollDesc::
	.string "A CYNDAQUIL DOLL.\n"
	.string "Place it on a mat or a desk.$"

BattleFrontier_ExchangeServiceCorner_Text_ChikoritaDollDesc::
	.string "A CHIKORITA DOLL.\n"
	.string "Place it on a mat or a desk.$"

BattleFrontier_ExchangeServiceCorner_Text_TotodileDollDesc::
	.string "A TOTODILE DOLL.\n"
	.string "Place it on a mat or a desk.$"

BattleFrontier_ExchangeServiceCorner_Text_LargeDollDesc::
	.string "A large DOLL.\n"
	.string "Place it on a mat or a desk.$"

BattleFrontier_ExchangeServiceCorner_Text_ProteinDesc::
	.string "Raises the stat ATTACK of one\n"
	.string "POKéMON.$"

BattleFrontier_ExchangeServiceCorner_Text_CalciumDesc::
	.string "Raises the stat SP. ATK of one\n"
	.string "POKéMON.$"

BattleFrontier_ExchangeServiceCorner_Text_IronDesc::
	.string "Raises the stat DEFENSE of one\n"
	.string "POKéMON.$"

BattleFrontier_ExchangeServiceCorner_Text_ZincDesc::
	.string "Raises the stat SP. DEF of one\n"
	.string "POKéMON.$"

BattleFrontier_ExchangeServiceCorner_Text_CarbosDesc::
	.string "Raises the stat SPEED of one\n"
	.string "POKéMON.$"

BattleFrontier_ExchangeServiceCorner_Text_HPUpDesc::
	.string "Raises the HP of one POKéMON.$"

BattleFrontier_ExchangeServiceCorner_Text_BottleCapDesc::
	.string "The hyper trainer here takes one\n"
	.string "to max out one of a POKéMON's IVs.$"

BattleFrontier_ExchangeServiceCorner_Text_GoldBottleCapDesc::
	.string "The hyper trainer here takes one\n"
	.string "to max out all of a POKéMON's IVs.$"

BattleFrontier_ExchangeServiceCorner_Text_LeftoversDesc::
	.string "A hold item that gradually restores\n"
	.string "HP in battle.$"

BattleFrontier_ExchangeServiceCorner_Text_WhiteHerbDesc::
	.string "A hold item that restores any\n"
	.string "lowered stat.$"

BattleFrontier_ExchangeServiceCorner_Text_QuickClawDesc::
	.string "A hold item that occasionally allows\n"
	.string "the first strike.$"

BattleFrontier_ExchangeServiceCorner_Text_MentalHerbDesc::
	.string "A hold item that snaps POKéMON out\n"
	.string "of infatuation.$"

BattleFrontier_ExchangeServiceCorner_Text_BrightpowderDesc::
	.string "A hold item that casts a glare to\n"
	.string "reduce accuracy.$"

BattleFrontier_ExchangeServiceCorner_Text_ChoiceBandDesc::
	.string "Raises a move's power, but permits\n"
	.string "only that move.$"

BattleFrontier_ExchangeServiceCorner_Text_KingsRockDesc::
	.string "A hold item that may cause flinching\n"
	.string "when the foe is hit.$"

BattleFrontier_ExchangeServiceCorner_Text_FocusBandDesc::
	.string "A hold item that occasionally\n"
	.string "prevents fainting.$"

BattleFrontier_ExchangeServiceCorner_Text_ScopeLensDesc::
	.string "A hold item that raises the\n"
	.string "critical-hit rate.$"

@ GRAFT: hyper training. A Bottle Cap maxes one IV, a Gold Bottle Cap all six.
@ VAR_0x8004 = party slot, VAR_0x8005/6 = has Bottle Cap / Gold Bottle Cap, VAR_0x8007 = trainable stats
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainer::
	lock
	faceplayer
	msgbox BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerIntro, MSGBOX_DEFAULT
	checkitem ITEM_BOTTLE_CAP
	copyvar VAR_0x8005, VAR_RESULT
	checkitem ITEM_GOLD_BOTTLE_CAP
	copyvar VAR_0x8006, VAR_RESULT
	goto_if_eq VAR_0x8005, TRUE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainChooseMon
	goto_if_eq VAR_0x8006, TRUE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainChooseMon
	msgbox BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerNoCaps, MSGBOX_DEFAULT
	release
	end

BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainChooseMon::
	msgbox BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerWhichMon, MSGBOX_DEFAULT
	special ChoosePartyMon
	waitstate
	goto_if_ge VAR_0x8004, PARTY_SIZE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainCancel
	specialvar VAR_RESULT, ScriptGetPartyMonSpecies
	goto_if_eq VAR_RESULT, SPECIES_EGG, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainEgg
	setvar VAR_0x8007, FALSE
	canhypertrain STAT_HP, VAR_0x8004
	goto_if_eq VAR_RESULT, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainCheckHPDone
	setvar VAR_0x8007, TRUE
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainCheckHPDone:
	canhypertrain STAT_ATK, VAR_0x8004
	goto_if_eq VAR_RESULT, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainCheckAtkDone
	setvar VAR_0x8007, TRUE
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainCheckAtkDone:
	canhypertrain STAT_DEF, VAR_0x8004
	goto_if_eq VAR_RESULT, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainCheckDefDone
	setvar VAR_0x8007, TRUE
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainCheckDefDone:
	canhypertrain STAT_SPATK, VAR_0x8004
	goto_if_eq VAR_RESULT, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainCheckSpAtkDone
	setvar VAR_0x8007, TRUE
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainCheckSpAtkDone:
	canhypertrain STAT_SPDEF, VAR_0x8004
	goto_if_eq VAR_RESULT, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainCheckSpDefDone
	setvar VAR_0x8007, TRUE
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainCheckSpDefDone:
	canhypertrain STAT_SPEED, VAR_0x8004
	goto_if_eq VAR_RESULT, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainCheckSpeedDone
	setvar VAR_0x8007, TRUE
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainCheckSpeedDone:
	goto_if_eq VAR_0x8007, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainMaxed
	goto_if_eq VAR_0x8006, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainChooseStat
	msgbox BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerUseGold, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, YES, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainAll
	goto_if_eq VAR_0x8005, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainCancel
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainChooseStat::
	msgbox BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerWhichStat, MSGBOX_DEFAULT
	canhypertrain STAT_HP, VAR_0x8004
	goto_if_eq VAR_RESULT, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainPushHPDone
	dynmultipush BattleFrontier_ExchangeServiceCorner_Text_StatHP, STAT_HP
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainPushHPDone:
	canhypertrain STAT_ATK, VAR_0x8004
	goto_if_eq VAR_RESULT, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainPushAtkDone
	dynmultipush BattleFrontier_ExchangeServiceCorner_Text_StatAtk, STAT_ATK
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainPushAtkDone:
	canhypertrain STAT_DEF, VAR_0x8004
	goto_if_eq VAR_RESULT, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainPushDefDone
	dynmultipush BattleFrontier_ExchangeServiceCorner_Text_StatDef, STAT_DEF
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainPushDefDone:
	canhypertrain STAT_SPATK, VAR_0x8004
	goto_if_eq VAR_RESULT, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainPushSpAtkDone
	dynmultipush BattleFrontier_ExchangeServiceCorner_Text_StatSpAtk, STAT_SPATK
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainPushSpAtkDone:
	canhypertrain STAT_SPDEF, VAR_0x8004
	goto_if_eq VAR_RESULT, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainPushSpDefDone
	dynmultipush BattleFrontier_ExchangeServiceCorner_Text_StatSpDef, STAT_SPDEF
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainPushSpDefDone:
	canhypertrain STAT_SPEED, VAR_0x8004
	goto_if_eq VAR_RESULT, FALSE, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainPushSpeedDone
	dynmultipush BattleFrontier_ExchangeServiceCorner_Text_StatSpeed, STAT_SPEED
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainPushSpeedDone:
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case STAT_HP, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainHP
	case STAT_ATK, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainAtk
	case STAT_DEF, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainDef
	case STAT_SPATK, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainSpAtk
	case STAT_SPDEF, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainSpDef
	case STAT_SPEED, BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainSpeed
	goto BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainCancel
	end

BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainHP::
	hypertrain STAT_HP, VAR_0x8004
	goto BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainOneDone
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainAtk::
	hypertrain STAT_ATK, VAR_0x8004
	goto BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainOneDone
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainDef::
	hypertrain STAT_DEF, VAR_0x8004
	goto BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainOneDone
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainSpAtk::
	hypertrain STAT_SPATK, VAR_0x8004
	goto BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainOneDone
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainSpDef::
	hypertrain STAT_SPDEF, VAR_0x8004
	goto BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainOneDone
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainSpeed::
	hypertrain STAT_SPEED, VAR_0x8004
	goto BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainOneDone

BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainOneDone::
	removeitem ITEM_BOTTLE_CAP
	goto BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainDone

BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainAll::
	hypertrain STAT_HP, VAR_0x8004
	hypertrain STAT_ATK, VAR_0x8004
	hypertrain STAT_DEF, VAR_0x8004
	hypertrain STAT_SPATK, VAR_0x8004
	hypertrain STAT_SPDEF, VAR_0x8004
	hypertrain STAT_SPEED, VAR_0x8004
	removeitem ITEM_GOLD_BOTTLE_CAP
BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainDone::
	bufferpartymonnick STR_VAR_1, VAR_0x8004
	playfanfare MUS_LEVEL_UP
	msgbox BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerDone, MSGBOX_DEFAULT
	waitfanfare
	release
	end

BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainMaxed::
	msgbox BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerMaxed, MSGBOX_DEFAULT
	release
	end

BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainEgg::
	msgbox BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerEgg, MSGBOX_DEFAULT
	release
	end

BattleFrontier_ExchangeServiceCorner_EventScript_HyperTrainCancel::
	msgbox BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerCancel, MSGBOX_DEFAULT
	release
	end

BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerIntro::
	.string "I'm a hyper trainer! A BOTTLE CAP\n"
	.string "maxes one IV, a GOLD one maxes all!$"

BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerNoCaps::
	.string "You don't have any BOTTLE CAPS.\n"
	.string "The clerks here trade them for BP.$"

BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerWhichMon::
	.string "Which POKéMON should I train?$"

BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerUseGold::
	.string "Use a GOLD BOTTLE CAP to max out\n"
	.string "all of its IVs?$"

BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerWhichStat::
	.string "Which stat should I train?$"

BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerDone::
	.string "Hyper training complete!\n"
	.string "{STR_VAR_1} got stronger!$"

BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerMaxed::
	.string "That POKéMON's IVs are already\n"
	.string "as high as they go.$"

BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerEgg::
	.string "I can't train an EGG!$"

BattleFrontier_ExchangeServiceCorner_Text_HyperTrainerCancel::
	.string "Come back anytime.$"

BattleFrontier_ExchangeServiceCorner_Text_StatHP::
	.string "HP$"
BattleFrontier_ExchangeServiceCorner_Text_StatAtk::
	.string "ATTACK$"
BattleFrontier_ExchangeServiceCorner_Text_StatDef::
	.string "DEFENSE$"
BattleFrontier_ExchangeServiceCorner_Text_StatSpAtk::
	.string "SP. ATK$"
BattleFrontier_ExchangeServiceCorner_Text_StatSpDef::
	.string "SP. DEF$"
BattleFrontier_ExchangeServiceCorner_Text_StatSpeed::
	.string "SPEED$"
