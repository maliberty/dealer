%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tree.h"
#include "dealer.h"

static int d2n(char s[4]);

static int predeal_compass; /* global variable for predeal communication */

static int pointcount_index; /* global variable for pointcount communication */

static int shapeno;

static int biasdeal[4][4] = {{-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}};

static struct Tree *var_lookup(char *s, int mustbethere);
static struct Action *newaction(ActionType type, struct Tree *p1, char *s1, int, struct Tree *);
static struct Tree *newtree(int, struct Tree *, struct Tree *, int, int);
static struct Expr *newexpr(struct Tree *tr1, char *ch1, struct Expr *ex1);
static void bias_deal(int suit, int compass, int length);
static void predeal_holding(int compass, char *holding);
static void insertshape(char s[4], int any, int neg_shape);
static void new_var(char *s, struct Tree *t);

// from scan.cpp
extern int yylineno;
extern int yylex();
extern char *yytext;
%}


%union {
        int     y_int;
        char    *y_str;
        struct Tree *y_tree;
        struct Action *y_action;
        struct Expr   *y_expr;
        char    y_distr[4];
}

%left QUERY COLON
%left OR2
%left AND2
%left CMPEQ CMPNE
%left CMPLT CMPLE CMPGT CMPGE
%left ARPLUS ARMINUS
%left ARTIMES ARDIVIDE ARMOD
%nonassoc NOT

%token GENERATE PRODUCE HCP SHAPE ANY EXCEPT CONDITION ACTION
%token PRINT PRINTALL PRINTEW PRINTPBN PRINTCOMPACT PRINTONELINE
%token AVERAGE HASCARD FREQUENCY PREDEAL POINTCOUNT ALTCOUNT
%token CONTROL LOSER DEALER VULNERABLE
%token QUALITY CCCC
%token TRICKS NOTRUMPS NORTHSOUTH EASTWEST
%token EVALCONTRACT ALL NONE SCORE IMPS RND DISTPTS
%token PT0 PT1 PT2 PT3 PT4 PT5 PT6 PT7 PT8 PT9 PRINTES

%token <y_int> NUMBER
%token <y_str> HOLDING
%token <y_str> STRING
%token <y_str> IDENT
%token <y_int> COMPASS
%token <y_int> VULNERABLE_SIDE
%token <y_int> VULN
%token <y_int> SUIT
%token <y_int> CARD
%token <y_int> CONTRACT
%token <y_distr> DISTR DISTR_OR_NUMBER

%type <y_tree>  expr
%type <y_int> number compass printlist shlprefix any vulnerable
%type <y_distr> shape
%type <y_action> actionlist action
%type <y_expr> exprlist
%type <y_str> optstring

%start defs

%%

defs
        : /* empty */
        | defs def
        ;

def
        : GENERATE number
                { if(!maxgenerate) maxgenerate = $2; }
        | PRODUCE number
                { if(!maxproduce) maxproduce = $2; }
        | DEALER  compass
                { maxdealer = $2; }
        | VULNERABLE vulnerable
                { maxvuln = $2; }
        | PREDEAL predealargs
        | POINTCOUNT { clearpointcount(); pointcount_index=12;} pointcountargs
        | ALTCOUNT number
                { clearpointcount_alt($2); pointcount_index=12;} pointcountargs
        | CONDITION expr
                { decisiontree = $2; }
        | IDENT '=' expr
                { new_var($1, $3); }
        | ACTION actionlist
                { actionlist = $2; }
        ;

predealargs
        : predealarg
        | predealargs predealarg
        ;

predealarg
        :  COMPASS { predeal_compass = $1;} holdings
        |  SUIT '(' COMPASS ')' CMPEQ NUMBER {bias_deal($1,$3,$6);}
        ;

holdings
        : HOLDING
                { predeal_holding(predeal_compass, $1); }
        | holdings ',' HOLDING
                { predeal_holding(predeal_compass, $3); }
        ;

pointcountargs
        : /* empty */
        | number
                { pointcount(pointcount_index, $1);
                  pointcount_index--;
                }
          pointcountargs
        ;

compass
        : COMPASS
                { use_compass[$1] = 1; $$= $1; }
        ;

