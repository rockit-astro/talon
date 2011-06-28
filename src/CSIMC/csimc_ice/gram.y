/* yacc grammar for CSI MC */

%{

#include "sa.h"

/* handy macros to compile various types at PC */
#define c_t(v,t)        do {*(t*)PC = (t)(v); PC += sizeof(t); } while (0)
#define c_o(o)          c_t(o,Opcode)   /* compile an opcode */
#define c_r(r)          c_t(r,VRef)     /* compile a variable reference */
#define c_c(c)          c_t(c,VType)    /* compile a constant value */
#define c_b(b)          c_t(b,BrType)   /* compile a branch offset */
#define c_p(p)          c_t(p,CPType)   /* compile a pointer */
#define c_B(B)          c_t(B,Byte)   	/* compile a Byte */

/* do/while/for all compile with same preamble code arrangement:
 *    br   A    jump over preamble
 *    br   B	"break" branch
 *    br   C    "continue" branch
 * A: 		<-- this addr is pushed, followed by PRFL
 *
 * The preamble allows compiling a "break" or "continue" to a known location
 * before knowing the size of the test and body of the loop. The reason for
 * the PRFL is to find this preamble among possible if/else statements within
 * the loop body.
 * 
 * Assuming pb is the stack address of the PRFL, ie, the preamble base:
 *    A is at pb[1]
 *    offset portion of B branch instruction is at pb[1]-2*BRISZ+OPSZ
 *    offset portion of C branch instruction is at pb[1]-BRISZ+OPSZ
 */

#define	PRFL	0x1d2c3b4a		/* flag to mark base of preamble */
#define	OPSZ	(sizeof(Opcode))	/* bytes in op code */
#define	BRSZ	(sizeof(BrType))	/* bytes in branch */
#define	BRP(a)	((BrType*)a)		/* addr as a BrType pointer */
#define	BRISZ	(OPSZ+BRSZ)		/* bytes in total branch instruction */
#define	BRKOP	((CPType)pb[1]-2*BRISZ)	/* addr of Break BR opcode */
#define	BRKBR	(BRKOP+OPSZ)		/* addr of Break BR branch */
#define	BRKBP	BRP(BRKBR)		/* " as a BrType pointer */
#define	CONOP	((CPType)pb[1]-BRISZ)	/* addr of Continue BR opcode */
#define	CONBR	(CONOP+OPSZ)		/* addr of Continue BR branch */
#define	CONBP	BRP(CONBR)		/* " as a BrType pointer */
#define	NEWOP	((CPType)pb[1])		/* addr of first instr */

/* compile the preamble code for a loop construct, as well as push info on
 * the stack to allow filling it in later as things are learned.
 * (we can use the same stack since this is compile time, not runtime)
 */
static void
pushPre(void)
{
	c_o (OC_BR);		/* compile branch around preamble */
	c_b (BRSZ+2*BRISZ);	/*   skip over us and two OC_BR instructions */
	c_o (OC_BR);		/* compile branch to "break" addr */
	c_b (0);		/*   just leave room for offset now */
	c_o (OC_BR);		/* compile branch to "continue" addr */
	c_b (0);		/*   just leave room for offset now */
	PUSH(PC);		/* push addr of first useful code of loop */
	PUSH(PRFL);		/* push preamble marker */
}

/* search back up stack to find closest-enclosing preamble.
 * return pointer to PRFL or NULL
 */
static VType *
findPre(void)
{
	VType *pb = SP;

	while (*pb != PRFL)
	    if (++pb == &STACK(NSTACK))
		return (NULL);

#ifdef TRPREAMBLE
	printf ("LOOP starts at %d\n", NEWOP);
	printf ("Preamble jumps %d\n", pb[1]-(BRSZ+OPSZ+BRSZ+OPSZ+BRSZ)));
	printf ("Break    jumps %d\n", BRKBR);
	printf ("Continue jumps %d\n", CONBR);
#endif /* TRPREAMBLE */

	return (pb);
}

/* pop the current preamble */
static void
popPre(void)
{
	POP();
	POP();
}

/* called to update pti.yyp.ntmpv with the largest stack location used in a
 * function so far during function definition. it ends up holding the number
 * of stack locations the function will need when it is invoked to hold the
 * largest local variable used by the function.
 * N.B. the VRef IDX for local vars is i+2, where i is used as $i. see var.c.
 */
