"""
Patch party_menu.c, data/party_menu.h, and constants/party_menu.h for graft party selection.
Also patches graft_menu.c and graft_data.c/h for the new flow.
"""
import sys

def patch_file(path, patches, description=""):
    """Apply a list of (old, new) replacements to a file."""
    with open(path, "r") as f:
        content = f.read()
    original = content
    for old, new in patches:
        if old in content:
            content = content.replace(old, new, 1)
        else:
            print(f"  WARNING: Could not find patch target in {path}")
            print(f"  Looking for: {old[:80]}...")
    if content != original:
        with open(path, "w") as f:
            f.write(content)
        print(f"  [OK] Patched {path} {description}")
    else:
        print(f"  [SKIP] No changes to {path}")
    return content != original

# ========== 1. constants/party_menu.h ==========
CONST_PATH = "/home/gaming/pokeemerald-expansion/include/constants/party_menu.h"
patch_file(CONST_PATH, [
    ("#define PARTY_ACTION_CHOOSE_FAINTED_MON 15",
     "#define PARTY_ACTION_CHOOSE_FAINTED_MON 15\n#define PARTY_ACTION_GRAFT 16")
], "PARTY_ACTION_GRAFT")

# ========== 2. party_menu.c ==========
PM_PATH = "/home/gaming/pokeemerald-expansion/src/party_menu.c"
with open(PM_PATH, "r") as f:
    lines = f.readlines()

changes = 0

# 2a. Add MENU_APPLY_GRAFT to enum
for i, line in enumerate(lines):
    if "    MENU_FIELD_MOVES" in line and "MENU_APPLY_GRAFT" not in "".join(lines[max(0,i-3):i]):
        lines.insert(i, "    MENU_APPLY_GRAFT,\n")
        print(f"  [2a] Added MENU_APPLY_GRAFT to enum")
        changes += 1
        break

# 2b. Add ACTIONS_GRAFT to enum
for i, line in enumerate(lines):
    if line.strip() == "ACTIONS_SUMMARY_ONLY," and "ACTIONS_GRAFT" not in "".join(lines[max(0,i):i+2]):
        lines.insert(i+1, "    ACTIONS_GRAFT,\n")
        print(f"  [2b] Added ACTIONS_GRAFT to enum")
        changes += 1
        break

# 2c. Add CursorCb_ApplyGraft forward declaration
content_check = "".join(lines)
if "static void CursorCb_ApplyGraft" not in content_check:
    for i, line in enumerate(lines):
        if "static void CursorCb_Cancel1(u8 taskId)" in line:
            # Insert forward declaration before the implementation
            lines.insert(i, "static void CursorCb_ApplyGraft(u8 taskId);\n\n")
            print(f"  [2c] Added CursorCb_ApplyGraft forward declaration")
            changes += 1
            break

# 2d. Add CursorCb_ApplyGraft implementation before CursorCb_Cancel1
content_check = "".join(lines)
if "void CursorCb_ApplyGraft(u8 taskId)\n" not in content_check:
    for i, line in enumerate(lines):
        if "static void CursorCb_Cancel1(u8 taskId)" in line:
            graft_impl = """static void CursorCb_ApplyGraft(u8 taskId)
{
    // Graft application is handled in the battle script command after
    // returning from the party menu. We just close the menu here.
    // gPartyMenu.slotId has the selected party slot.
    PlaySE(SE_SELECT);
    Task_ClosePartyMenu(taskId);
}

"""
            lines.insert(i, graft_impl)
            print(f"  [2d] Added CursorCb_ApplyGraft implementation")
            changes += 1
            break

# 2e. Add PARTY_ACTION_GRAFT case in HandleChooseMonSelection
content_check = "".join(lines)
if "case PARTY_ACTION_GRAFT:" not in content_check:
    for i, line in enumerate(lines):
        if "        default:" in line and i > 1450 and i < 1600:
            # This is in HandleChooseMonSelection
            graft_case = """        case PARTY_ACTION_GRAFT:
            if (IsSelectedMonNotEgg((u8 *)slotPtr))
            {
                PlaySE(SE_SELECT);
                PartyMenuRemoveWindow(&sPartyMenuInternal->windowId[1]);
                Task_TryCreateSelectionWindow(taskId);
            }
            break;
"""
            lines.insert(i, graft_case)
            print(f"  [2e] Added PARTY_ACTION_GRAFT case in HandleChooseMonSelection")
            changes += 1
            break

# 2f. Add ACTIONS_GRAFT to GetPartyMenuActionsTypeInBattle
content_check = "".join(lines)
if "ACTIONS_GRAFT" not in content_check:
    # Find the function - look for the pattern near line 7413
    for i in range(len(lines)-1, -1, -1):
        if "return ACTIONS_SUMMARY_ONLY;" in lines[i]:
            # Check we're in GetPartyMenuActionsTypeInBattle by looking backwards
            in_func = False
            for j in range(max(0, i-20), i):
                if "GetPartyMenuActionsTypeInBattle" in lines[j]:
                    in_func = True
                    break
            if in_func:
                lines.insert(i, "    if (gPartyMenu.action == PARTY_ACTION_GRAFT)\n        return ACTIONS_GRAFT;\n")
                print(f"  [2f] Added ACTIONS_GRAFT to GetPartyMenuActionsTypeInBattle")
                changes += 1
                break

