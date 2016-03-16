#ifndef DEALER_H
#define DEALER_H

typedef unsigned char Card;

typedef Card Deal[52];

extern const char *player_name[4];

extern int verbose;

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
extern int maxproduce;
extern int will_print;
extern struct Tree *decisiontree;
extern struct Action *actionlist;
extern int use_compass[NSUITS];
extern int use_vulnerable[NSUITS];
extern long seed;
extern char *input_file;

extern void pointcount(int, int);
extern void setshapebit(int, int, int, int, int, int);
extern void clearpointcount(void);
extern void clearpointcount_alt(int);
extern void predeal(int, Card);
extern void *mycalloc(unsigned, size_t);
extern void yyerror(const char *);

#ifdef FRANCOIS
int hascard(Deal, int, Card, int);
#define HAS_CARD(d, p, c) hascard(d, p, c, 0)
#else
int hascard(Deal, int, Card);
#define HAS_CARD(d, p, c) hascard(d, p, c)
#endif

Card make_card(char rankchar, char suitchar);
extern int make_contract(char suitchar, char trickchar);

#endif /* DEALER_H */