static void
maxTmpv (VRef r)
{
	int class = (r >> VR_CLASSSHIFT) & VR_CLASSMASK;

	if (class == VC_FTMP) {
	    int n = ((r >> VR_IDXSHIFT) & VR_IDXMASK)-1;
	    if (n > pti.yyp.ntmpv)
		pti.yyp.ntmpv = n;
	}
}

/* compile a function call, called with nfa args.
 * return 0 if ok, else -1
 */
static int
compileCall(char *name, int nfa)
{
	UFSTblE *up = findUFunc (name);
	IFSTblE *ip;

	/* try user funcs first */
	if (up) {
	    if (up->nargs != nfa) {
		printf ("%s() expects %d arg%s, not %d\n", name, up->nargs,
					    up->nargs==1?"":"s", nfa);
		return (-1);
	    }
	    c_o(OC_PUSHC);
	    c_c(nfa);
	    c_o(OC_UCALL);
	    c_p(up->code);
	    c_B(up->ntmpv);
	    return (0);
	}

	/* then built-in funcs */
	ip = findIFunc (name);
	if (ip) {
	    if (nfa < (int)ip->minnargs || nfa > ip->maxnargs) {
		if (ip->minnargs == ip->maxnargs)
		    printf ("%s() expects %d arg%s, not %d\n", name,
			    ip->minnargs, ip->minnargs==1?"":"s", nfa);
		else
		    printf ("%s() expects at least %d args\n", name,
								ip->minnargs);
		return (-1);
	    }
	    c_o(OC_PUSHC);
	    c_c(nfa);
	    c_o(OC_ICALL);
	    c_p(ip->addr);

	    /* if printf, compile the format string in-line next */
	    if (!strcmp (name,"printf")) {
		int i, l;
		if (!pti.yyp.bigstring) {
		    printf ("%s() missing format\n", name);
		    return (-1);
		}
		l = strlen(pti.yyp.bigstring)+1; /* 0 too */
		for (i = 0; i < l; i++)
		    c_B(pti.yyp.bigstring[i]);
		free (pti.yyp.bigstring);
		pti.yyp.bigstring = NULL;
		pti.yyp.nbigstring = 0;
	    }
	    return (0);
	}

	/* neither */
	printf ("Undef func: %s\n", name);
	return (-1);
}

/* create a new user function.
 * the new code is already compiled in the per-task area, and nfpn and ntmpv
 *   are set.
 * return 0 if ok else -1
 */
static int
compileFunc(char *name)
{
	UFSTblE *fp = newUFunc();
	Byte *code;
	int ncode;
	int ret = 0;

	if (!fp) {
	    printf("No mem for new func\n");
	    ret = 1;
	    goto out;
	}

	c_o (OC_URETURN);
	ncode = PC - (CPType)pti.ucode;
	code = malloc (ncode);
	if (!code) {
	    printf("No mem for new code\n");
	    ret = 1;
	    goto out;
	}
	memcpy (code, pti.ucode, ncode);
	strncpy (fp->name, name, NNAME);
	fp->code = code;
	fp->size = ncode;
	fp->nargs = pti.yyp.nfpn;
	fp->ntmpv = pti.yyp.ntmpv;

    out:
	initPC();
	c_o (OC_HALT);

	return (0);
}

%}

%token	ASSIGN
%token	ADDOP	SUBOP	MULTOP	DIVOP	MODOP	ANDOP	XOROP	OROP LSHOP RSHOP
%token	CHIF	CHELSE
%token	LAND	LOR
%token	AND	XOR	OR
%token	EQ	NE
%token	LT	LE	GT	GE
%token  LSHIFT	RSHIFT
%token	ADD	SUB
%token	MULT	DIV	MOD
%token  NOT	COMP	INC	DEC	UPLUS	UMINUS
%token  NUMBER	NAME	STRING
%token	SEMI
%token	LPAREN	RPAREN

%token	BLKSRT	BLKEND
%token	IF	ELSE
%token	DO	WHILE	FOR
%token	BREAK	CONT	RETURN
%token	DEFINE	UNDEF	COMMA