vulnerable
        : VULNERABLE_SIDE
                { use_vulnerable[$1] = 1; $$= $1; }
        ;

shlprefix
        : ','
                { $$ = 0; }
        | ARPLUS
                { $$ = 0; }
        | /* empty */
                { $$ = 0; }
        | ARMINUS
                { $$ = 1; }
        ;

any
        : /* empty */
                { $$ = 0; }
        | ANY
                { $$ = 1; }
        ;

/* AM990705: extra production to fix unary-minus syntax glitch */
number
        : NUMBER
        | ARMINUS NUMBER
                { $$ = - $2; }
        | DISTR_OR_NUMBER
                { $$ = d2n($1); }
        ;

shape
        : DISTR
        | DISTR_OR_NUMBER
        ;
shapelistel
        : shlprefix any shape
                { insertshape($3, $2, $1); }
        ;

shapelist
        : shapelistel
        | shapelist shapelistel
        ;

expr
        : number
                { $$ = newtree(TRT_NUMBER, NIL, NIL, $1, 0); }
        | IDENT
                { $$ = var_lookup($1, 1); }
        | SUIT '(' compass ')'
                { $$ = newtree(TRT_LENGTH, NIL, NIL, $1, $3); }
        | HCP '(' compass ')'
                { $$ = newtree(TRT_HCPTOTAL, NIL, NIL, $3, 0); }
        | HCP '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_HCP, NIL, NIL, $3, $5); }
        | DISTPTS '(' compass ')'
                { $$ = newtree(TRT_DISTPTSTOTAL, NIL, NIL, $3, 0); }
        | DISTPTS '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_DISTPTS, NIL, NIL, $3, $5); }
        | PT0 '(' compass ')'
                { $$ = newtree(TRT_PT0TOTAL, NIL, NIL, $3, 0); }
        | PT0 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT0, NIL, NIL, $3, $5); }
        | PT1 '(' compass ')'
                { $$ = newtree(TRT_PT1TOTAL, NIL, NIL, $3, 0); }
        | PT1 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT1, NIL, NIL, $3, $5); }
        | PT2 '(' compass ')'
                { $$ = newtree(TRT_PT2TOTAL, NIL, NIL, $3, 0); }
        | PT2 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT2, NIL, NIL, $3, $5); }
        | PT3 '(' compass ')'
                { $$ = newtree(TRT_PT3TOTAL, NIL, NIL, $3, 0); }
        | PT3 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT3, NIL, NIL, $3, $5); }
        | PT4 '(' compass ')'
                { $$ = newtree(TRT_PT4TOTAL, NIL, NIL, $3, 0); }
        | PT4 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT4, NIL, NIL, $3, $5); }
        | PT5 '(' compass ')'
                { $$ = newtree(TRT_PT5TOTAL, NIL, NIL, $3, 0); }
        | PT5 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT5, NIL, NIL, $3, $5); }
        | PT6 '(' compass ')'
                { $$ = newtree(TRT_PT6TOTAL, NIL, NIL, $3, 0); }
        | PT6 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT6, NIL, NIL, $3, $5); }
        | PT7 '(' compass ')'
                { $$ = newtree(TRT_PT7TOTAL, NIL, NIL, $3, 0); }
        | PT7 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT7, NIL, NIL, $3, $5); }
        | PT8 '(' compass ')'
                { $$ = newtree(TRT_PT8TOTAL, NIL, NIL, $3, 0); }
        | PT8 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT8, NIL, NIL, $3, $5); }
        | PT9 '(' compass ')'
                { $$ = newtree(TRT_PT9TOTAL, NIL, NIL, $3, 0); }
        | PT9 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT9, NIL, NIL, $3, $5); }
        | LOSER '(' compass ')'
                { $$ = newtree(TRT_LOSERTOTAL, NIL, NIL, $3, 0); }
        | LOSER '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_LOSER, NIL, NIL, $3, $5); }
        | CONTROL '(' compass ')'
                { $$ = newtree(TRT_CONTROLTOTAL, NIL, NIL, $3, 0); }
        | CONTROL '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_CONTROL, NIL, NIL, $3, $5); }
        | CCCC '(' compass ')'
                { $$ = newtree(TRT_CCCC, NIL, NIL, $3, 0); }
        | QUALITY '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_QUALITY, NIL, NIL, $3, $5); }
        | SHAPE '(' compass ',' shapelist ')'
                {
          $$ = newtree(TRT_SHAPE, NIL, NIL, $3, 1<<(shapeno++));
          if (shapeno >= 32) {
            yyerror("Too many shapes -- only 32 allowed!\n");
            YYERROR;
          }
        }
        | HASCARD '(' COMPASS ',' CARD ')'
                { $$ = newtree(TRT_HASCARD, NIL, NIL, $3, $5); }
        | TRICKS '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_TRICKS, NIL, NIL, $3, $5); }
        | TRICKS '(' compass ',' NOTRUMPS ')'
                { $$ = newtree(TRT_TRICKS, NIL, NIL, $3, 4); }
        | SCORE '(' VULN ',' CONTRACT ',' expr ')'
                { $$ = newtree(TRT_SCORE, $7, NIL, $3, $5); }
        | IMPS '(' expr ')'
                { $$ = newtree(TRT_IMPS, $3, NIL, 0, 0); }
        | '(' expr ')'
                { $$ = $2; }
        | expr CMPEQ expr
                { $$ = newtree(TRT_CMPEQ, $1, $3, 0, 0); }
        | expr CMPNE expr
                { $$ = newtree(TRT_CMPNE, $1, $3, 0, 0); }
        | expr CMPLT expr
                { $$ = newtree(TRT_CMPLT, $1, $3, 0, 0); }
        | expr CMPLE expr
                { $$ = newtree(TRT_CMPLE, $1, $3, 0, 0); }
        | expr CMPGT expr
                { $$ = newtree(TRT_CMPGT, $1, $3, 0, 0); }
        | expr CMPGE expr
                { $$ = newtree(TRT_CMPGE, $1, $3, 0, 0); }
        | expr AND2 expr
                { $$ = newtree(TRT_AND2, $1, $3, 0, 0); }
        | expr OR2 expr
                { $$ = newtree(TRT_OR2, $1, $3, 0, 0); }
        | expr ARPLUS expr
                { $$ = newtree(TRT_ARPLUS, $1, $3, 0, 0); }
        | expr ARMINUS expr
                { $$ = newtree(TRT_ARMINUS, $1, $3, 0, 0); }
        | expr ARTIMES expr
                { $$ = newtree(TRT_ARTIMES, $1, $3, 0, 0); }
        | expr ARDIVIDE expr
                { $$ = newtree(TRT_ARDIVIDE, $1, $3, 0, 0); }
        | expr ARMOD expr
                { $$ = newtree(TRT_ARMOD, $1, $3, 0, 0); }
        | expr QUERY expr COLON expr
                { $$ = newtree(TRT_IF, $1, newtree(TRT_THENELSE, $3, $5, 0, 0), 0, 0); }
        | NOT expr
                { $$ = newtree(TRT_NOT, $2, NIL, 0, 0); }
        | RND '(' expr ')'
                { $$ = newtree(TRT_RND, $3, NIL, 0, 0); }
        ;

