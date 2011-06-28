/* execute a compiled program */

#include "sa.h"

static void do_ucall(void);
static void do_icall(void);
static void do_ureturn(void);

#if defined(WANT_XTRACE) || defined(HOSTTEST)

/* print the current stack frame up through local vars.
 * do not print per-thread locals.
 */
static void
traceSt(void)
{
    VType *p, *stop;

    /* if FP same as per initFrame() skip locals */
    stop = *FP == 0 ? FP-1-NTMPV : FP-1;

    printf ("\tSP %2d:", (int)(&STACK(NSTACK)-SP));
    for (p = SP; p < stop; p++)
        printf (" %4ld", (long)*p);

    printf ("\n");
}

/* display info about current opcode.
 * N.B. PC has already advanced by 1.
 */
static void
tracePC(Opcode oc)
{
    printf ("PC %5u: ", (int)PC-1);

    switch (oc)
    {
        case OC_HALT:
            printf ("HALT");
            break;
        case OC_PUSHC:
            printf ("PUSHC");
            break;
        case OC_PUSHV:
            printf ("PUSHV");
            break;
        case OC_PUSHR:
            printf ("PUSHR");
            break;
        case OC_NOT:
            printf ("NOT");
            break;
        case OC_COMP:
            printf ("COMP");
            break;
        case OC_UMINUS:
            printf ("UMINUS");
            break;
        case OC_ASSIGN:
            printf ("ASSIGN");
            break;
        case OC_ADDOP:
            printf ("ADDOP");
            break;
        case OC_SUBOP:
            printf ("SUBOP");
            break;
        case OC_MULTOP:
            printf ("MULTOP");
            break;
        case OC_DIVOP:
            printf ("DIVOP");
            break;
        case OC_MODOP:
            printf ("MODOP");
            break;
        case OC_OROP:
            printf ("OROP");
            break;
        case OC_XOROP:
            printf ("XOROP");
            break;
        case OC_ANDOP:
            printf ("ANDOP");
            break;
        case OC_LSHOP:
            printf ("LSHOP");
            break;
        case OC_RSHOP:
            printf ("RSHOP");
            break;
        case OC_PREINC:
            printf ("PREINC");
            break;
        case OC_POSTINC:
            printf ("POSTINC");
            break;
        case OC_PREDEC:
            printf ("PREDEC");
            break;
        case OC_POSTDEC:
            printf ("POSTDEC");
            break;
        case OC_MULT:
            printf ("MULT");
            break;
        case OC_DIV:
            printf ("DIV");
            break;
        case OC_MOD:
            printf ("MOD");
            break;
        case OC_ADD:
            printf ("ADD");
            break;
        case OC_SUB:
            printf ("SUB");
            break;
        case OC_LSHIFT:
            printf ("LSHIFT");
            break;
        case OC_RSHIFT:
            printf ("RSHIFT");
            break;
        case OC_LT:
            printf ("LT");
            break;
        case OC_LE:
            printf ("LE");
            break;
        case OC_EQ:
            printf ("EQ");
            break;
        case OC_NE:
            printf ("NE");
            break;
        case OC_GE:
            printf ("GE");
            break;
        case OC_GT:
            printf ("GT");
            break;
        case OC_AND:
            printf ("AND");
            break;
        case OC_OR:
            printf ("OR");
            break;
        case OC_XOR:
            printf ("XOR");
            break;
        case OC_LAND:
            printf ("LAND");
            break;
        case OC_LOR:
            printf ("LOR");
            break;
        case OC_BRT:
            printf ("BRT");
            printf ("%4d", *(BrType*)PC);
            break;
        case OC_BRF:
            printf ("BRF");
            printf ("%4d", *(BrType*)PC);
            break;
        case OC_BR:
            printf ("BR ");
            printf ("%4d", *(BrType*)PC);
            break;
        case OC_POPS:
            printf ("POPS");
            break;
        case OC_UCALL:
            printf ("UCALL");
            break;
        case OC_URETURN:
            printf ("URETURN");
            break;
        case OC_ICALL:
            printf ("ICALL");
            break;
    }
}

#else
/* smaller versions */
static void
traceSt(void)
{
    printf (" SP=%2d: %4ld\n", (int)(&STACK(NSTACK)-SP), PEEK(0));

}

static void
tracePC(Opcode oc)
{
    printf ("PC=%5u Op=%2u", (int)PC-1, oc);
}
#endif /* WANT_XTRACE */

/* execute next opcode at pc:
 * perform the operation at pc and advance.
 * 0 to keep going, -1 when hit OC_HALT.
 */