%right	ASSIGN
%right	ADDOP	SUBOP	MULTOP	DIVOP	MODOP	ANDOP	XOROP	OROP LSHOP RSHOP
%right	CHIF	CHELSE
%left	LAND	LOR
%left	AND	XOR	OR
%left	EQ	NE
%left	LT	LE	GT	GE
%left	LSHIFT	RSHIFT
%left	ADD	SUB
%left	MULT	DIV	MOD
%right  NOT	COMP	INC	DEC	UPLUS	UMINUS

%start Input

%%

Input:	  /* compile one statement or new func then return */
	  ImmediateStatement 		{ return (0); }
        | FunctionDefinition 		{ return (0); }
	| FunctionUndefine		{ return (0); }
        ;

ImmediateStatement:	/* compile to be executed now */
	  Statement  			{ c_o(OC_HALT); }
	;

FunctionDefinition:	/* build a function and save if good */
	  DEFINE NAME LPAREN		{ freeUFunc ($2.s);
					  pti.yyp.fpn = (NameSpc*) calloc
						    (NFUNP, sizeof(NameSpc));
					  pti.yyp.nfpn = 0;
					}
	  Params RPAREN			{ initPC(); pti.yyp.ntmpv=0; }
	  CompoundStatement		{ if (compileFunc ($2.s) < 0)
	  				    return (1);
					}
	;

Params:   /* list of function's params, or nothing */
	  /* empty */
	| ParamList
	;

ParamList:  /* find the formal parameters, left to right */
	  Param
	| ParamList COMMA Param
	;

Param:    /* save function param in next location in pti.yyp.fpn[] */
	NAME				{ if (pti.yyp.nfpn == NFUNP) {
					    printf ("Max %d params\n", NFUNP);
					    return (1);
					  } else if ($1.s[0] == '$'
							&& !isdigit($1.s[1]))
					    strncpy(pti.yyp.fpn[pti.yyp.nfpn++],
					    			$1.s, NNAME);
					  else {
					    printf ("Illeg Param: %s\n", $1.s);
					    return (1);
					  }
					}
	;

FunctionUndefine: /* undefine the named function */
	UNDEF NAME EOS			{ c_o(OC_HALT); /* just mark empty */
					  if (freeUFunc ($2.s) < 0) {
					    printf ("No func: %s\n", $2.s);
					    return (1);
					  }
					}
	;

Statement: /* each kind of statement.. very much like C's */
	  ExpP EOS
	| DoStart Statement WHILE DoTest LPAREN Exp RPAREN DWEnd EOS
	| WHILE WUStart LPAREN Exp RPAREN WhileTest Statement WUEnd
	| FOR LPAREN ExpPOp For1 ExpOp For2 ExpPOp For3 RPAREN Statement ForEnd
	| IF LPAREN Exp RPAREN IfTest Statement IfEnd
	| IF LPAREN Exp RPAREN IfTest Statement ELSE IfElse Statement IfEnd
	| CompoundStatement
	| Return
	| Break
	| Continue
	| SimplePrint
	| EOS
        ;

EOS:
	  SEMI
	;

StatementList:
	  Statement
	| StatementList Statement
	;

CompoundStatement:
	  BLKSRT BLKEND
	| BLKSRT StatementList BLKEND
	;

Return:	  /* return: return optional expresion, then OC_URETURN */
	  RETURN ReturnVal { c_o (OC_URETURN); } EOS
	;

ReturnVal:  /* compile optional return value expression */
	  /* empty */
	| Exp
	;

Break: /* search back up stack for first preamble, compile jump to its break */
	  BREAK EOS			{ VType *pb = findPre();
					  if (!pb) {
					    printf ("`break' but no loop\n");
					    return (1);
					  }
					  c_o (OC_BR);
					  c_b (BRKOP - PC);
					}
	;

Continue: /* srch back up stack for first preamble, compile jump to its cont. */
	  CONT EOS			{ VType *pb = findPre();
					  if (!pb) {
					    printf ("`continue' but no loop\n");
					    return (1);
					  }
					  c_o (OC_BR);
					  c_b (CONOP - PC);
					}
	;

