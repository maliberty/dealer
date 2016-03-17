#ifndef DEALER_H
#define DEALER_H

#include "pointcount.h"

typedef unsigned char Card;

typedef Card Deal[52];

#ifdef STD_RAND
#define RANDOM rand
#define SRANDOM(seed) srand(seed);
#else
#define RANDOM gnurand
int gnurand();
#define SRANDOM(seed) gnusrand(seed)
int gnusrand(unsigned int);
#endif /* STD_RAND */

struct Handstat {
    int hs_length[NSUITS];         /* distribution */
    int hs_hcp[NSUITS];            /* 4321 HCP per suit */
    int hs_totalhcp;               /* HCP in the hand */
    int hs_dist[NSUITS];           /* Distribution points per suit */
    int hs_totaldist;              /* Distribution points in the hand */
    int hs_bits;                   /* Bitmap to check distribution */
    int hs_loser[NSUITS];          /* Losers in a suit */
    int hs_totalloser;             /* Losers in the hand */
    int hs_control[NSUITS];        /* Controls in a suit */
    int hs_totalcontrol;           /* Controls in the hand */
    int hs_counts[idxEnd][NSUITS]; /* other auxiliary counts */
    int hs_totalcounts[idxEnd];    /* totals of the above */
};

extern Handstat hs[4];

extern Deal curdeal;

extern int maxgenerate;
extern int maxdealer;
extern int maxvuln;
extern int maxproduce;
extern bool will_print;
extern Tree *decisiontree;
extern Action *actionlist;
extern bool use_compass[NSUITS];
extern long seed;
extern const char *input_file;
extern const char *player_name[4];

extern void pointcount(int index, int value);
extern void setshapebit(int cl, int di, int ht, int sp, int msk, int excepted);
extern void clearpointcount();
extern void clearpointcount_alt(int cin);
extern void predeal(int player, Card onecard);
extern void *mycalloc(unsigned nel, size_t siz);
extern void yyerror(const char *s);
extern Card make_card(char rankchar, char suitchar);
extern int make_contract(char suitchar, char trickchar);

#ifdef FRANCOIS
int hascard(Deal d, int player, Card onecard, int vectordeal);
inline int HAS_CARD(Deal d, int p, Card c) {
    return hascard(d, p, c, 0);
}
#else
int hascard(Deal d, int player, Card onecard);
inline int HAS_CARD(Deal d, int p, Card c) {
    return hascard(d, p, c);
}
#endif

inline Card make_card(int suit, int rank) {
    return (Card)((suit << 6) | rank);
}

inline int make_contract(int suit, int tricks) {
    return tricks * 5 + suit;
}

inline int card_suit(Card c) {
    return c >> 6;
}

inline int card_rank(Card c) {
    return c & 0x3F;
}

const Card NO_CARD = 0xFF;

#endif /* DEALER_H */
