#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <limits.h>

static int quiet = 0;

#include <netinet/in.h>
#include <unistd.h>
#include <sys/time.h>

#include "getopt.h"
#include "tree.h"
#include "pointcount.h"
#include "dealer.h"
#include "c4.h"
#include "pbn.h"

#ifdef FRANCOIS
static const int TWO_TO_THE_13 = (1 << 13);
#endif
static const int RANDBITS = 16;
static const int NRANDVALS = (1 << RANDBITS);

/* Global variables */

enum { STAT_MODE, EXHAUST_MODE };
static const int DEFAULT_MODE = STAT_MODE;
static int computing_mode = DEFAULT_MODE;

int uppercase = 0;
char lcrep[] = "23456789tjqka";
char ucrep[] = "23456789TJQKA";
#define representation (uppercase ? ucrep : lcrep);

static const int imparr[24] = {10,  40,  80,   120,  160,  210,  260,  310,  360,  410,  490,  590,
                               740, 890, 1090, 1190, 1490, 1740, 1990, 2240, 2490, 2990, 3490, 3990};

static Deal fullpack;
static Deal stacked_pack;

static int swapping = 0;
static int swapindex = 0;
static int loading = 0;
static int loadindex = 0;

/* Various handshapes can be asked for. For every shape the user is
   interested in a number is generated. In every distribution that fits that
   shape the corresponding bit is set in the distrbitmaps 4-dimensional array.
   This makes looking up a shape a small constant cost.
*/
static int ***distrbitmaps[14];

static int results[2][5][14];
static int nprod;
static int ngen;

static Tree defaulttree = {TreeType::Number, NIL, NIL, 1, 0};
static Action defaultaction = {(Action *)0, ActionType::PrintAll};
static unsigned char zero52[NRANDVALS];
static Deal *deallist;

/* Function definitions */
static void fprintcompact(FILE *, Deal, int);
static int trix(char);
static void error(const char *);
static void printew(Deal d);
static int true_dd(Deal d, int l, int c);
extern void yyparse();

/* globals */
Deal curdeal;
Handstat hs[4];
const char *input_file = 0;
int maxgenerate;
int maxdealer;
int maxproduce;
int maxvuln;
long seed = 0;
bool use_compass[NSUITS];
bool will_print;
Tree *decisiontree = &defaulttree;
Action *actionlist = &defaultaction;
const char *player_name[] = {"North", "East", "South", "West"};
const Tree *NIL = nullptr;

#ifdef FRANCOIS
/* Special variables for exhaustive mode
   Exhaustive mode created by Francois DELLACHERIE, 01-1999  */
int vectordeal;
/* a vectordeal is a binary *word* of at most 26 bits: there are at most
   26 cards to deal between two players.  Each card to deal will be
   affected to a given position in the vector.  Example : we need to deal
   the 26 minor cards between north and south (what a double fit !!!)
   Then those cards will be represented in a binary vector as shown below:
        Card    : DA DK ... D2 CA CK ... C4 C3 C2
        Bit-Pos : 25 24     13 12 11      2  1  0
   The two players will be respectively represented by the bit values 0 and
   1. Let's say north gets the 0, and south the 1. Then the vector
   11001100000001111111110000 represents the following hands :
        North: D QJ8765432 C 5432
        South: D AKT9      C AKQJT9876
   A suitable vectordeal is a vector deal with hamming weight equal to 13.
   For computing those vectors, we use a straightforward
   meet in the middle approach.  */
int exh_predealt_vector = 0;
char exh_card_map[256];
/* exh_card_map[card] is the coordinate of the card in the vector */
Card exh_card_at_bit[26];
/* exh_card_at_bit[i] is the card pointed by the bit #i */
unsigned char exh_player[2];
/* the two players that have unknown cards */
unsigned char exh_empty_slots[2];
/* the number of empty slots of those players */
unsigned char exh_suit_length_at_map[NSUITS][26];
/* exh_suit_length_at_map[SUIT][POS] = 1 if the card at
   position POS is from the suit SUIT */
unsigned char exh_suit_points_at_map[NSUITS][26];
/* same as above with the hcp-value instead of 1 */
unsigned char exh_msb_suit_length[NSUITS][TWO_TO_THE_13];
/* we split the the vector deal into 2 sub-vectors of length 13. Example for
   the previous vector: 1100110000000 and 11111111110000
                        msb               lsb
   exh_msb_suit_length[SUIT][MSB_VECTOR] represents the msb-contribution of
   the MSB_VECTOR for the length of the hand for suit SUIT, and for player 0.
   In the trivial above example, we have: exh_msb_suit_length[SUIT][MSBs] = 0
   unless SUIT == DIAMOND, = the number of 0 in MSB, otherwise.*/
unsigned char exh_lsb_suit_length[NSUITS][TWO_TO_THE_13];
/* same as above for the lsb-sub-vector */
unsigned char exh_msb_suit_points[NSUITS][TWO_TO_THE_13];
/* same as above for the hcp's in the suits */
unsigned char exh_lsb_suit_points[NSUITS][TWO_TO_THE_13];
/* same as above for the lsb-sub-vector */
unsigned char exh_msb_totalpoints[TWO_TO_THE_13];
/* the total of the points of the cards represented by the
   13 most significant bits in the vector deal */
unsigned char exh_lsb_totalpoints[TWO_TO_THE_13];
/* same as above for the lsb */
unsigned char exh_total_cards_in_suit[NSUITS];
/* the total of cards remaining to be dealt in each suit */
unsigned char exh_total_points_in_suit[NSUITS];
/* the total of hcps  remaining to be dealt in each suit */
unsigned char exh_total_points;
/* the total of hcps  remaining to be dealt */
int *HAM_T[14], Tsize[14];
/* Hamming-related tables.  See explanation below... */
int completely_known_hand[4] = {0, 0, 0, 0};
#endif /* FRANCOIS */

static void initevalcontract() {
    int i, j, k;
    for (i = 0; i < 2; i++)
        for (j = 0; j < 5; j++)
            for (k = 0; k < 14; k++)
                results[i][j][k] = 0;
}

static int imps(int scorediff) {
    int i, j;
    j = abs(scorediff);
    for (i = 0; i < 24; i++)
        if (imparr[i] >= j)
            return scorediff < 0 ? -i : i;
    return scorediff < 0 ? -i : i;
}

static int score(int vuln, int suit, int level, int tricks) {
    int total = 0;

    /* going down */
    if (tricks < 6 + level)
        return -50 * (1 + vuln) * (6 + level - tricks);

    /* Tricks */
    total = total + ((suit >= SUIT_HEART) ? 30 : 20) * (tricks - 6);

    /* NT bonus */
    if (suit == SUIT_NT)
        total += 10;

    /* part score bonus */
    total += 50;

    /* game bonus for NT */
    if ((suit == SUIT_NT) && level >= 3)
        total += 250 + 200 * vuln;

    /* game bonus for major-partscore bns */
    if ((suit == SUIT_HEART || suit == SUIT_SPADE) && level >= 4)
        total += 250 + 200 * vuln;

    /* game bonus for minor-partscore bns */
    if ((suit == SUIT_CLUB || suit == SUIT_DIAMOND) && level >= 5)
        total += 250 + 200 * vuln;

    /* small slam bonus */
    if (level == 6)
        total += 500 + 250 * vuln;

    /* grand slam bonus */
    if (level == 7)
        total += 1000 + 500 * vuln;

    return total;
}

