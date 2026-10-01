#ifndef GUARD_GRAFT_BAG_H
#define GUARD_GRAFT_BAG_H

// Graft Bag: grafting balls and the Splicer break Pokémon down into pieces (one trait each),
// which are stored here and grafted onto a party mon later from the Graft Bag screen.
// Storage is a count per possible piece (the position is the piece), in SaveBlock1's graftBag.

enum GraftPieceKind
{
    GRAFT_PIECE_MOVE,
    GRAFT_PIECE_ABILITY,
    GRAFT_PIECE_TYPE,
    GRAFT_PIECE_NATURE,
    GRAFT_PIECE_STAT,
    GRAFT_PIECE_KIND_COUNT,
};

#define GRAFT_PIECE_MAX_COUNT 15 // 4-bit counts
#define GRAFT_PIECES_PER_MON  13 // 4 moves, ability, 2 types, nature, 6 stats

struct GraftPiece
{
    u8 kind;
    u8 stat;   // GRAFT_PIECE_STAT: which stat (0 HP, 1 Atk, 2 Def, 3 Speed, 4 SpAtk, 5 SpDef)
    u16 value; // move, ability, type, nature, or the stat's base value
};

u32 GetGraftPieceCount(const struct GraftPiece *piece);
bool32 AddGraftPiece(const struct GraftPiece *piece);    // FALSE when that piece is already at the max
bool32 RemoveGraftPiece(const struct GraftPiece *piece); // FALSE when there's none
void ClearGraftBag(void);
bool32 IsGraftBagEmpty(void);

// The pieces of one kind the player has, in display order. Returns how many were written (at most maxCount).
u32 GetGraftBagPieces(u32 kind, struct GraftPiece *out, u32 maxCount);
u32 GetGraftBagKindTotal(u32 kind); // number of different pieces of a kind

// Every piece a mon is made of, from its current traits (grafts included). Returns the count.
u32 GetMonGraftPieces(struct Pokemon *mon, bool32 isWild, struct GraftPiece *out);

// Breaking a mon down: Patch Ball = 1 random piece, Graft Ball = 3 different, Splice Ball/Splicer = all.
// Adds them to the bag; *gotCount/*lostCount report what fit. The pieces taken are written to out.
#define GRAFT_HARVEST_ALL 0xFF
u32 HarvestGraftPieces(struct Pokemon *mon, bool32 isWild, u32 numPieces, struct GraftPiece *out, u32 *lostCount);

void GiveGraftBagIfMissing(void);

// Screen (graft_bag_menu.c)
extern struct GraftPiece gGraftPieceInUse; // the piece being grafted from the party menu
void OpenGraftBag(void (*exitCallback)(void));

// "Tackle", "Sturdy", "Rock type", "Adamant nature", "ATK 65"
u8 *GetGraftPieceName(u8 *dest, const struct GraftPiece *piece);

#endif // GUARD_GRAFT_BAG_H