exprlist
        : expr
                { $$ = newexpr($1, 0, 0); }
        | STRING
                { $$ = newexpr(0, $1, 0); }
        | exprlist ',' expr
                { $$ = newexpr($3, 0, $1); }
        | exprlist ',' STRING
                { $$ = newexpr(0, $3, $1); }
        ;

actionlist
        : action
                { $$ = $1; }
        | action ',' actionlist
                { $$ = $1; $$->ac_next = $3; }
        | /* empty */
                { $$ = 0; }
        ;
action
        : PRINTALL
                { will_print++;
                  $$ = newaction(ActionType::PrintAll, NIL, (char *) 0, 0, NIL);
                }
        | PRINTEW
                { will_print++;
                  $$ = newaction(ActionType::PrintEW, NIL, (char *) 0, 0, NIL);
                }
        | PRINT '(' printlist ')'
                { will_print++;
                  $$ = newaction(ActionType::Print, NIL, (char *) 0, $3, NIL);
                }
        | PRINTCOMPACT
                { will_print++;
                  $$=newaction(ActionType::PrintCompact,NIL,0,0, NIL);}
        | PRINTONELINE
                { will_print++;
                  $$ = newaction(ActionType::PrintOneLine, NIL, 0, 0, NIL);}
        | PRINTPBN
                { will_print++;
                  $$=newaction(ActionType::PrintPBN,NIL,0,0, NIL);}
        | PRINTES '(' exprlist ')'
                { will_print++;
                  $$=newaction(ActionType::PrintES,(struct Tree*)$3,0,0, NIL); }
        | EVALCONTRACT  /* should allow user to specify vuln, suit, decl */
                { will_print++;
                  $$=newaction(ActionType::EvalContract,0,0,0, NIL);}
        | PRINTCOMPACT '(' expr ')'
                { will_print++;
                  $$=newaction(ActionType::PrintCompact,$3,0,0, NIL);}
        | PRINTONELINE '(' expr ')'
                { will_print++;
                  $$=newaction(ActionType::PrintOneLine,$3,0,0, NIL);}
        | AVERAGE optstring expr
                { $$ = newaction(ActionType::Average, $3, $2, 0, NIL); }
        | FREQUENCY optstring '(' expr ',' number ',' number ')'
                { $$ = newaction(ActionType::Frequency, $4, $2, 0, NIL);
                  $$->ac_u.acu_f.acuf_lowbnd = $6;
                  $$->ac_u.acu_f.acuf_highbnd = $8;}
        | FREQUENCY optstring
           '(' expr ',' number ',' number ',' expr ',' number ',' number ')' {
             $$ = newaction(ActionType::Frequency2d, $4, $2, 0, $10);
             $$->ac_u.acu_f2d.acuf_lowbnd_expr1 = $6;
             $$->ac_u.acu_f2d.acuf_highbnd_expr1 = $8;
             $$->ac_u.acu_f2d.acuf_lowbnd_expr2 = $12;
             $$->ac_u.acu_f2d.acuf_highbnd_expr2 = $14;
                }
        ;