static void showevalcontract(int nh) {
    int s, l, i, v;
    for (v = 0; v < 2; v++) {
        printf("%sVulnerable\n", v ? "" : "Not ");
        printf("   ");
        for (l = 1; l < 8; l++)
            printf("  %d       ", l);
        printf("\n");
        for (s = 0; s < 5; s++) {
            printf("%c: ", "cdhsn"[s]);
            for (l = 1; l < 8; l++) {
                int t = 0, tn = 0;
                for (i = 0; i < 14; i++) {
                    t += results[0][s][i] * score(v, s, l, i);
                    tn += results[1][s][i] * score(v, s, l, i);
                }
                printf("%4d/%4d ", t / nh, tn / nh);
            }
            printf("\n");
        }
        printf("\n");
    }
}

static int dd(Deal d, int l, int c) {
    /* results-cached version of dd() */
    /* the dd cache, and the ngen it refers to */
    static int cached_ngen = -1;
    static char cached_tricks[4][5];

    /* invalidate cache if it's another deal */
    if (ngen != cached_ngen) {
        memset(cached_tricks, -1, sizeof(cached_tricks));
        cached_ngen = ngen;
    }
    if (cached_tricks[l][c] == -1) {
        /* cache the costly computation's result */
        cached_tricks[l][c] = true_dd(d, l, c);
    }

    /* return the cached value */
    return cached_tricks[l][c];
}

static struct tagLibdeal {
    unsigned long suits[4];
    unsigned short tricks[5];
    int valid;
} libdeal;

static int get_tricks(int pn, int dn) {
    int tk = libdeal.tricks[dn];
    int resu;
    resu = (pn ? (tk >> (4 * pn)) : tk) & 0x0F;
    return resu;
}

int true_dd(Deal d, int l, int c) {
    if (loading && libdeal.valid) {
        int resu = get_tricks((l + 1) % 4, (c + 1) % 5);
        /* This will get the number of tricks EW can get.  If the user wanted NW,
           we have to subtract 13 from that number. */
        return ((l == 0) || (l == 2)) ? 13 - resu : resu;
    } else {
        FILE *f;
        char cmd[1024];
        char tn1[256], tn2[256];
        char res;

        f = fopen(tn1, "w+");
        if (f == 0)
            error("Can't open temporary file");
        fprintcompact(f, d, 0);
        fprintf(f, "%c %c\n", "eswn"[l], "cdhsn"[c]);
        fclose(f);
        tmpnam(tn2);
        sprintf(cmd, "bridge -d -q %s >%s", tn1, tn2);
        system(cmd);
        f = fopen(tn2, "r");
        if (f == 0)
            error("Can't read output of analysis");
        fscanf(f, "%*[^\n]\n%c", &res);
        fclose(f);
        remove(tn1);
        remove(tn2);
        /* This will get the number of tricks EW can get.  If the user wanted NW,
           we have to subtract 13 from that number. */
        return ((l == 1) || (l == 3)) ? 13 - trix(res) : trix(res);
    }
}

// FIXME: no caller and incomplete!
void evalcontract() {
    int s;
    for (s = 0; s < 5; s++) {
        results[1][s][dd(curdeal, 3, s)]++; /* south declarer */
        results[0][s][dd(curdeal, 1, s)]++; /* north declarer */
    }
}

void error(const char *s) {
    fprintf(stderr, "%s\n", s);
    exit(10);
}

/* implementations of regular & alternate pointcounts */

static int countindex = -1;

static void zerocount(int points[13]) {
    int i;
    for (i = 12; i >= 0; i--)
        points[i] = 0;
}

void clearpointcount() {
    zerocount(tblPointcount[idxHcp]);
    countindex = -1;
}

void clearpointcount_alt(int cin) {
    zerocount(tblPointcount[cin]);
    countindex = cin;
}

void pointcount(int index, int value) {
    assert(index <= 12);
    if (index < 0) {
        yyerror("too many pointcount values");
    }
    if (countindex < 0)
        tblPointcount[idxHcp][index] = value;
    else
        tblPointcount[countindex][index] = value;
}

void *mycalloc(unsigned nel, size_t siz) {
    void *p = calloc(nel, siz);
    if (p)
        return p;
    fprintf(stderr, "Out of memory\n");
    exit(-1); /*NOTREACHED */
}

static void initdistr() {
    int ***p4, **p3, *p2;
    int clubs, diamonds, hearts;

    /* Allocate the four dimensional pointer array */

    for (clubs = 0; clubs <= 13; clubs++) {
        p4 = (int ***)mycalloc((unsigned)14 - clubs, sizeof(*p4));
        distrbitmaps[clubs] = p4;
        for (diamonds = 0; diamonds <= 13 - clubs; diamonds++) {
            p3 = (int **)mycalloc((unsigned)14 - clubs - diamonds, sizeof(*p3));
            p4[diamonds] = p3;
            for (hearts = 0; hearts <= 13 - clubs - diamonds; hearts++) {
                p2 = (int *)mycalloc((unsigned)14 - clubs - diamonds - hearts, sizeof(*p2));
                p3[hearts] = p2;
            }
        }
    }
}

void setshapebit(int cl, int di, int ht, int sp, int msk, int excepted) {
    if (excepted)
        distrbitmaps[cl][di][ht][sp] &= ~msk;
    else
        distrbitmaps[cl][di][ht][sp] |= msk;
}

static void newpack(Deal d) {
    int suit, rank, place;

    place = 0;
    for (suit = SUIT_CLUB; suit <= SUIT_SPADE; suit++)
        for (rank = 0; rank < 13; rank++)
            d[place++] = make_card(suit, rank);
}