int
execute_1 (void)
{
    Opcode oc = *(Opcode*)PC++;
    VRef r;

    if (cvg(CV_TRACE))
        tracePC (oc);

    switch (oc)
    {
        case OC_HALT:
            break;
        case OC_PUSHC:
            PUSH(*((VType*)PC));
            PC += sizeof(VType);
            break;
        case OC_PUSHV:
            PUSH(vgv(*(VRef*)PC));
            PC += sizeof(VRef);
            break;
        case OC_PUSHR:
            PUSH(*((VRef*)PC));
            PC += sizeof(VRef);
            break;

        case OC_NOT:
            PEEK(0) = !PEEK(0);
            break;
        case OC_COMP:
            PEEK(0) = ~PEEK(0);
            break;
        case OC_UMINUS:
            PEEK(0) = -PEEK(0);
            break;

        case OC_ASSIGN:
            r = *++SP;
            PEEK(0) = vsv(r,PEEK(-1));
            break;
        case OC_ADDOP:
            r = *++SP;
            PEEK(0) = vsv(r,vgv(r) +  PEEK(-1));
            break;
        case OC_SUBOP:
            r = *++SP;
            PEEK(0) = vsv(r,vgv(r) -  PEEK(-1));
            break;
        case OC_MULTOP:
            r = *++SP;
            PEEK(0) = vsv(r,vgv(r) *  PEEK(-1));
            break;
        case OC_DIVOP:
            r = *++SP;
            PEEK(0) = vsv(r,vgv(r) /  PEEK(-1));
            break;
        case OC_MODOP:
            r = *++SP;
            PEEK(0) = vsv(r,vgv(r) %  PEEK(-1));
            break;
        case OC_OROP:
            r = *++SP;
            PEEK(0) = vsv(r,vgv(r) |  PEEK(-1));
            break;
        case OC_XOROP:
            r = *++SP;
            PEEK(0) = vsv(r,vgv(r) ^  PEEK(-1));
            break;
        case OC_ANDOP:
            r = *++SP;
            PEEK(0) = vsv(r,vgv(r) &  PEEK(-1));
            break;
        case OC_LSHOP:
            r = *++SP;
            PEEK(0) = vsv(r,vgv(r) << PEEK(-1));
            break;
        case OC_RSHOP:
            r = *++SP;
            PEEK(0) = vsv(r,vgv(r) >> PEEK(-1));
            break;

        case OC_PREINC:
            r = PEEK(0);
            PEEK(0) = vsv(r, vgv(r)+1);
            break;
        case OC_PREDEC:
            r = PEEK(0);
            PEEK(0) = vsv(r, vgv(r)-1);
            break;
        case OC_POSTINC:
            r = PEEK(0);
            (void)vsv(r, (PEEK(0)=vgv(r))+1);
            break;
        case OC_POSTDEC:
            r = PEEK(0);
            (void)vsv(r, (PEEK(0)=vgv(r))-1);
            break;

        case OC_MULT:
            POP();
            PEEK(0) *=  PEEK(-1);
            break;
        case OC_DIV:
            POP();
            PEEK(0) /=  PEEK(-1);
            break;
        case OC_MOD:
            POP();
            PEEK(0) %=  PEEK(-1);
            break;
        case OC_ADD:
            POP();
            PEEK(0) +=  PEEK(-1);
            break;
        case OC_SUB:
            POP();
            PEEK(0) -=  PEEK(-1);
            break;
        case OC_LSHIFT:
            POP();
            PEEK(0) <<= PEEK(-1);
            break;
        case OC_RSHIFT:
            POP();
            PEEK(0) >>= PEEK(-1);
            break;
        case OC_AND:
            POP();
            PEEK(0) &=  PEEK(-1);
            break;
        case OC_OR:
            POP();
            PEEK(0) |=  PEEK(-1);
            break;
        case OC_XOR:
            POP();
            PEEK(0) ^=  PEEK(-1);
            break;

        case OC_LT:
            POP();
            PEEK(0) = PEEK(0) <  PEEK(-1);
            break;
        case OC_LE:
            POP();
            PEEK(0) = PEEK(0) <= PEEK(-1);
            break;
        case OC_EQ:
            POP();
            PEEK(0) = PEEK(0) == PEEK(-1);
            break;
        case OC_NE:
            POP();
            PEEK(0) = PEEK(0) != PEEK(-1);
            break;
        case OC_GE:
            POP();
            PEEK(0) = PEEK(0) >= PEEK(-1);
            break;
        case OC_GT:
            POP();
            PEEK(0) = PEEK(0) >  PEEK(-1);
            break;
        case OC_LAND:
            POP();
            PEEK(0) = PEEK(0) && PEEK(-1);
            break;
        case OC_LOR:
            POP();
            PEEK(0) = PEEK(0) || PEEK(-1);
            break;

        case OC_BRT:
            PC += POP() ? *(BrType*)PC   : sizeof(BrType);
            break;
        case OC_BRF:
            PC += POP() ? sizeof(BrType) : *(BrType*)PC;
            break;
        case OC_BR:
            PC += *(BrType*)PC;
            break;

        case OC_POPS:
            POP();
            break;

        case OC_UCALL:
            do_ucall();
            break;
        case OC_URETURN:
            do_ureturn();
            break;
        case OC_ICALL:
            do_icall();
            break;
        default:
            errFlash (13);
    }

    if (cvg(CV_TRACE))
        traceSt ();

    if (SP < pti.minsp)
        pti.minsp = SP;

    return (oc == OC_HALT ? -1 : 0);
}