optstring
        : /* empty */
                { $$ = (char *) 0; }
        | STRING
                { $$ = $1; }
        ;
printlist
        : COMPASS
                { $$ = (1<<$1); }
        | printlist ',' COMPASS
                { $$ = $1|(1<<$3); }
        ;
%%

static struct var {
    struct var *v_next;
    char *v_ident;
    struct Tree *v_tree;
} *vars = 0;

static struct Tree *var_lookup(char *s, int mustbethere) {
    struct var *v;

    for (v = vars; v != 0; v = v->v_next)
        if (strcmp(s, v->v_ident) == 0)
            return v->v_tree;
    if (mustbethere)
        yyerror("unknown variable");
    return 0;
}

static void new_var(char *s, struct Tree *t) {
    struct var *v;
    /* char *mycalloc(); */

    if (var_lookup(s, 0) != 0)
        yyerror("redefined variable");
    v = (struct var *)mycalloc(1, sizeof(*v));
    v->v_next = vars;
    v->v_ident = s;
    v->v_tree = t;
    vars = v;
}

void yyerror(const char *s) {
    fprintf(stderr, "line %d: %s at %s\n", yylineno, s, yytext);
    exit(-1);
}

static int perm[24][4] = {
        {       0,      1,      2,      3       },
        {       0,      1,      3,      2       },
        {       0,      2,      1,      3       },
        {       0,      2,      3,      1       },
        {       0,      3,      1,      2       },
        {       0,      3,      2,      1       },
        {       1,      0,      2,      3       },
        {       1,      0,      3,      2       },
        {       1,      2,      0,      3       },
        {       1,      2,      3,      0       },
        {       1,      3,      0,      2       },
        {       1,      3,      2,      0       },
        {       2,      0,      1,      3       },
        {       2,      0,      3,      1       },
        {       2,      1,      0,      3       },
        {       2,      1,      3,      0       },
        {       2,      3,      0,      1       },
        {       2,      3,      1,      0       },
        {       3,      0,      1,      2       },
        {       3,      0,      2,      1       },
        {       3,      1,      0,      2       },
        {       3,      1,      2,      0       },
        {       3,      2,      0,      1       },
        {       3,      2,      1,      0       },
};