#ifdef FRANCOIS
int hascard(Deal d, int player, Card onecard, int vectordeal) {
    int i;
    int who;
    switch (computing_mode) {
        case STAT_MODE:
            for (i = player * 13; i < (player + 1) * 13; i++)
                if (d[i] == onecard)
                    return 1;
            return 0;
            break;
        case EXHAUST_MODE:
            if (exh_card_map[onecard] == -1)
                return 0;
            who = 1 & (vectordeal >> exh_card_map[onecard]);
            return (exh_player[who] == player);
    }
    return 0;
#else
int hascard(Deal d, int player, Card onecard) {
    int i;

    for (i = player * 13; i < (player + 1) * 13; i++)
        if (d[i] == onecard)
            return 1;
    return 0;
#endif /* FRANCOIS */
}

Card make_card(char rankchar, char suitchar) {
    int rank, suit = 0;

    for (rank = 0; rank < 13 && ucrep[rank] != rankchar; rank++)
        ;
    assert(rank < 13);
    switch (suitchar) {
        case 'C':
            suit = 0;
            break;
        case 'D':
            suit = 1;
            break;
        case 'H':
            suit = 2;
            break;
        case 'S':
            suit = 3;
            break;
        default:
            assert(0);
    }
    return make_card(suit, rank);
}

int make_contract(char suitchar, char trickchar) {
    int trick, suit;
    trick = (int)trickchar - '0';
    switch (suitchar) {
        case 'C':
            suit = 0;
            break;
        case 'D':
            suit = 1;
            break;
        case 'H':
            suit = 2;
            break;
        case 'S':
            suit = 3;
            break;
        case 'N':
            suit = 4;
            break;
        default:
            suit = 0;
            printf("%c", suitchar);
            assert(0);
    }
    return make_contract(suit, trick);
}

static void analyze(Deal d, Handstat *hsbase) {
    /* Analyze a hand.  Modified by HU to count controls and losers. */
    /* Further mod by AM to count several alternate pointcounts too  */

    int player, next, c, r, s, t;
    Card curcard;
    Handstat *hs;

    /* for each player */
    for (player = COMPASS_NORTH; player <= COMPASS_WEST; ++player) {
        /* If the expressions in the input never mention a player
           we do not calculate his hand statistics. */
        if (!use_compass[player]) {
#ifdef _DEBUG
            /* In debug mode, blast the unused handstat, so that we can recognize it
               as garbage should if we accidently read from it */
            hs = hsbase + player;
            memset(hs, 0xDF, sizeof(Handstat));
#endif /* _DEBUG_ */

            continue;
        }
        /* where are the handstats for this player? */
        hs = hsbase + player;

        /* Initialize the handstat structure */
        memset(hs, 0x00, sizeof(Handstat));

#ifdef _DEBUG
        /* To debug, blast it with garbage.... */
        memset(hs, 0xDF, sizeof(Handstat));

        /* then overwrite those specific counters which need to be incremented */
        for (t = idxHcp; t < idxEnd; ++t) {
            /* clear the points for each suit */
            for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
                hs->hs_counts[t][s] = 0;
            }
            /* and the total points as well */
            hs->hs_totalcounts[t] = 0;
        }

        /* clear the length and distribution for each suit */
        for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
            hs->hs_length[s] = 0;
            hs->hs_dist[s] = 0;
        }
        /* clear the total distribution */
        hs->hs_totaldist = 0;
        /* clear the total losers */
        hs->hs_totalloser = 0;
#endif /* _DEBUG_ */

        /* start from the first card for this player, and walk through them all -
           use the player offset to jump to the first card.  Can't just increment
           through the deck, because we skip those players who are not part of the
           analysis */
        next = 13 * player;
        for (c = 0; c < 13; c++) {
            curcard = d[next++];
            s = card_suit(curcard);
            r = card_rank(curcard);

/* Enable this #if to dump a visual look at the hands as they are
   analysed.  Best bet is to use test.all, or something else that
   generates only one hand, lest you quickly run out of screen
   realestate */
#if 0
#ifdef _DEBUG
#define VIEWCOUNT
#endif /* _DEBUG_ */
#endif /* 0 */

#ifdef VIEWCOUNT
            printf("%c%c", "CDHS"[s], "23456789TJQKA"[r]);
#endif /* VIEWCOUNT */
            hs->hs_length[s]++;
            for (t = idxHcp; t < idxEnd; ++t) {
#ifdef VIEWCOUNT
                printf(" %d ", tblPointcount[t][r]);
#endif /* VIEWCOUNT */
                hs->hs_counts[t][s] += tblPointcount[t][r];
            }
#ifdef VIEWCOUNT
            printf("\n");
#endif /* VIEWCOUNT */
        }
#ifdef VIEWCOUNT
        printf("---\n");
#undef VIEWCOUNT
#endif /* VIEWCOUNT */

        for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
            assert(hs->hs_length[s] < 14);
            assert(hs->hs_length[s] >= 0);
            switch (hs->hs_length[s]) {
                case 0: {
                    /* A void is 0 losers */
                    hs->hs_loser[s] = 0;
                    break;
                }
                case 1: {
                    /* Singleton A 0 losers, K or Q 1 loser */
                    int losers[] = {1, 1, 0};
                    assert(hs->hs_control[s] < 3);
                    hs->hs_loser[s] = losers[hs->hs_counts[idxControls][s]];
                    break;
                }
                case 2: {
                    /* Doubleton AK 0 losers, Ax or Kx 1, Qx 2 */
                    int losers[] = {2, 1, 1, 0};
                    assert(hs->hs_control[s] <= 3);
                    hs->hs_loser[s] = losers[hs->hs_counts[idxControls][s]];
                    break;
                }
                default: {
                    /* Losers, first correct the number of losers */
                    assert(hs->hs_counts[idxWinners][s] < 4);
                    assert(hs->hs_counts[idxWinners][s] >= 0);
                    hs->hs_loser[s] = 3 - hs->hs_counts[idxWinners][s];
                    break;
                }
            }

            /* Now add the losers to the total. */
            hs->hs_totalloser += hs->hs_loser[s];

            /* total up the other flavors of points */
            for (t = idxHcp; t < idxEnd; ++t) {
                hs->hs_totalcounts[t] += hs->hs_counts[t][s];
            }

            /* Now, using the values calculated already, load those pointcount
               values which are common enough to warrant a non array lookup */
            hs->hs_hcp[s] = hs->hs_counts[idxHcp][s];
            hs->hs_control[s] = hs->hs_counts[idxControls][s];
            hs->hs_dist[s] = hs->hs_length[s] > 4 ? hs->hs_length[s] - 4 : 0;
            hs->hs_totaldist += hs->hs_dist[s];

        } /* end for each suit */

        hs->hs_totalhcp = hs->hs_totalcounts[idxHcp];
        hs->hs_totalcontrol = hs->hs_totalcounts[idxControls];

        hs->hs_bits = distrbitmaps[hs->hs_length[SUIT_CLUB]][hs->hs_length[SUIT_DIAMOND]][hs->hs_length[SUIT_HEART]]
                                  [hs->hs_length[SUIT_SPADE]];
    } /* end for each player */
}

void fprintcompact(FILE *f, Deal d, int ononeline) {
    char pt[] = "nesw";
    int s, p, r;
    for (p = COMPASS_NORTH; p <= COMPASS_WEST; p++) {
        fprintf(f, "%c ", pt[p]);
        for (s = SUIT_SPADE; s >= SUIT_CLUB; s--) {
            for (r = 12; r >= 0; r--)
                if (HAS_CARD(d, p, make_card(s, r)))
                    fprintf(f, "%c", ucrep[r]);
            if (s > 0)
                fprintf(f, ".");
        }
        /* OK to use \n as this is mainly intended for internal dealer use. */
        fprintf(f, ononeline ? " " : "\n");
    }
}

static void printdeal(Deal d) {
    int suit, player, rank, cards;

    printf("%4d.\n", (nprod + 1));

    for (suit = SUIT_SPADE; suit >= SUIT_CLUB; suit--) {
        cards = 10;
        for (player = COMPASS_NORTH; player <= COMPASS_WEST; player++) {
            while (cards < 10) {
                printf("  ");
                cards++;
            }
            cards = 0;
            for (rank = 12; rank >= 0; rank--) {
                if (HAS_CARD(d, player, make_card(suit, rank))) {
                    printf("%c ", ucrep[rank]);
                    cards++;
                }
            }
            if (cards == 0) {
                printf("- ");
                cards++;
            }
        }
        printf("\n");
    }
    printf("\n");
}

static void setup_deal() {
    int i, j;

    j = 0;
    for (i = 0; i < 52; i++) {
        if (stacked_pack[i] != NO_CARD) {
            curdeal[i] = stacked_pack[i];
        } else {
            while (fullpack[j] == NO_CARD)
                j++;
            curdeal[i] = fullpack[j++];
            assert(j <= 52);
        }
    }
}

void predeal(int player, Card onecard) {
    int i, j;

    for (i = 0; i < 52; i++) {
        if (fullpack[i] == onecard) {
            fullpack[i] = NO_CARD;
            for (j = player * 13; j < (player + 1) * 13; j++)
                if (stacked_pack[j] == NO_CARD) {
                    stacked_pack[j] = onecard;
                    return;
                }
            yyerror("More than 13 cards for one player");
        }
    }
    yyerror("Card predealt twice");
}

