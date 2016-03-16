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
extern int will_print;
extern Tree *decisiontree;
extern Action *actionlist;
extern int use_compass[NSUITS];
extern int use_vulnerable[NSUITS];
extern long seed;
extern char *input_file;
extern const char *player_name[4];
extern int verbose;

extern void pointcount(int, int);
extern void setshapebit(int, int, int, int, int, int);
extern void clearpointcount(void);
extern void clearpointcount_alt(int);
extern void predeal(int, Card);
extern void *mycalloc(unsigned, size_t);
extern void yyerror(const char *);
extern Card make_card(char rankchar, char suitchar);
extern int make_contract(char suitchar, char trickchar);

#ifdef FRANCOIS
int hascard(Deal, int, Card, int);
inline int HAS_CARD(Deal d, int p, Card c) {
    return hascard(d, p, c, 0);
}
#else
int hascard(Deal, int, Card);
inline int HAS_CARD(Deal d, int p, Card c) {
    return hascard(d, p, c);
}
#endif

inline Card MAKECARD(int suit, int rank) {
    return (Card)((suit << 6) | rank);
}

inline int MAKECONTRACT(int suit, int tricks) {
    return tricks * 5 + suit;
}

inline int C_SUIT(Card c) {
    return c >> 6;
}

inline int C_RANK(Card c) {
    return c & 0x3F;
}

const Card NO_CARD = 0xFF;

#endif /* DEALER_H */