SimplePrint:	  /* =expression shortcut */
	  ASSIGN Exp			{ IFSTblE *ip = findIFunc("printf");
					  c_o(OC_PUSHC);
					  c_c(1);
					  c_o(OC_ICALL);
					  c_p(ip->addr);
					  c_B('%'); c_B('d'); c_B('\n'); c_B(0);
					  c_o(OC_POPS);
					}
	  EOS
	;

DoStart:  /* put preamble on stack to support new do-loop */
	  DO 				{ pushPre(); }
	;

DoTest:	  /* do's continue comes here for test */
	  				{ VType *pb = findPre();
	  				  *CONBP = PC-CONBR;
					}
	;

DWEnd:	  /* loop if true, else end and we know break comes here */
					{ VType *pb = findPre();
					  c_o (OC_BRT);
					  c_b (NEWOP - PC);
					  *BRKBP = PC-BRKBR;
					  popPre();
					}

WUStart:  /* put loop preamble on stack to support new while or until loop.
	   * `continue' comes here (the loop's test) too.
	   */
	  				{ VType *pb;
	  				  pushPre();
					  pb = findPre();
	  				  *CONBP = PC-CONBR;
					}
	;

WhileTest: /* if while's test fails, same as break */
	  				{ VType *pb = findPre();
					  c_o (OC_BRF);
					  c_b (BRKOP - PC);
					}

WUEnd: /* loop to top, and now we know break comes here */
					{ VType *pb = findPre();
					  c_o (OC_BR);
                                          c_b (NEWOP - PC);
					  *BRKBP = PC-BRKBR;
					  popPre();
					}
	;

For1:	  /* put loop preamble on stack to support new for-loop */
	  EOS 				{ pushPre(); }
	;

For2:	  /* if for's test fails, same as break; else jump to body.
	   * for's continue comes here (the loop's test) too.
	   * then save start of loop iterator.
	   */
	  EOS				{ VType *pb = findPre();
	  				  CPType t;
					  c_o (OC_BRF);
					  c_b (BRKOP - PC);
	  				  c_o (OC_BR);
					  t = PC;
	  				  c_b (0);
	  				  *CONBP = PC-CONBR;
					  PUSH(t);
					}
	;

For3:	  /* after loop iterator go to test for repeat. then we know where
	   * for's body starts so fill in branch after test.
	   */
	  				{ VType *pb = findPre();
					  CPType t = (CPType)POP();
					  c_o (OC_BR);
					  c_b (NEWOP - PC);
					  *BRP(t) = (BrType)(PC-t);
					}
	;

ForEnd:	  /* back to iterator, same as continue */
					{ VType *pb = findPre();
					  c_o (OC_BR);
					  c_b (CONOP - PC);
					  *BRKBP = PC-BRKBR;
					  popPre();
					}
	;

IfTest:	  /* test is done.. continue if true else branch to "else" part */
					{ c_o (OC_BRF);
					  PUSH(PC);
					  c_b (0);
					}
	;

IfElse:	  /* prepare end of "true" branch around "else" part, then finish 
	   * jump here for "else" case.
	   */
	  				{ CPType t = (CPType)POP();
					  c_o (OC_BR);
					  PUSH(PC);
					  c_b (0);
					  *BRP(t) = (BrType)(PC-t);
					}
	;

IfEnd:	  /* either way stack holds whatever needs to know end of whole "if" */
					{ CPType t = (CPType)PEEK(0);
					  *BRP(t) = (BrType)(PC-t);
					  POP();
					}
	;

ExpP:	  /* use this one if you want result popped off the stack */
	  Exp				{ c_o (OC_POPS); }
	;

ExpPOp: /* optional ExpP for `for' */
	
	| ExpP
	;

ExpOp:	/* optional Exp for `for' */
					{ c_o(OC_PUSHC); /* `True' default */
					  c_c(1);
					}
	| Exp
	;