static void initprogram() {
    int i, i_cycle;
    int val;

    /* Now initialize array zero52 with numbers 0..51 repeatedly. This whole
       charade is just to prevent having to do divisions. */
    val = 0;
    for (i = 0, i_cycle = 0; i < NRANDVALS; i++) {
        while (stacked_pack[val] != NO_CARD) {
            /* this slot is predealt, do not use it */
            val++;
            if (val == 52) {
                val = 0;
                i_cycle = i;
            }
        }
        zero52[i] = val++;
        if (val == 52) {
            val = 0;
            i_cycle = i + 1;
        }
    }
    /* Fill the last part of the array with 0xFF, just to prevent
       that 0 occurs more than 51. This is probably just for hack value */
    while (i > i_cycle) {
        zero52[i - 1] = 0xFF;
        i--;
    }
}

static void swap2(Deal d, int p1, int p2) {
    /* functions to assist "simulated" shuffling with player
       swapping or loading from Ginsberg's library.dat -- AM990423 */
    Card t;
    int i;
    p1 *= 13;
    p2 *= 13;
    for (i = 0; i < 13; ++i) {
        t = d[p1 + i];
        d[p1 + i] = d[p2 + i];
        d[p2 + i] = t;
    }
}

static FILE *find_library(const char *basename, const char *openopt) {
    static const char *prefixes[] = {
        "", "./", "../", "../../", "c:/", "c:/data/", "d:/myprojects/dealer/", "d:/arch/games/gib/", 0};
    int i;
    char buf[256];
    FILE *result = 0;
    for (i = 0; prefixes[i]; ++i) {
        strcpy(buf, prefixes[i]);
        strcat(buf, basename);
        result = fopen(buf, openopt);
        if (result)
            break;
    }
    return result;
}

static int shuffle(Deal d) {
    int i, j, k;
    Card t;

    if (loading) {
        static FILE *lib = 0;
        if (!lib) {
            lib = find_library("library.dat", "rb");
            if (!lib) {
                fprintf(stderr, "Cannot find or open library file\n");
                exit(-1);
            }
            fseek(lib, 26 * loadindex, SEEK_SET);
        }
        if (fread(&libdeal, 26, 1, lib)) {
            int ph[4], i, suit, rank, pn;
            unsigned long su;
            libdeal.valid = 1;
            for (i = 0; i < 4; ++i)
                ph[i] = 13 * i;
            for (i = 0; i <= 4; ++i) {
                libdeal.tricks[i] = ntohs(libdeal.tricks[i]);
            }
            for (suit = 0; suit < 4; ++suit) {
                su = libdeal.suits[suit];
                su = ntohl(su);
                for (rank = 0; rank < 13; ++rank) {
                    pn = su & 0x03;
                    su >>= 2;
                    d[ph[pn]++] = make_card(suit, 12 - rank);
                }
            }
            return 1;
        } else {
            libdeal.valid = 0;
            return 0;
        }
    }

    if (swapindex) {
        switch (swapindex) {
            case 1:
                swap2(d, 1, 3);
                break;
            case 2:
                swap2(d, 2, 3);
                break;
            case 3:
                swap2(d, 1, 2);
                break;
            case 4:
                swap2(d, 1, 3);
                break;
            case 5:
                swap2(d, 2, 3);
                break;
        }
    } else {
        /* Algorithm according to Knuth. For each card exchange with a random
           other card. This is supposed to be the perfect shuffle algorithm.
           It only depends on a valid random number generator.  */
        for (i = 0; i < 52; i++) {
            if (stacked_pack[i] == NO_CARD) {
                /* Thorvald Aagaard 14.08.1999 don't switch a predealt card */
                do {
                    do {
#ifdef STD_RAND
                        k = RANDOM();
#else
                        /* Upper bits most random */
                        k = (RANDOM() >> (31 - RANDBITS));
#endif /* STD_RAND */
                        j = zero52[k];
                    } while (j == 0xFF);
                } while (stacked_pack[j] != NO_CARD);

                t = d[j];
                d[j] = d[i];
                d[i] = t;
            }
        }
    }
    if (swapping) {
        ++swapindex;
        if ((swapping == 2 && swapindex > 1) || (swapping == 3 && swapindex > 5))
            swapindex = 0;
    }
    return 1;
}

#ifdef FRANCOIS
/* Specific routines for EXHAUST_MODE */

void exh_get2players() {
    /* Just finds who are the 2 players for whom we make exhaustive dealing */
    int player, player_bit;
    for (player = COMPASS_NORTH, player_bit = 0; player <= COMPASS_WEST; player++) {
        if (completely_known_hand[player]) {
            if (use_compass[player]) {
                /* We refuse to compute anything for a player who has already his (her)
                   13 cards */
                fprintf(stderr, "Exhaust-mode error: cannot compute anything for %s (known hand)\n",
                        player_name[player]);
                exit(-1); /*NOTREACHED */
            }
        } else {
            if (player_bit == 2) {
                /* Exhaust mode only if *exactly* 2 hands have unknown cards */
                fprintf(stderr, "Exhaust-mode error: more than 2 unknown hands...\n");
                exit(-1); /*NOTREACHED */
            }
            exh_player[player_bit++] = player;
        }
    }
    if (player_bit < 2) {
        /* Exhaust mode only if *exactly* 2 hands have unknown cards */
        fprintf(stderr, "Exhaust-mode error: less than 2 unknown hands...\n");
        exit(-1); /*NOTREACHED */
    }
}

void exh_set_bit_values(int bit_pos, Card onecard) {
    /* only sets up some tables (see table definitions above) */
    int suit, rank;
    int suitloop;
    suit = card_suit(onecard);
    rank = card_rank(onecard);
    for (suitloop = SUIT_CLUB; suitloop <= SUIT_SPADE; suitloop++) {
        exh_suit_points_at_map[suitloop][bit_pos] = (suit == suitloop ? tblPointcount[0][rank] : 0);
        exh_suit_length_at_map[suitloop][bit_pos] = (suit == suitloop ? 1 : 0);
    }
    exh_card_map[onecard] = bit_pos;
    exh_card_at_bit[bit_pos] = onecard;
}

void exh_setup_card_map() {
    int i;
    for (i = 0; i < 256; i++) {
        exh_card_map[i] = -1; /* undefined */
    }
}

void exh_map_cards() {
    int i, i_player;
    int bit_pos;

    for (i = 0, bit_pos = 0; i < 52; i++) {
        if (fullpack[i] != NO_CARD) {
            exh_set_bit_values(bit_pos, fullpack[i]);
            bit_pos++;
        }
    }
    /* Some cards may also have been predealt for the exh-players. In that case,
       those cards are "put" in the msb positions of the vectordeal. The value
       of the exh_predealt_vector is the constant part of the vector deal. */
    for (i_player = 0; i_player < 2; i_player++) {
        int player = exh_player[i_player];
        exh_empty_slots[i_player] = 0;
        for (i = 13 * player; i < 13 * (player + 1); i++) {
            if (stacked_pack[i] == NO_CARD) {
                exh_empty_slots[i_player]++;
            } else {
                exh_set_bit_values(bit_pos, stacked_pack[i]);
                exh_predealt_vector |= (i_player << bit_pos);
                bit_pos++;
            }
        }
    }
}