/* initialize SP and FP so we appear to have NTMPV local variables.
 * also rig up a OC_HALT in case OC_URETURN is called from top level.
 */
void
initFrame(void)
{
    static Opcode halt = OC_HALT;

    SP = &STACK(NSTACK);    /* start with empty stack */
    PUSH (0);       /* push number of args in prev frame */
    PUSH (0);       /* push bogus FP */
    FP = SP;        /* set current FP */
    PUSH (&halt);       /* return points to OC_HALT */
    SP -= NTMPV;        /* push room for NTMPV local vars */
}

/* call a previously compiled user function.
 * OC_UCALL opcode is followed by 2 params:
 *   CPType: addr of start of function being called
 *   Byte:   n stack locations needed to support locals: $0, $1, etc
 *
 * params have just been pushed, left-to-right, then nparams.
 * we add current fp, return address, room for temps.
 * when jump, looks like this: (addresses decrease down the page)
 *
 *       param1
 *       param2
 *       param3
 *       3 (n params)
 * fp -> old fp
 *       return addr
 *       $0           <-\
 *        ...           |-- OC_UCALL reserves ntmps locations for $0, $1, ..
 * sp -> $i           <-/
 */
static void
do_ucall(void)
{
    CPType newpc;           /* new func addr */
    int ntmps;          /* n temps needed */

    newpc = *(CPType*)PC;       /* OC_UCALL param 1 is new addr */
    PC += sizeof(CPType);       /* advance past addr */
    ntmps = *(Byte*)PC;     /* OC_UCALL param 2 is n temp vars */
    PC += sizeof(Byte);     /* advance past var count */
    PUSH(FP-&STACK(0));     /* push current frame pointer */
    FP = SP;            /* becomes new frame */
    PUSH(PC);           /* push return address */
    SP -= ntmps;            /* push room for local temps */
    PC = newpc;         /* off we go */
}

/* return from a user function.
 * stack points to value to be returned (garbage if no return(x).. so it goes).
 * restore old frame, pop params, put return value at final sp.
 * see do_ucall() for a picture of a frame.
 */
static void
do_ureturn(void)
{
    VType retval = PEEK(0);     /* save return value */
    CPType retpc = (CPType)FP[-1];  /* save return address from this frame*/
    SP = FP + FP[1] + 1;        /* pop callers args but .. */
    PEEK(0) = retval;       /* appear to have just pushed ret val */
    FP = &STACK(*FP);       /* restore previous fp */
    PC = retpc;         /* back we go */
}

/* call a built-in function.
 * the OC_ICALL opcode is followed by CPType addr of C function to call.
 * params have just been pushed, left-to-right, then nparams.
 * SP is pointing at n params. thus:
 *
 *       param1
 *       param2
 *       param3
 * SP -> 3 (n params)
 *
 * N.B. we assume built-ins will not call user funcs so we don't bother to
 *   make a new stack frame.
 * N.B. built-ins are free to get params and make local temps using SP but
 *   when they return SP must be the same as when called and SP[0] must be
 *   unchanged.
 */
static void
do_icall(void)
{
    int retval;
    BIType fp = *(BIType*)PC;   /* get real func addr */
    PC += sizeof(CPType);       /* advance PC past addr */
    retval = (*fp)();       /* call */
    SP += PEEK(0);          /* pop callers args but .. */
    PEEK(0) = retval;       /* appear to have just pushed ret val */
}

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: exec.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