if changes:
    with open(PM_PATH, "w") as f:
        f.writelines(lines)
    print(f"  [OK] Wrote {changes} patches to party_menu.c")

# ========== 3. data/party_menu.h ==========
DATA_PATH = "/home/gaming/pokeemerald-expansion/src/data/party_menu.h"
patch_file(DATA_PATH, [
    # 3a. Add MENU_APPLY_GRAFT to sCursorOptions
    ('[MENU_CHANGE_ABILITY] = {COMPOUND_STRING("Change Ability"), CursorCb_ChangeAbility},',
     '[MENU_CHANGE_ABILITY] = {COMPOUND_STRING("Change Ability"), CursorCb_ChangeAbility},\n    [MENU_APPLY_GRAFT] = {COMPOUND_STRING("APPLY"), CursorCb_ApplyGraft},'),
    # 3b. Add action array
    ("static const u8 sPartyMenuAction_ZygardeCube[] = {MENU_CHANGE_FORM, MENU_CHANGE_ABILITY, MENU_CANCEL1};",
     "static const u8 sPartyMenuAction_ZygardeCube[] = {MENU_CHANGE_FORM, MENU_CHANGE_ABILITY, MENU_CANCEL1};\nstatic const u8 sPartyMenuAction_Graft[] = {MENU_APPLY_GRAFT, MENU_SUMMARY, MENU_CANCEL1};"),
    # 3c. Add to sPartyMenuActions
    ("[ACTIONS_ZYGARDE_CUBE] = sPartyMenuAction_ZygardeCube,\n};",
     "[ACTIONS_ZYGARDE_CUBE] = sPartyMenuAction_ZygardeCube,\n    [ACTIONS_GRAFT] = sPartyMenuAction_Graft,\n};"),
    # 3d. Add to sPartyMenuActionCounts
    ("[ACTIONS_ZYGARDE_CUBE] = ARRAY_COUNT(sPartyMenuAction_ZygardeCube),\n};",
     "[ACTIONS_ZYGARDE_CUBE] = ARRAY_COUNT(sPartyMenuAction_ZygardeCube),\n    [ACTIONS_GRAFT] = ARRAY_COUNT(sPartyMenuAction_Graft),\n};"),
], "graft party menu entries")

# ========== 4. graft_data.c - add SetGraftedStat and SetGraftedNature ==========
GD_PATH = "/home/gaming/pokeemerald-expansion/src/graft_data.c"
with open(GD_PATH, "r") as f:
    content = f.read()

additions = ""
if "SetGraftedStat" not in content:
    additions += """
void SetGraftedStat(u8 partySlot, u8 statIndex, u16 value)
{
    if (partySlot >= PARTY_SIZE || statIndex >= 6)
        return;
    gSaveBlock1Ptr->graftData.statOverrides[partySlot][statIndex] = value;
    gSaveBlock1Ptr->graftData.graftFlags[partySlot] |= (1 << statIndex);
}
"""
    print("  [4a] Adding SetGraftedStat")

if "SetGraftedNature" not in content:
    additions += """
void SetGraftedNature(u8 partySlot, u8 natureId)
{
    if (partySlot >= PARTY_SIZE)
        return;
    // Modify personality to produce desired nature
    // nature = personality % NUM_NATURES
    u32 personality = GetMonData(&gPlayerParty[partySlot], MON_DATA_PERSONALITY);
    u8 currentNature = personality % NUM_NATURES;
    if (currentNature != natureId)
    {
        personality = (personality / NUM_NATURES) * NUM_NATURES + natureId;
        SetMonData(&gPlayerParty[partySlot], MON_DATA_PERSONALITY, &personality);
    }
}
"""
    print("  [4b] Adding SetGraftedNature")

if additions:
    with open(GD_PATH, "w") as f:
        f.write(content + additions)
    print(f"  [OK] Wrote graft_data.c additions")

# ========== 5. graft_data.h - add declarations ==========
GDH_PATH = "/home/gaming/pokeemerald-expansion/include/graft_data.h"
with open(GDH_PATH, "r") as f:
    content = f.read()

decl_additions = ""
if "SetGraftedStat" not in content:
    decl_additions += "void SetGraftedStat(u8 partySlot, u8 statIndex, u16 value);\n"
if "SetGraftedNature" not in content:
    decl_additions += "void SetGraftedNature(u8 partySlot, u8 natureId);\n"

if decl_additions:
    with open(GDH_PATH, "w") as f:
        f.write(content + decl_additions)
    print(f"  [OK] Wrote graft_data.h declarations")

print("\n[ALL DONE] Party menu patches applied. Now need to update Cmd_graftmenu for party selection flow.")
