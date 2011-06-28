/* home-brew lexical scanner */

#include "sa.h"
#include "gram.tab.h"

#define yylval  pti.yyp.yylval
#define yylen   pti.yyp.yylen
#define base    pti.yyp.base
#define state   pti.yyp.state
#define push    pti.yyp.push
#define hist    pti.yyp.hist

typedef enum
{
    INWHITE,
    INNUMBER,
    INIDENT,
    INSTRING,
    TWINOP,
    GG,
    LL,
    CCOMMENT,
    CCOMMENT2,
    EOLCOMMENT,
} State;

static int nextToken(void);

/* called by yacc parser.
 * expects to return a token from mcg.tab.h with yylval filled with value
 * or -1 if trouble.
 */
int
yylex(void)
{
    int tok;

    while ((tok = nextToken()) == 0)
        continue;
    return (tok);
}

static int
nextToken(void)
{
    int tok = 0;
    int c;

    scheduler(1);

    if (push)
    {
        c = push;
        push = 0;
    }
    else
    {
        c = getChar();
    }

    switch (state)
    {
        case INWHITE:
            switch (c)
            {
                    /* whitespace */
                case ' ':   /* FALLTHRU */
                case '\t':  /* FALLTHRU */
                case '\r':  /* FALLTHRU */
                case '\f':  /* FALLTHRU */
                case '\n':  /* FALLTHRU */
                case '\a':
                    break;

                case '#':
                    state = EOLCOMMENT;
                    break;

                    /* start of string */
                case '"':
                    if (pti.yyp.bigstring)
                        free (pti.yyp.bigstring);
                    pti.yyp.bigstring = malloc (1);
                    pti.yyp.nbigstring = 0;
                    state = INSTRING;
                    break;

                    /* trivial 1 char tokens.. just stay in INWHITE */
                case '{':
                    tok = BLKSRT;
                    break;
                case '}':
                    tok = BLKEND;
                    break;
                case ',':
                    tok = COMMA;
                    break;
                case '(':
                    tok = LPAREN;
                    break;
                case ')':
                    tok = RPAREN;
                    break;
                case '?':
                    tok = CHIF;
                    break;
                case ':':
                    tok = CHELSE;
                    break;
                case '~':
                    tok = COMP;
                    break;
                case ';':
                    tok = SEMI;
                    break;

                    /* possible >1 char tokens.. switch to TWINOP */
                case '&': /* FALLTHRU */
                case '|': /* FALLTHRU */
                case '+': /* FALLTHRU */
                case '-': /* FALLTHRU */
                case '*': /* FALLTHRU */
                case '%': /* FALLTHRU */
                case '^': /* FALLTHRU */
                case '!': /* FALLTHRU */
                case '=': /* FALLTHRU */
                case '/': /* FALLTHRU */
                case '<': /* FALLTHRU */
                case '>':
                    hist = c;
                    state = TWINOP;
                    break;

                default:
                    if (isdigit(c))
                    {
                        state = INNUMBER;
                        base = (c == '0') ? 8 : 10; /* check for 16 next */
                        yylval.v = c - '0';
                    }
                    else if (isalpha(c) || c == '_' || c == '$')
                    {
                        state = INIDENT;
                        yylen = 0;
                        yylval.s[yylen++] = c;
                    }
                    else
                        tok = -1;
                    break;
            }
            break;

        case TWINOP:
            switch (c)
            {
                case '=':
                    switch (hist)
                    {
                        case '+':
                            tok = ADDOP;
                            break;
                        case '-':
                            tok = SUBOP;
                            break;
                        case '*':
                            tok = MULTOP;
                            break;
                        case '/':
                            tok = DIVOP;
                            break;
                        case '%':
                            tok = MODOP;
                            break;
                        case '^':
                            tok = XOROP;
                            break;
                        case '|':
                            tok = OROP;
                            break;
                        case '&':
                            tok = ANDOP;
                            break;
                        case '<':
                            tok = LE;
                            break;
                        case '>':
                            tok = GE;
                            break;
                        case '!':
                            tok = NE;
                            break;
                        case '=':
                            tok = EQ;
                            break;
                        default:
                            break;
                    }

                    if (!tok)
                        tok = -1;
                    break;

                case '&':
                    if (hist == '&') tok = LAND;
                    else goto chkhist;
                    break;

                case '|':
                    if (hist == '|') tok = LOR;
                    else goto chkhist;
                    break;

                case '+':
                    if (hist == '+') tok = INC;
                    else goto chkhist;
                    break;

                case '-':
                    if (hist == '-') tok = DEC;
                    else goto chkhist;
                    break;

                case '>':
                    if (hist == '>') state = GG;
                    else goto chkhist;
                    break;

                case '<':
                    if (hist == '<') state = LL;
                    else goto chkhist;
                    break;

                case '*':
                    if (hist == '/') state = CCOMMENT;
                    else goto chkhist;
                    break;

                case '/':
                    if (hist == '/') state = EOLCOMMENT;
                    else goto chkhist;
                    break;

                default:
chkhist:
                    switch (hist)
                    {
                        case '+':
                            tok = ADD;
                            break;
                        case '-':
                            tok = SUB;
                            break;
                        case '*':
                            tok = MULT;
                            break;
                        case '/':
                            tok = DIV;
                            break;
                        case '%':
                            tok = MOD;
                            break;
                        case '^':
                            tok = XOR;
                            break;
                        case '!':
                            tok = NOT;
                            break;
                        case '=':
                            tok = ASSIGN;
                            break;
                        case '|':
                            tok = OR;
                            break;
                        case '&':
                            tok = AND;
                            break;
                        case '>':
                            tok = GT;
                            break;
                        case '<':
                            tok = LT;
                            break;
                        default:
                            break;
                    }

                    if (tok)
                        push = c;
                    else
                        tok = -1;
                    break;
            }
            break;

        case GG:
            if (c == '=')
                tok = RSHOP;
            else
            {
                tok = RSHIFT;
                push = c;
            }
            break;

        case LL:
            if (c == '=')
                tok = LSHOP;
            else
            {
                tok = LSHIFT;
                push = c;
            }
            break;

        case INNUMBER:
            if (isupper(c))
                c = tolower(c);
            if (c == 'x' && yylval.v == 0)
                base = 16;
            else if (isdigit(c))
                yylval.v = base*yylval.v + c - '0';
            else if (base == 16 && 'a' <= c && c <= 'f')
                yylval.v = 16*yylval.v + 10 + c - 'a';
            else
            {
                tok = NUMBER;
                push = c;
            }
            break;

        case INIDENT:
            if (isalnum(c) || c == '_')
            {
                if (yylen < NNAME)
                    yylval.s[yylen++] = c;
            }
            else if (c == '@')
            {
                if (yylen > NCNAME)
                    yylen = NCNAME;
                yylval.s[yylen++] = c;
            }
            else
            {
                yylval.s[yylen] = '\0';
                if (!strncmp (yylval.s, "if", NNAME))       tok = IF;
                else if (!strncmp (yylval.s, "else", NNAME))     tok = ELSE;
                else if (!strncmp (yylval.s, "while", NNAME))    tok = WHILE;
                else if (!strncmp (yylval.s, "do", NNAME))       tok = DO;
                else if (!strncmp (yylval.s, "for", NNAME))      tok = FOR;
                else if (!strncmp (yylval.s, "break", NNAME))    tok = BREAK;
                else if (!strncmp (yylval.s, "continue", NNAME)) tok = CONT;
                else if (!strncmp (yylval.s, "return", NNAME))   tok = RETURN;
                else if (!strncmp (yylval.s, "define", NNAME))   tok = DEFINE;
                else if (!strncmp (yylval.s, "undef", NNAME))    tok = UNDEF;
                else tok = NAME;

                push = c;
            }
            break;

        case INSTRING:
            /* leave everything as-is except allow embedded " as \" */
            pti.yyp.bigstring= realloc(pti.yyp.bigstring, pti.yyp.nbigstring+1);
            pti.yyp.bigstring[pti.yyp.nbigstring++] = c;
            switch (c)
            {
                case '\\':
                    break;
                case '"':
                    if (hist == '\\')
                        pti.yyp.bigstring[--pti.yyp.nbigstring-1] = c;
                    else
                    {
                        pti.yyp.bigstring[pti.yyp.nbigstring-1] = '\0';
                        tok = STRING;
                    }
                    break;
            }
            hist = c;
            break;

        case CCOMMENT:
            if (c == '*') state = CCOMMENT2;
            break;

        case CCOMMENT2:
            state = (c == '/') ? INWHITE : CCOMMENT;
            break;

        case EOLCOMMENT:
            if (c == '\n')
                state = INWHITE;
            break;

    }

    /* reset state if we decided on a token or error */
    if (tok)
        state = INWHITE;

    return (tok);
}

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: lex.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