void exh_print_stats(Handstat *hs) {
    int s;
    for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
        printf("  Suit %d: ", s);
        printf("Len = %2d, Points = %2d\n", hs->hs_length[s], hs->hs_hcp[s]);
    }
    printf("  Totalpoints: %2d\n", hs->hs_totalhcp);
}

void exh_print_vector(Handstat *hs) {
    int i, s, r;
    int onecard;
    Handstat *hsp;

    printf("Player %d: ", exh_player[0]);
    for (i = 0; i < 26; i++) {
        if (!(1 & (vectordeal >> i))) {
            onecard = exh_card_at_bit[i];
            s = card_suit(onecard);
            r = card_rank(onecard);
            printf("%c%d ", ucrep[r], s);
        }
    }
    printf("\n");
    hsp = hs + exh_player[0];
    exh_print_stats(hsp);
    printf("Player %d: ", exh_player[1]);
    for (i = 0; i < 26; i++) {
        if ((1 & (vectordeal >> i))) {
            onecard = exh_card_at_bit[i];
            s = card_suit(onecard);
            r = card_rank(onecard);
            printf("%c%d ", ucrep[r], s);
        }
    }
    printf("\n");
    hsp = hs + exh_player[1];
    exh_print_stats(hsp);
}

void exh_precompute_analyse_tables(void) {
    /* This routine precomputes the values of the tables exh_lsb_... and
       exh_msb_... These tables will allow very fast computation of
       hand-primitives given the value of the vector deal.
       Example: the number of hcp in diamonds of the player 0 will be :
              exh_lsb_suit_length[vectordeal & (TWO_TO_THE_13-1)]
            + exh_msb_suit_length[vectordeal >> 13];
       The way those table are precomputed may seem a little bit ridiculous.
       This may be the reminiscence of Z80-programming ;-) FD-0499
    */
    int vec_13;
    int i_bit;
    int suit;
    unsigned char *elsp, *emsp, *elt, *emt, *elsl, *emsl;
    unsigned char *mespam, *meslam;
    unsigned char *lespam, *leslam;

    for (vec_13 = 0; vec_13 < TWO_TO_THE_13; vec_13++) {
        elt = exh_lsb_totalpoints + vec_13;
        emt = exh_msb_totalpoints + vec_13;
        *elt = *emt = 0;
        for (suit = SUIT_CLUB; suit <= SUIT_SPADE; suit++) {
            elsp = exh_lsb_suit_points[suit] + vec_13;
            emsp = exh_msb_suit_points[suit] + vec_13;
            elsl = exh_lsb_suit_length[suit] + vec_13;
            emsl = exh_msb_suit_length[suit] + vec_13;
            lespam = exh_suit_points_at_map[suit] + 12;
            leslam = exh_suit_length_at_map[suit] + 12;
            mespam = exh_suit_points_at_map[suit] + 25;
            meslam = exh_suit_length_at_map[suit] + 25;
            *elsp = *emsp = *elsl = *emsl = 0;
            for (i_bit = 13; i_bit--; lespam--, leslam--, mespam--, meslam--) {
                if (!(1 & (vec_13 >> i_bit))) {
                    *(elsp) += *lespam;
                    *(emsp) += *mespam;
                    *(elt) += *lespam;
                    *(emt) += *mespam;
                    *(elsl) += *leslam;
                    *(emsl) += *meslam;
                }
            }
        }
    }
    for (suit = SUIT_CLUB; suit <= SUIT_SPADE; suit++) {
        exh_total_points_in_suit[suit] = exh_lsb_suit_points[suit][0] + exh_msb_suit_points[suit][0];
        exh_total_cards_in_suit[suit] = exh_lsb_suit_length[suit][0] + exh_msb_suit_length[suit][0];
    }
    exh_total_points = exh_lsb_totalpoints[0] + exh_msb_totalpoints[0];
}

void exh_analyze_vec(int high_vec, int low_vec, Handstat *hs) {
    /* analyse the 2 remaining hands with the vectordeal data-structure.
       This is VERY fast !!!  */
    int s;
    Handstat *hs0;
    Handstat *hs1;
    hs0 = hs + exh_player[0];
    hs1 = hs + exh_player[1];
    hs0->hs_totalhcp = hs1->hs_totalhcp = 0;
    for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
        hs0->hs_length[s] = exh_lsb_suit_length[s][low_vec] + exh_msb_suit_length[s][high_vec];
        hs0->hs_hcp[s] = exh_lsb_suit_points[s][low_vec] + exh_msb_suit_points[s][high_vec];
        hs1->hs_length[s] = exh_total_cards_in_suit[s] - hs0->hs_length[s];
        hs1->hs_hcp[s] = exh_total_points_in_suit[s] - hs0->hs_hcp[s];
    }
    hs0->hs_totalhcp = exh_lsb_totalpoints[low_vec] + exh_msb_totalpoints[high_vec];
    hs1->hs_totalhcp = exh_total_points - hs0->hs_totalhcp;
    hs0->hs_bits = distrbitmaps[hs0->hs_length[SUIT_CLUB]][hs0->hs_length[SUIT_DIAMOND]][hs0->hs_length[SUIT_HEART]]
                               [hs0->hs_length[SUIT_SPADE]];
    hs1->hs_bits = distrbitmaps[hs1->hs_length[SUIT_CLUB]][hs1->hs_length[SUIT_DIAMOND]][hs1->hs_length[SUIT_HEART]]
                               [hs1->hs_length[SUIT_SPADE]];
}

/* End of Specific routines for EXHAUST_MODE */
#endif /* FRANCOIS */

int trix(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    return c - 'A' + 10;
}