static void insertshape(char s[4], int any, int neg_shape) {
    int i, j, p;
    int xcount = 0, ccount = 0;
    char copy_s[4];

    for (i = 0; i < 4; i++) {
        if (s[i] == 'x')
            xcount++;
        else
            ccount += s[i] - '0';
    }
    switch (xcount) {
        case 0:
            if (ccount != 13)
                yyerror("wrong number of cards in shape");
            for (p = 0; p < (any ? 24 : 1); p++)
                setshapebit(s[perm[p][3]] - '0', s[perm[p][2]] - '0', s[perm[p][1]] - '0',
                            s[perm[p][0]] - '0', 1 << shapeno, neg_shape);
            break;
        default:
            if (ccount > 13)
                yyerror("too many cards in ambiguous shape");
            bcopy(s, copy_s, 4);
            for (i = 0; copy_s[i] != 'x'; i++)
                ;
            if (xcount == 1) {
                copy_s[i] = 13 - ccount + '0'; /* could go above '9' */
                insertshape(copy_s, any, neg_shape);
            } else {
                for (j = 0; j <= 13 - ccount; j++) {
                    copy_s[i] = j + '0';
                    insertshape(copy_s, any, neg_shape);
                }
            }
            break;
    }
}

static int d2n(char s[4]) {
    static char copys[5];

    strncpy(copys, s, 4);
    return atoi(copys);
}

static struct Tree *newtree(int type, struct Tree *p1, struct Tree *p2, int i1, int i2) {
    /* char *mycalloc(); */
    struct Tree *p;

    p = (struct Tree *)mycalloc(1, sizeof(*p));
    p->tr_type = type;
    p->tr_leaf1 = p1;
    p->tr_leaf2 = p2;
    p->tr_int1 = i1;
    p->tr_int2 = i2;
    return p;
}

static struct Action *newaction(ActionType type, struct Tree *p1, char *s1, int i1, struct Tree *p2) {
    /* char *mycalloc(); */
    struct Action *a;

    a = (struct Action *)mycalloc(1, sizeof(*a));
    a->ac_type = type;
    a->ac_expr1 = p1;
    a->ac_str1 = s1;
    a->ac_int1 = i1;
    a->ac_expr2 = p2;
    return a;
}

static struct Expr *newexpr(struct Tree *tr1, char *ch1, struct Expr *ex1) {
    struct Expr *e;
    e = (struct Expr *)mycalloc(1, sizeof(*e));
    e->ex_tr = tr1;
    e->ex_ch = ch1;
    e->next = 0;
    if (ex1) {
        struct Expr *exau = ex1;
        /* AM990705: the while's body had mysteriously disappeared, reinserted it */
        while (exau->next)
            exau = exau->next;
        exau->next = e;
        return ex1;
    } else {
        return e;
    }
}

static void predeal_holding(int compass, char *holding) {
    char suit;

    suit = *holding++;
    while (*holding) {
        predeal(compass, make_card(*holding, suit));
        holding++;
    }
}

#define TRUNCZ(x) ((x) < 0 ? 0 : (x))

static const char *suit_name[] = {"Club", "Diamond", "Heart", "Spade"};

static int bias_len(int compass) {
    return TRUNCZ(biasdeal[compass][0]) + TRUNCZ(biasdeal[compass][1])
        + TRUNCZ(biasdeal[compass][2]) + TRUNCZ(biasdeal[compass][3]);
}

static int bias_totsuit(int suit) {
    return TRUNCZ(biasdeal[0][suit]) + TRUNCZ(biasdeal[1][suit])
        + TRUNCZ(biasdeal[2][suit]) + TRUNCZ(biasdeal[3][suit]);
}

static void bias_deal(int suit, int compass, int length) {
    if (biasdeal[compass][suit] != -1) {
        char s[256];
        sprintf(s, "%s's %s suit has length already set to %d", player_name[compass],
                suit_name[suit], biasdeal[compass][suit]);
        yyerror(s);
    }
    biasdeal[compass][suit] = length;
    if (bias_len(compass) > 13) {
        char s[256];
        sprintf(s, "Suit lengths too long for %s", player_name[compass]);
        yyerror(s);
    }
    if (bias_totsuit(suit) > 13) {
        char s[256];
        sprintf(s, "Too many %ss", suit_name[suit]);
        yyerror(s);
    }
}
