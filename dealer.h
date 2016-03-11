#ifndef DEALER_H
#define DEALER_H

typedef unsigned char Card;

typedef Card Deal[52];

static const char *player_name[] = {"North", "East", "South", "West"};

extern int verbose;

/* Changes for cccc and quality */
struct Context {
    Deal *pd;            /* pointer to current deal */
    struct Handstat *ps; /* Pointer to stats of current deal */
};
extern struct Context c;

#ifdef STD_RAND
#define RANDOM rand
#define SRANDOM(seed) srand(seed);
#else
#define RANDOM gnurand
int gnurand();
#define SRANDOM(seed) gnusrand(seed)
int gnusrand(unsigned int);
#endif /* STD_RAND */

#include "pointcount.h"

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

extern struct Handstat hs[4];

extern Deal curdeal;

extern int maxgenerate;
extern int maxdealer;
extern int maxvuln;
extern int will_print;

#define printcompact(d) (fprintcompact(stdout, d, 0))
#define printoneline(d) (fprintcompact(stdout, d, 1))

#ifdef FRANCOIS
int hascard(Deal, int, Card, int);
#define HAS_CARD(d, p, c) hascard(d, p, c, 0)
#else
int hascard(Deal, int, Card);
#define HAS_CARD(d, p, c) hascard(d, p, c)
#endif

Card make_card(char rankchar, char suitchar);
int make_contract(char suitchar, char trickchar);

#endif /* DEALER_H */