static int evaltree(const Tree *t) {
    switch (t->tr_type) {
        default:
            assert(0);
        case TreeType::Number:
            return t->tr_int1;
        case TreeType::And2:
            return evaltree(t->tr_leaf1) && evaltree(t->tr_leaf2);
        case TreeType::Or2:
            return evaltree(t->tr_leaf1) || evaltree(t->tr_leaf2);
        case TreeType::ArPlus:
            return evaltree(t->tr_leaf1) + evaltree(t->tr_leaf2);
        case TreeType::ArMinus:
            return evaltree(t->tr_leaf1) - evaltree(t->tr_leaf2);
        case TreeType::Artimes:
            return evaltree(t->tr_leaf1) * evaltree(t->tr_leaf2);
        case TreeType::ArDivide:
            return evaltree(t->tr_leaf1) / evaltree(t->tr_leaf2);
        case TreeType::ArMod:
            return evaltree(t->tr_leaf1) % evaltree(t->tr_leaf2);
        case TreeType::CmpEQ:
            return evaltree(t->tr_leaf1) == evaltree(t->tr_leaf2);
        case TreeType::CmpNE:
            return evaltree(t->tr_leaf1) != evaltree(t->tr_leaf2);
        case TreeType::CmpLT:
            return evaltree(t->tr_leaf1) < evaltree(t->tr_leaf2);
        case TreeType::CmpLE:
            return evaltree(t->tr_leaf1) <= evaltree(t->tr_leaf2);
        case TreeType::CmpGT:
            return evaltree(t->tr_leaf1) > evaltree(t->tr_leaf2);
        case TreeType::CmpGE:
            return evaltree(t->tr_leaf1) >= evaltree(t->tr_leaf2);
        case TreeType::Not:
            return !evaltree(t->tr_leaf1);
        case TreeType::Length: /* suit, compass */
            assert(t->tr_int1 >= SUIT_CLUB && t->tr_int1 <= SUIT_SPADE);
            assert(t->tr_int2 >= COMPASS_NORTH && t->tr_int2 <= COMPASS_WEST);
            return hs[t->tr_int2].hs_length[t->tr_int1];
        case TreeType::HcpTotal: /* compass */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            return hs[t->tr_int1].hs_totalhcp;
        case TreeType::DistPtsTotal: /* compass */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            return hs[t->tr_int1].hs_totaldist;
        case TreeType::Pt0Total: /* compass */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            return hs[t->tr_int1].hs_totalcounts[idxTens];
        case TreeType::Pt1Total: /* compass */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            return hs[t->tr_int1].hs_totalcounts[idxJacks];
        case TreeType::Pt2Total: /* compass */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            return hs[t->tr_int1].hs_totalcounts[idxQueens];
        case TreeType::Pt3Total: /* compass */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            return hs[t->tr_int1].hs_totalcounts[idxKings];
        case TreeType::Pt4Total: /* compass */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            return hs[t->tr_int1].hs_totalcounts[idxAces];
        case TreeType::Pt5Total: /* compass */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            return hs[t->tr_int1].hs_totalcounts[idxTop2];
        case TreeType::Pt6Total: /* compass */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            return hs[t->tr_int1].hs_totalcounts[idxTop3];
        case TreeType::Pt7Total: /* compass */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            return hs[t->tr_int1].hs_totalcounts[idxTop4];
        case TreeType::Pt8Total: /* compass */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            return hs[t->tr_int1].hs_totalcounts[idxTop5];
        case TreeType::Pt9Total: /* compass */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            return hs[t->tr_int1].hs_totalcounts[idxC13];
        case TreeType::Hcp: /* compass, suit */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            assert(t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
            return hs[t->tr_int1].hs_hcp[t->tr_int2];
        case TreeType::DistPts: /* compass, suit */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            assert(t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
            return hs[t->tr_int1].hs_dist[t->tr_int2];
        case TreeType::Pt0: /* compass, suit */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            assert(t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
            return hs[t->tr_int1].hs_counts[idxTens][t->tr_int2];
        case TreeType::Pt1: /* compass, suit */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            assert(t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
            return hs[t->tr_int1].hs_counts[idxJacks][t->tr_int2];
        case TreeType::Pt2: /* compass, suit */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            assert(t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
            return hs[t->tr_int1].hs_counts[idxQueens][t->tr_int2];
        case TreeType::Pt3: /* compass, suit */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            assert(t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
            return hs[t->tr_int1].hs_counts[idxKings][t->tr_int2];
        case TreeType::Pt4: /* compass, suit */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            assert(t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
            return hs[t->tr_int1].hs_counts[idxAces][t->tr_int2];
        case TreeType::Pt5: /* compass, suit */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            assert(t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
            return hs[t->tr_int1].hs_counts[idxTop2][t->tr_int2];
        case TreeType::Pt6: /* compass, suit */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            assert(t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
            return hs[t->tr_int1].hs_counts[idxTop3][t->tr_int2];
        case TreeType::Pt7: /* compass, suit */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            assert(t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
            return hs[t->tr_int1].hs_counts[idxTop4][t->tr_int2];
        case TreeType::Pt8: /* compass, suit */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            assert(t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
            return hs[t->tr_int1].hs_counts[idxTop5][t->tr_int2];
        case TreeType::Pt9: /* compass, suit */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            assert(t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
            return hs[t->tr_int1].hs_counts[idxC13][t->tr_int2];
        case TreeType::Shape: /* compass, shapemask */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            return (hs[t->tr_int1].hs_bits & t->tr_int2) != 0;
        case TreeType::HasCard: /* compass, card */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
#ifdef FRANCOIS
            return hascard(curdeal, t->tr_int1, (Card)t->tr_int2, vectordeal);
#else
            return hascard(curdeal, t->tr_int1, (Card)t->tr_int2);
#endif                             /* FRANCOIS */
        case TreeType::LoserTotal: /* compass */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            return hs[t->tr_int1].hs_totalloser;
        case TreeType::Loser: /* compass, suit */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            assert(t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
            return hs[t->tr_int1].hs_loser[t->tr_int2];
        case TreeType::ControlTotal: /* compass */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            return hs[t->tr_int1].hs_totalcontrol;
        case TreeType::Control: /* compass, suit */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            assert(t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
            return hs[t->tr_int1].hs_control[t->tr_int2];
        case TreeType::Cccc:
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            return cccc(t->tr_int1);
        case TreeType::Quality:
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            assert(t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
            return quality(t->tr_int1, t->tr_int2);
        case TreeType::If:
            assert(t->tr_leaf2->tr_type == TreeType::ThenElse);
            return (evaltree(t->tr_leaf1) ? evaltree(t->tr_leaf2->tr_leaf1) : evaltree(t->tr_leaf2->tr_leaf2));
        case TreeType::Tricks: /* compass, suit */
            assert(t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
            assert(t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= 1 + SUIT_SPADE);
            return dd(curdeal, t->tr_int1, t->tr_int2);
        case TreeType::Score: /* vul/non_vul, contract, tricks in leaf1 */
            assert(t->tr_int1 >= NON_VUL && t->tr_int1 <= VUL);
            return score(t->tr_int1, t->tr_int2 % 5, t->tr_int2 / 5, evaltree(t->tr_leaf1));
        case TreeType::Imps:
            return imps(evaltree(t->tr_leaf1));
        case TreeType::Rnd:
            return (int)(((double)evaltree(t->tr_leaf1)) * RANDOM() / (RAND_MAX + 1.0));
    }
}

/* This is a macro to replace the original code :
int interesting () {
  return evaltree (decisiontree);
}
*/
#define interesting() ((int)evaltree(decisiontree))

static void setup_action() {
    Action *acp;

    /* Initialize all actions */
    for (acp = actionlist; acp != 0; acp = acp->ac_next) {
        switch (acp->ac_type) {
            default:
                assert(0); /*NOTREACHED */
            case ActionType::EvalContract:
                initevalcontract();
                break;
            case ActionType::PrintCompact:
            case ActionType::PrintPBN:
            case ActionType::PrintEW:
            case ActionType::PrintAll:
            case ActionType::PrintOneLine:
            case ActionType::PrintES:
                break;
            case ActionType::Print:
                deallist = (Deal *)mycalloc(maxproduce, sizeof(Deal));
                break;
            case ActionType::Average:
                break;
            case ActionType::Frequency:
                acp->ac_u.acu_f.acuf_entries = 0;
                acp->ac_u.acu_f.acuf_freqs =
                    (long *)mycalloc(acp->ac_u.acu_f.acuf_highbnd - acp->ac_u.acu_f.acuf_lowbnd + 1, sizeof(long));
                break;
            case ActionType::Frequency2d:
                acp->ac_u.acu_f2d.acuf_freqs = (long *)mycalloc(
                    (acp->ac_u.acu_f2d.acuf_highbnd_expr1 - acp->ac_u.acu_f2d.acuf_lowbnd_expr1 + 3) *
                        (acp->ac_u.acu_f2d.acuf_highbnd_expr2 - acp->ac_u.acu_f2d.acuf_lowbnd_expr2 + 3),
                    sizeof(long));
                break;
        }
    }
}

static void action() {
    Action *acp;
    int expr, expr2, val1, val2, high1 = 0, high2 = 0, low1 = 0, low2 = 0;

    for (acp = actionlist; acp != 0; acp = acp->ac_next) {
        switch (acp->ac_type) {
            default:
                assert(0); /*NOTREACHED */
            case ActionType::PrintCompact:
                fprintcompact(stdout, curdeal, 0);
                if (acp->ac_expr1) {
                    expr = evaltree(acp->ac_expr1);
                    printf("%d\n", expr);
                }
                break;
            case ActionType::PrintOneLine:
                fprintcompact(stdout, curdeal, 1);
                if (acp->ac_expr1) {
                    expr = evaltree(acp->ac_expr1);
                    printf("%d", expr);
                }
                printf("\n");
                break;

            case ActionType::PrintES: {
                Expr *pex = (Expr *)acp->ac_expr1;
                while (pex) {
                    if (pex->ex_tr) {
                        expr = evaltree(pex->ex_tr);
                        printf("%d", expr);
                    }
                    if (pex->ex_ch) {
                        printf("%s", pex->ex_ch);
                    }
                    pex = pex->next;
                }
            } break;
            case ActionType::PrintAll:
                printdeal(curdeal);
                break;
            case ActionType::PrintEW:
                printew(curdeal);
                break;
            case ActionType::PrintPBN:
                if (!quiet)
                    printpbn(nprod, curdeal);
                break;
            case ActionType::Print:
                memcpy(deallist[nprod], curdeal, sizeof(Deal));
                break;
            case ActionType::Average:
                acp->ac_int1 += evaltree(acp->ac_expr1);
                break;
            case ActionType::Frequency:
                expr = evaltree(acp->ac_expr1);
                if (expr < acp->ac_u.acu_f.acuf_lowbnd)
                    acp->ac_u.acu_f.acuf_uflow++;
                else if (expr > acp->ac_u.acu_f.acuf_highbnd)
                    acp->ac_u.acu_f.acuf_oflow++;
                else
                    acp->ac_u.acu_f.acuf_freqs[expr - acp->ac_u.acu_f.acuf_lowbnd]++;
                acp->ac_u.acu_f.acuf_entries++;
                break;
            case ActionType::Frequency2d:
                expr = evaltree(acp->ac_expr1);
                expr2 = evaltree(acp->ac_expr2);

                high1 = acp->ac_u.acu_f2d.acuf_highbnd_expr1;
                high2 = acp->ac_u.acu_f2d.acuf_highbnd_expr2;
                low1 = acp->ac_u.acu_f2d.acuf_lowbnd_expr1;
                low2 = acp->ac_u.acu_f2d.acuf_lowbnd_expr2;
                if (expr > high1)
                    val1 = high1 - low1 + 2;
                else {
                    val1 = expr - low1 + 1;
                    if (val1 < 0)
                        val1 = 0;
                }
                if (expr2 > high2)
                    val2 = high2 - low2 + 2;
                else {
                    val2 = expr2 - low2 + 1;
                    if (val2 < 0)
                        val2 = 0;
                }
                acp->ac_u.acu_f2d.acuf_freqs[(high2 - low2 + 3) * val1 + val2]++;
                break;
        }
    }
}

static void printhands(int boardno, Deal *dealp, int player, int nhands) {
    int i, suit, rank, cards;

    for (i = 0; i < nhands; i++)
        printf("%4d.%15c", boardno + i + 1, ' ');
    printf("\n");
    for (suit = SUIT_SPADE; suit >= SUIT_CLUB; suit--) {
        cards = 10;
        for (i = 0; i < nhands; i++) {
            while (cards < 10) {
                printf("  ");
                cards++;
            }
            cards = 0;
            for (rank = 12; rank >= 0; rank--) {
                if (HAS_CARD(dealp[i], player, make_card(suit, rank))) {
                    printf("%c ", ucrep[rank]);
                    cards++;
                }
            }
            if (cards == 0) {
                printf("- ");
                cards++;
            }
        }
        printf("\n");
    }
    printf("\n");
}

static float percent(int n, int d) {
    return 100 * n / (float)d;
}

static void cleanup_action() {
    Action *acp;
    int player, i;

    for (acp = actionlist; acp != 0; acp = acp->ac_next) {
        switch (acp->ac_type) {
            default:
                assert(0); /*NOTREACHED */
            case ActionType::PrintAll:
            case ActionType::PrintCompact:
            case ActionType::PrintPBN:
            case ActionType::PrintEW:
            case ActionType::PrintOneLine:
            case ActionType::PrintES:
                break;
            case ActionType::EvalContract:
                showevalcontract(nprod);
                break;
            case ActionType::Print:
                for (player = COMPASS_NORTH; player <= COMPASS_WEST; player++) {
                    if (!(acp->ac_int1 & (1 << player)))
                        continue;
                    printf("\n\n%s hands:\n\n\n\n", player_name[player]);
                    for (i = 0; i < nprod; i += 4)
                        printhands(i, deallist + i, player, nprod - i > 4 ? 4 : nprod - i);
                    printf("\f");
                }
                break;
            case ActionType::Average:
                if (acp->ac_str1)
                    printf("%s: ", acp->ac_str1);
                printf("%g\n", (double)acp->ac_int1 / nprod);
                break;
            case ActionType::Frequency: {
                Acuft *f = &acp->ac_u.acu_f;
                printf("Frequency %s:\n", acp->ac_str1 ? acp->ac_str1 : "");
                if (f->acuf_uflow)
                    printf("Low  \t%8ld\t%5.2f %%\n", f->acuf_uflow, percent(f->acuf_uflow, f->acuf_entries));
                for (i = f->acuf_lowbnd; i <= f->acuf_highbnd; i++)
                    printf("%5d\t%8ld\t%5.2f %%\n", i, f->acuf_freqs[i - f->acuf_lowbnd],
                           percent(f->acuf_freqs[i - f->acuf_lowbnd], f->acuf_entries));
                if (f->acuf_oflow)
                    printf("High \t%8ld\t%5.2f %%\n", f->acuf_oflow, percent(f->acuf_oflow, f->acuf_entries));
                break;
            }
            case ActionType::Frequency2d: {
                int j, n = 0, low1 = 0, high1 = 0, low2 = 0, high2 = 0, sumrow, sumtot, sumcol;
                printf("Frequency %s:\n", acp->ac_str1 ? acp->ac_str1 : "");
                high1 = acp->ac_u.acu_f2d.acuf_highbnd_expr1;
                high2 = acp->ac_u.acu_f2d.acuf_highbnd_expr2;
                low1 = acp->ac_u.acu_f2d.acuf_lowbnd_expr1;
                low2 = acp->ac_u.acu_f2d.acuf_lowbnd_expr2;
                printf("        Low");
                for (j = 1; j < (high2 - low2) + 2; j++)
                    printf(" %6d", j + low2 - 1);
                printf("   High    Sum\n");
                sumtot = 0;
                for (i = 0; i < (high1 - low1) + 3; i++) {
                    sumrow = 0;
                    if (i == 0)
                        printf("Low ");
                    else if (i == (high1 - low1 + 2))
                        printf("High");
                    else
                        printf("%4d", i + low1 - 1);
                    for (j = 0; j < (high2 - low2) + 3; j++) {
                        n = acp->ac_u.acu_f2d.acuf_freqs[(high2 - low2 + 3) * i + j];
                        sumrow += n;
                        printf(" %6d", n);
                    }
                    printf(" %6d\n", sumrow);
                    sumtot += sumrow;
                }
                printf("Sum ");
                for (j = 0; j < (high2 - low2) + 3; j++) {
                    sumcol = 0;
                    for (i = 0; i < (high1 - low1) + 3; i++)
                        sumcol += acp->ac_u.acu_f2d.acuf_freqs[(high2 - low2 + 3) * i + j];
                    printf(" %6d", sumcol);
                }
                printf(" %6d\n\n", sumtot);
            }
        }
    }
}

void printew(Deal d) {
    /* This function prints the east and west hands only (with west to the
       left of east), primarily intended for examples of auctions with 2
       players only.  HU.  */
    int suit, player, rank, cards;

    for (suit = SUIT_SPADE; suit >= SUIT_CLUB; suit--) {
        cards = 10;
        for (player = COMPASS_WEST; player >= COMPASS_EAST; player--) {
            if (player != COMPASS_SOUTH) {
                while (cards < 10) {
                    printf("  ");
                    cards++;
                }
                cards = 0;
                for (rank = 12; rank >= 0; rank--) {
                    if (HAS_CARD(d, player, make_card(suit, rank))) {
                        printf("%c ", ucrep[rank]);
                        cards++;
                    }
                }
                if (cards == 0) {
                    printf("- ");
                    cards++;
                }
            }
        }
        printf("\n");
    }
    printf("\n");
}

int main(int argc, char **argv) {
    int seed_provided = 0;
    char c;
    int errflg = 0;
    int progressmeter = 0;
    int i = 0;

    timeval tvstart, tvstop;

    bool verbose = false;

    gettimeofday(&tvstart, (void *)0);

    while ((c = mygetopt(argc, argv, "023ehuvmqp:g:s:l:V")) != -1) {
        switch (c) {
            case '0':
            case '2':
            case '3':
                swapping = c - '0';
                break;
            case 'e':
#ifdef FRANCOIS
                computing_mode = EXHAUST_MODE;
                break;
#else
                /* Break if code not included in executable */
                printf("Exhaust mode not included in this executable\n");
                return 1;
#endif /* FRANCOIS */
            case 'l':
                loading = 1;
                loadindex = atoi(optarg);
                break;
            case 'g':
                maxgenerate = atoi(optarg);
                break;
            case 'm':
                progressmeter ^= 1;
                break;
            case 'p':
                maxproduce = atoi(optarg);
                break;
            case 's':
                seed_provided = 1;
                seed = atol(optarg);
                if (seed == LONG_MIN || seed == LONG_MAX) {
                    fprintf(stderr, "Seed overflow: seed must be between %ld and %ld\n", LONG_MIN, LONG_MAX);
                    exit(-1);
                }
                break;
            case 'u':
                uppercase = 1;
                break;
            case 'v':
                verbose = true;
                break;
            case 'q':
                quiet ^= 1;
                break;
            case 'V':
                printf("Version info....\n");
                printf("$Revision: 1.24 $\n");
                printf("$Date: 2003/08/05 19:53:04 $\n");
                printf("$Author: henk $\n");
                return 1;
            case '?':
            case 'h':
                errflg = 1;
                break;
        }
    }
    if (argc - optind > 2 || errflg) {
        fprintf(stderr, "Usage: %s [-emvu] [-s seed] [-p num] [-v num] [inputfile]\n", argv[0]);
        exit(-1);
    }
    if (optind < argc && freopen(input_file = argv[optind], "r", stdin) == NULL) {
        perror(argv[optind]);
        exit(-1);
    }
    newpack(fullpack);
    /* Empty pack */
    for (i = 0; i < 52; i++)
        stacked_pack[i] = NO_CARD;
    initdistr();
    maxdealer = -1;
    maxvuln = -1;

    yyparse();

    /* The most suspect part of this program */
    if (!seed_provided) {
        (void)time(&seed);
    }
    SRANDOM(seed);

    initprogram();
    if (maxgenerate == 0)
        maxgenerate = 10000000;
    if (maxproduce == 0)
        maxproduce = ((actionlist == &defaultaction) || will_print) ? 40 : maxgenerate;

    setup_action();

    if (progressmeter)
        fprintf(stderr, "Calculating...  0%% complete\r");

    switch (computing_mode) {
        case STAT_MODE:
            setup_deal();
            for (ngen = nprod = 0; ngen < maxgenerate && nprod < maxproduce; ngen++) {
                shuffle(curdeal);
                analyze(curdeal, hs);
                if (interesting()) {
                    action();
                    nprod++;
                    if (progressmeter) {
                        if ((100 * nprod / maxproduce) > 100 * (nprod - 1) / maxproduce)
                            fprintf(stderr, "Calculating... %2d%% complete\r", 100 * nprod / maxproduce);
                    }
                }
            }
            break;
#ifdef FRANCOIS
        case EXHAUST_MODE: {
            int i, j, half, ham13, c_tsize[14], highvec, *lowvec_ptr;
            int emptyslots;

            exh_get2players();
            exh_setup_card_map();
            exh_map_cards();
            exh_precompute_analyse_tables();
            emptyslots = exh_empty_slots[0] + exh_empty_slots[1];
            /* building the T table :
               o HAM_T[ham13] will contain all the 13-bits vector that have a hamming weight 'ham13'
               o Tsize[ham13] will contain the number of such possible vectors: 13/(ham13!(13-ham13)!)
               o Tsize will be mirrored into c_tsize
             */
            i = 13;
            for (c_tsize[i] = Tsize[i] = 1, HAM_T[13] = (int *)malloc(sizeof(int)); i--;)
                HAM_T[i] = (int *)malloc(sizeof(int) * (c_tsize[i] = Tsize[i] = Tsize[i + 1] * (i + 1) / (13 - i)));
            /*
               one generate the 2^13 possible half-vectors and :
               o compute its hamming weight;
               o store it into the table T
             */
            for (half = 1 << 13; half--;) {
                for (ham13 = 0, i = 13; i--;)
                    ham13 += (half >> i) & 1;
                HAM_T[ham13][--c_tsize[ham13]] = half;
            }

            for (ham13 = 14; ham13--;)
                for (i = Tsize[ham13]; i--;) {
                    highvec = HAM_T[ham13][i];
                    if (!(((highvec << 13) ^ exh_predealt_vector) >> emptyslots)) {
                        for (lowvec_ptr = HAM_T[13 - ham13], j = Tsize[ham13]; j--; lowvec_ptr++) {
                            ngen++;
                            vectordeal = (highvec << 13) | *lowvec_ptr;
                            exh_analyze_vec(highvec, *lowvec_ptr, hs);
                            if (interesting()) {
                                /*  exh_print_vector(hs); */
                                action();
                                nprod++;
                            }
                        }
                    }
                }
        } break;
#endif /* FRANCOIS */
        default:
            fprintf(stderr, "Unrecognized computation mode...\n");
            exit(-1); /*NOTREACHED */
    }
    if (progressmeter)
        fprintf(stderr, "                                      \r");
    gettimeofday(&tvstop, (void *)0);
    cleanup_action();
    if (verbose) {
        printf("Generated %d hands\n", ngen);
        printf("Produced %d hands\n", nprod);
        printf("Initial random seed %lu\n", seed);
        printf("Time needed %8.3f sec\n",
               (tvstop.tv_sec + tvstop.tv_usec / 1000000.0 - (tvstart.tv_sec + tvstart.tv_usec / 1000000.0)));
    }
    return 0;
}
