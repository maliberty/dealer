#ifndef TREE_H
#define TREE_H
/*
 * decision tree and other expression stuff
 */

#define SUIT_CLUB 0
#define SUIT_DIAMOND 1
#define SUIT_HEART 2
#define SUIT_SPADE 3
#define SUIT_NT 4
#define NSUITS 4

#define COMPASS_NORTH 0
#define COMPASS_EAST 1
#define COMPASS_SOUTH 2
#define COMPASS_WEST 3

#define VULNERABLE_NONE 0
#define VULNERABLE_NS 1
#define VULNERABLE_EW 2
#define VULNERABLE_ALL 3

#define NON_VUL 0
#define VUL 1

enum class TreeType {
    Number,
    And2,
    Or2,
    CmpEQ,
    CmpNE,
    CmpLT,
    CmpLE,
    CmpGT,
    CmpGE,
    Length,
    ArPlus,
    ArMinus,
    Artimes,
    ArDivide,
    ArMod,
    HcpTotal,
    Hcp,
    Shape,
    Not,
    HasCard,
    If,
    ThenElse,
    LoserTotal,
    Loser,
    Tricks,
    Rnd,
    Control,
    ControlTotal,
    Score,
    Imps,
    Cccc,
    Quality,
    Pt0Total,
    Pt0,
    Pt1Total,
    Pt1,
    Pt2Total,
    Pt2,
    Pt3Total,
    Pt3,
    Pt4Total,
    Pt4,
    Pt5Total,
    Pt5,
    Pt6Total,
    Pt6,
    Pt7Total,
    Pt7,
    Pt8Total,
    Pt8,
    Pt9Total,
    Pt9,
    DistPts,
    DistPtsTotal,
};

struct Tree {
    TreeType tr_type;
    const Tree *tr_leaf1, *tr_leaf2;
    int tr_int1;
    int tr_int2;
};

struct Expr {
    const Tree *ex_tr;
    char *ex_ch;
    Expr *next;
};

extern const Tree *NIL;

/*
 * Actions to be taken
 */
struct Acuft {
    long acuf_lowbnd;
    long acuf_highbnd;
    long acuf_uflow;
    long acuf_oflow;
    long acuf_entries;
    long *acuf_freqs;
};

struct Acuft2d {
    long acuf_lowbnd_expr1;
    long acuf_highbnd_expr1;
    long acuf_lowbnd_expr2;
    long acuf_highbnd_expr2;
    long *acuf_freqs;
};

enum class ActionType {
    PrintAll,
    Print,
    Average,
    Frequency,
    PrintCompact,
    EvalContract,
    PrintPBN,
    PrintEW,
    Frequency2d,
    PrintOneLine,
    PrintES
};

struct Action {
    Action *ac_next;
    ActionType ac_type;
    const Tree *ac_expr1;
    const Tree *ac_expr2;
    int ac_int1;
    char *ac_str1;
    union {
        Acuft acu_f;
        Acuft2d acu_f2d;
    } ac_u;
};

/* Constants for CCCC and Quality */

#define RK_ACE 12
#define RK_KING 11
#define RK_QUEEN 10
#define RK_JACK 9
#define RK_TEN 8
#define RK_NINE 7
#define RK_EIGHT 6

#endif /* TREE_H */