Exp:	  /* use this one if you want result left on stack */
	  Const
	| Var
        | Ref ASSIGN Exp		{ c_o(OC_ASSIGN); }
	| Ref ADDOP Exp			{ c_o(OC_ADDOP); }
	| Ref SUBOP Exp			{ c_o(OC_SUBOP); }
	| Ref MULTOP Exp		{ c_o(OC_MULTOP); }
	| Ref DIVOP Exp			{ c_o(OC_DIVOP); }
	| Ref MODOP Exp			{ c_o(OC_MODOP); }
	| Ref OROP Exp			{ c_o(OC_OROP); }
	| Ref XOROP Exp			{ c_o(OC_XOROP); }
	| Ref ANDOP Exp			{ c_o(OC_ANDOP); }
	| Ref LSHOP Exp			{ c_o(OC_LSHOP); }
	| Ref RSHOP Exp			{ c_o(OC_RSHOP); }
        | NOT Exp			{ c_o(OC_NOT); }
        | COMP Exp			{ c_o(OC_COMP); }
        | INC Ref			{ c_o(OC_PREINC); }
        | Ref INC			{ c_o(OC_POSTINC); }
        | DEC Ref			{ c_o(OC_PREDEC); }
        | Ref DEC			{ c_o(OC_POSTDEC); }
        | ADD Exp %prec UPLUS
        | SUB Exp %prec UMINUS		{ c_o(OC_UMINUS); }
        | Exp MULT Exp   		{ c_o(OC_MULT); }
        | Exp DIV Exp  			{ c_o(OC_DIV); }
        | Exp MOD Exp			{ c_o(OC_MOD); }
        | Exp ADD Exp			{ c_o(OC_ADD); }
        | Exp SUB Exp			{ c_o(OC_SUB); }
        | Exp LSHIFT Exp		{ c_o(OC_LSHIFT); }
        | Exp RSHIFT Exp		{ c_o(OC_RSHIFT); }
        | Exp LT Exp			{ c_o(OC_LT); }
        | Exp LE Exp			{ c_o(OC_LE); }
        | Exp GT Exp			{ c_o(OC_GT); }
        | Exp GE Exp			{ c_o(OC_GE); }
        | Exp EQ Exp			{ c_o(OC_EQ); }
        | Exp NE Exp			{ c_o(OC_NE); }
        | Exp AND Exp			{ c_o(OC_AND); }
        | Exp OR Exp			{ c_o(OC_OR); }
        | Exp XOR Exp			{ c_o(OC_XOR); }
        | Exp LAND Exp			{ c_o(OC_LAND); }
        | Exp LOR Exp			{ c_o(OC_LOR); }
        | Exp CHIF IfTest Exp CHELSE IfElse Exp /* IfEnd here didn't work :( */
					{ CPType t = (CPType)PEEK(0);
					  *BRP(t) = (BrType)(PC-t);
					  POP();
					}
        | LPAREN Exp RPAREN
	| FunctionCall
        ;

Const:	  /* compile opcode to push a constant */
	  NUMBER			{ c_o(OC_PUSHC);
	                                  c_c($1.v);
					}
	;

Var:	  /* compile opcode to push current value of a variable */
	  NAME				{ VRef r = vgr($1.s, pti.yyp.fpn,
	  							pti.yyp.nfpn);
	  				  if (r != BADVREF) {
					    c_o(OC_PUSHV);
					    c_r(r);
					    maxTmpv (r);
					  } else {
					    printf ("Bad var name: %s\n", $1.s);
					    return (1);
					  }
					}
	;

Ref:	  /* compile opcode to push reference to a variable */
	  NAME				{ VRef r = vgr($1.s, pti.yyp.fpn,
	  							pti.yyp.nfpn);
	  				  if (r != BADVREF) {
					    c_o(OC_PUSHR);
					    c_r(r);
					    maxTmpv (r);
					  } else {
					    printf ("Bad var name: %s\n", $1.s);
					    return (1);
					  }
					}
	;


FunctionCall: /* call a function */
	  NAME LPAREN			{ PUSH(0); /* init arg count */ }
	  Args RPAREN			{ if (compileCall ($1.s, POP()) < 0)
	  				    return (1);
					}
	;

Args:	  /* push function args, if any */
	  /* empty */
	| ArgList
	;

ArgList:  /* push each function expression */
	  Arg
	| ArgList COMMA Arg
	;

Arg:	  /* push each exp result and inc arg count; string is in bigstring */
	  Exp				{ PEEK(0)++; /* another arg */ }
	| STRING /* STRING gets compiled with opcode, not pushed on stack */
	;
%%

static void
yyerror (char s[])
{
	if (!(pti.flags & (TF_INTERRUPT|TF_WEDIE)))
	    printf ("%s\n", s);
}

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: gram.y,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
