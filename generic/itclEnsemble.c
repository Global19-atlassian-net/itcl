/*
 * ------------------------------------------------------------------------
 *      PACKAGE:  [incr Tcl]
 *  DESCRIPTION:  Object-Oriented Extensions to Tcl
 *
 *  [incr Tcl] provides object-oriented extensions to Tcl, much as
 *  C++ provides object-oriented extensions to C.  It provides a means
 *  of encapsulating related procedures together with their shared data
 *  in a local namespace that is hidden from the outside world.  It
 *  promotes code re-use through inheritance.  More than anything else,
 *  it encourages better organization of Tcl applications through the
 *  object-oriented paradigm, leading to code that is easier to
 *  understand and maintain.
 *
 *  This part handles ensembles, which support compound commands in Tcl.
 *  The usual "info" command is an ensemble with parts like "info body"
 *  and "info globals".  Extension developers can extend commands like
 *  "info" by adding their own parts to the ensemble.
 *
 * ========================================================================
 *  AUTHOR:  Michael J. McLennan
 *           Bell Labs Innovations for Lucent Technologies
 *           mmclennan@lucent.com
 *           http://www.tcltk.com/itcl
 *
 *  overhauled version author: Arnulf Wiedemann
 *
 *     RCS:  $Id: itclEnsemble.c,v 1.1.2.3 2007/09/09 13:38:40 wiede Exp $
 * ========================================================================
 *           Copyright (c) 1993-1998  Lucent Technologies, Inc.
 * ------------------------------------------------------------------------
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
#include "itclInt.h"

#define ITCL_ENSEMBLE_CUSTOM        0x01
#define ITCL_ENSEMBLE_ENSEMBLE      0x02

/*
 *  Data used to represent an ensemble:
 */
struct Ensemble;
typedef struct EnsemblePart {
    char *name;                 /* name of this part */
    Tcl_Obj *namePtr;
    int minChars;               /* chars needed to uniquely identify part */
    Tcl_Command cmdPtr;         /* command handling this part */
    char *usage;                /* usage string describing syntax */
    struct Ensemble* ensemble;  /* ensemble containing this part */
    ItclArgList *arglistPtr;    /* the parsed argument list */
    Tcl_ObjCmdProc *objProc;    /* handling procedure for part */
    ClientData *clientData;     /* the procPtr for the part */
    Tcl_CmdDeleteProc *deleteProc;
                                /* procedure used to destroy client data */
    int flags;
} EnsemblePart;

/*
 *  Data used to represent an ensemble:
 */
typedef struct Ensemble {
    Tcl_Interp *interp;         /* interpreter containing this ensemble */
    EnsemblePart **parts;       /* list of parts in this ensemble */
    int numParts;               /* number of parts in part list */
    int maxParts;               /* current size of parts list */
    int ensembleId;             /* this ensembles id */
    Tcl_Command cmd;            /* command representing this ensemble */
    EnsemblePart* parent;       /* parent part for sub-ensembles
                                 * NULL => toplevel ensemble */
    Tcl_Namespace *nsPtr;       /* namespace for ensemble part commands */
} Ensemble;

/*
 *  Data shared by ensemble access commands and ensemble parser:
 */
typedef struct EnsembleParser {
    Tcl_Interp* master;           /* master interp containing ensembles */
    Tcl_Interp* parser;           /* slave interp for parsing */
    Ensemble* ensData;            /* add parts to this ensemble */
} EnsembleParser;

static int EnsembleSubCmd(ClientData clientData, Tcl_Interp *interp,
        int objc, Tcl_Obj *CONST objv[]);
static int EnsembleUnknownCmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *CONST objv[]);

/*
 *  Forward declarations for the procedures used in this file.
 */
static void GetEnsembleUsage _ANSI_ARGS_((Tcl_Interp *interp,
    Ensemble *ensData, Tcl_Obj *objPtr));

static void GetEnsemblePartUsage _ANSI_ARGS_((Tcl_Interp *interp,
    Ensemble *ensData, EnsemblePart *ensPart, Tcl_Obj *objPtr));

static int CreateEnsemble _ANSI_ARGS_((Tcl_Interp *interp,
    Ensemble *parentEnsData, CONST char *ensName));

static int AddEnsemblePart _ANSI_ARGS_((Tcl_Interp *interp,
    Ensemble* ensData, CONST char* partName, CONST char* usageInfo,
    Tcl_ObjCmdProc *objProc, ClientData clientData,
    Tcl_CmdDeleteProc *deleteProc, int flags, EnsemblePart **rVal));

static void DeleteEnsemble _ANSI_ARGS_((ClientData clientData));

static int FindEnsemble _ANSI_ARGS_((Tcl_Interp *interp, CONST84 char **nameArgv,
    int nameArgc, Ensemble** ensDataPtr));

static int CreateEnsemblePart _ANSI_ARGS_((Tcl_Interp *interp,
    Ensemble *ensData, CONST char* partName, EnsemblePart **ensPartPtr));

static void DeleteEnsemblePart _ANSI_ARGS_((EnsemblePart *ensPart));

static int FindEnsemblePart _ANSI_ARGS_((Tcl_Interp *interp,
    Ensemble *ensData, CONST char* partName, EnsemblePart **rensPart));

static int FindEnsemblePartIndex _ANSI_ARGS_((Ensemble *ensData,
    CONST char *partName, int *posPtr));

static void ComputeMinChars _ANSI_ARGS_((Ensemble *ensData, int pos));

static EnsembleParser* GetEnsembleParser _ANSI_ARGS_((Tcl_Interp *interp));

static void DeleteEnsParser _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp* interp));

#ifdef NOTDEF
static void
DumpEnsemble(
    Tcl_Interp *interp,
    Ensemble *ensData)
{
    Ensemble *ensData2;
    int i;

    ensData2 = ensData;
    while (1) {
        fprintf(stderr, "ENS!%s!%d!\n", Tcl_GetCommandName(interp, ensData2->cmd), ensData2->numParts);
        for (i=0; i<ensData2->numParts; i++) {
            fprintf(stderr, "  %d!%s!\n", i, ensData2->parts[i]->name);
        }
        if (ensData2->parent == NULL) {
            break;
        }
        ensData2 = ensData2->parent->ensemble;
    }
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * Itcl_EnsembleInit --
 *
 *      Called when any interpreter is created to make sure that
 *      things are properly set up for ensembles.
 *
 * Results:
 *      Returns TCL_OK if successful, and TCL_ERROR if anything goes
 *      wrong.
 *
 * Side effects:
 *      On the first call, the "ensemble" object type is registered
 *      with the Tcl compiler.  If an error is encountered, an error
 *      is left as the result in the interpreter.
 *
 *----------------------------------------------------------------------
 */
	/* ARGSUSED */
int
Itcl_EnsembleInit(
    Tcl_Interp *interp)         /* interpreter being initialized */
{
    Tcl_DString buffer;
    Tcl_InterpDeleteProc *procPtr;
    ItclObjectInfo *infoPtr;

    infoPtr = Tcl_GetAssocData(interp, ITCL_INTERP_DATA, &procPtr);
    Tcl_CreateObjCommand(interp, "::itcl::ensemble",
        Itcl_EnsembleCmd, (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL);

    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, ITCL_COMMANDS_NAMESPACE, -1);
    Tcl_DStringAppend(&buffer, "::ensembles", -1);
    infoPtr->ensembleInfo->ensembleNsPtr = Tcl_CreateNamespace(interp,
            Tcl_DStringValue(&buffer), NULL, NULL);
/* FIX ME !!! */
if (infoPtr->ensembleInfo->ensembleNsPtr == NULL) {
fprintf(stderr, "error in creating namespace: %s \n", Tcl_DStringValue(&buffer));
}
    Tcl_CreateObjCommand(interp,
            ITCL_COMMANDS_NAMESPACE "::ensembles::unknown",
	    EnsembleUnknownCmd, NULL, NULL);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Itcl_CreateEnsemble --
 *
 *      Creates an ensemble command, or adds a sub-ensemble to an
 *      existing ensemble command.  The ensemble name is a space-
 *      separated list.  The first word in the list is the command
 *      name for the top-level ensemble.  Other names do not have
 *      commands associated with them; they are merely sub-ensembles
 *      within the ensemble.  So a name like "a::b::foo bar baz"
 *      represents an ensemble command called "foo" in the namespace
 *      "a::b" that has a sub-ensemble "bar", that has a sub-ensemble
 *      "baz".
 *
 *      If the name is a single word, then this procedure creates
 *      a top-level ensemble and installs an access command for it.
 *      If a command already exists with that name, it is deleted.
 *
 *      If the name has more than one word, then the leading words
 *      are treated as a path name for an existing ensemble.  The
 *      last word is treated as the name for a new sub-ensemble.
 *      If an part already exists with that name, it is an error.
 *
 * Results:
 *      Returns TCL_OK if successful, and TCL_ERROR if anything goes
 *      wrong.
 *
 * Side effects:
 *      If an error is encountered, an error is left as the result
 *      in the interpreter.
 *
 *----------------------------------------------------------------------
 */
int
Itcl_CreateEnsemble(
    Tcl_Interp *interp,            /* interpreter to be updated */
    CONST char* ensName)           /* name of the new ensemble */
{
    CONST84 char **nameArgv = NULL;
    int nameArgc;
    Ensemble *parentEnsData;
    Tcl_DString buffer;

    /*
     *  Split the ensemble name into its path components.
     */
    if (Tcl_SplitList(interp, (CONST84 char *)ensName, &nameArgc,
	    &nameArgv) != TCL_OK) {
        goto ensCreateFail;
    }
    if (nameArgc < 1) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "invalid ensemble name \"", ensName, "\"",
            (char*)NULL);
        goto ensCreateFail;
    }

    /*
     *  If there is more than one path component, then follow
     *  the path down to the last component, to find the containing
     *  ensemble.
     */
    parentEnsData = NULL;
    if (nameArgc > 1) {
        if (FindEnsemble(interp, nameArgv, nameArgc-1, &parentEnsData)
            != TCL_OK) {
            goto ensCreateFail;
        }

        if (parentEnsData == NULL) {
            char *pname = Tcl_Merge(nameArgc-1, nameArgv);
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "invalid ensemble name \"", pname, "\"",
                (char*)NULL);
            ckfree(pname);
            goto ensCreateFail;
        }
    }

    /*
     *  Create the ensemble.
     */
    if (CreateEnsemble(interp, parentEnsData, nameArgv[nameArgc-1])
        != TCL_OK) {
        goto ensCreateFail;
    }

    ckfree((char*)nameArgv);
    return TCL_OK;

ensCreateFail:
    if (nameArgv) {
        ckfree((char*)nameArgv);
    }
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, "\n    (while creating ensemble \"", -1);
    Tcl_DStringAppend(&buffer, ensName, -1);
    Tcl_DStringAppend(&buffer, "\")", -1);
    Tcl_AddObjErrorInfo(interp, Tcl_DStringValue(&buffer), -1);
    Tcl_DStringFree(&buffer);

    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * Itcl_AddEnsemblePart --
 *
 *      Adds a part to an ensemble which has been created by
 *      Itcl_CreateEnsemble.  Ensembles are addressed by name, as
 *      described in Itcl_CreateEnsemble.
 *
 *      If the ensemble already has a part with the specified name,
 *      this procedure returns an error.  Otherwise, it adds a new
 *      part to the ensemble.
 *
 *      Any client data specified is automatically passed to the
 *      handling procedure whenever the part is invoked.  It is
 *      automatically destroyed by the deleteProc when the part is
 *      deleted.
 *
 * Results:
 *      Returns TCL_OK if successful, and TCL_ERROR if anything goes
 *      wrong.
 *
 * Side effects:
 *      If an error is encountered, an error is left as the result
 *      in the interpreter.
 *
 *----------------------------------------------------------------------
 */
int
Itcl_AddEnsemblePart(
    Tcl_Interp *interp,            /* interpreter to be updated */
    CONST char* ensName,           /* ensemble containing this part */
    CONST char* partName,          /* name of the new part */
    CONST char* usageInfo,         /* usage info for argument list */
    Tcl_ObjCmdProc *objProc,       /* handling procedure for part */
    ClientData clientData,         /* client data associated with part */
    Tcl_CmdDeleteProc *deleteProc) /* procedure used to destroy client data */
{
    CONST84 char **nameArgv = NULL;
    int nameArgc;
    Ensemble *ensData;
    EnsemblePart *ensPart;
    Tcl_DString buffer;

    /*
     *  Parse the ensemble name and look for a containing ensemble.
     */
    if (Tcl_SplitList(interp, (CONST84 char *)ensName, &nameArgc,
	    &nameArgv) != TCL_OK) {
        goto ensPartFail;
    }
    if (FindEnsemble(interp, nameArgv, nameArgc, &ensData) != TCL_OK) {
        goto ensPartFail;
    }

    if (ensData == NULL) {
        char *pname = Tcl_Merge(nameArgc, nameArgv);
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "invalid ensemble name \"", pname, "\"",
            (char*)NULL);
        ckfree(pname);
        goto ensPartFail;
    }

    /*
     *  Install the new part into the part list.
     */
    if (AddEnsemblePart(interp, ensData, partName, usageInfo,
            objProc, clientData, deleteProc, ITCL_ENSEMBLE_CUSTOM,
	    &ensPart) != TCL_OK) {
        goto ensPartFail;
    }

    ckfree((char*)nameArgv);
    return TCL_OK;

ensPartFail:
    if (nameArgv) {
        ckfree((char*)nameArgv);
    }
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, "\n    (while adding to ensemble \"", -1);
    Tcl_DStringAppend(&buffer, ensName, -1);
    Tcl_DStringAppend(&buffer, "\")", -1);
    Tcl_AddObjErrorInfo(interp, Tcl_DStringValue(&buffer), -1);
    Tcl_DStringFree(&buffer);

    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * Itcl_GetEnsemblePart --
 *
 *      Looks for a part within an ensemble, and returns information
 *      about it.
 *
 * Results:
 *      If the ensemble and its part are found, this procedure
 *      loads information about the part into the "infoPtr" structure
 *      and returns 1.  Otherwise, it returns 0.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
int
Itcl_GetEnsemblePart(
    Tcl_Interp *interp,            /* interpreter to be updated */
    CONST char *ensName,           /* ensemble containing the part */
    CONST char *partName,          /* name of the desired part */
    Tcl_CmdInfo *infoPtr)          /* returns: info associated with part */
{
    CONST84 char **nameArgv = NULL;
    int nameArgc;
    Ensemble *ensData;
    EnsemblePart *ensPart;
    Itcl_InterpState state;

    /*
     *  Parse the ensemble name and look for a containing ensemble.
     *  Save the interpreter state before we do this.  If we get any
     *  errors, we don't want them to affect the interpreter.
     */
    state = Itcl_SaveInterpState(interp, TCL_OK);

    if (Tcl_SplitList(interp, (CONST84 char *)ensName, &nameArgc,
	    &nameArgv) != TCL_OK) {
        goto ensGetFail;
    }
    if (FindEnsemble(interp, nameArgv, nameArgc, &ensData) != TCL_OK) {
        goto ensGetFail;
    }
    if (ensData == NULL) {
        goto ensGetFail;
    }

    /*
     *  Look for a part with the desired name.  If found, load
     *  its data into the "infoPtr" structure.
     */
    if (FindEnsemblePart(interp, ensData, partName, &ensPart)
        != TCL_OK || ensPart == NULL) {
        goto ensGetFail;
    }

    if (Tcl_GetCommandInfoFromToken(ensPart->cmdPtr, infoPtr) != 1) {
        goto ensGetFail;
    }

    Itcl_DiscardInterpState(state);
    return 1;

ensGetFail:
    Itcl_RestoreInterpState(interp, state);
    return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * Itcl_IsEnsemble --
 *
 *      Determines whether or not an existing command is an ensemble.
 *
 * Results:
 *      Returns non-zero if the command is an ensemble, and zero
 *      otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
int
Itcl_IsEnsemble(
    Tcl_CmdInfo* infoPtr)  /* command info from Tcl_GetCommandInfo() */
{
    if (infoPtr) {
// FIX ME use CMD and Tcl_IsEnsemble!!
        return (infoPtr->deleteProc == DeleteEnsemble);
    }
    return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * Itcl_GetEnsembleUsage --
 *
 *      Returns a summary of all of the parts of an ensemble and
 *      the meaning of their arguments.  Each part is listed on
 *      a separate line.  Having this summary is sometimes useful
 *      when building error messages for the "@error" handler in
 *      an ensemble.
 *
 *      Ensembles are accessed by name, as described in
 *      Itcl_CreateEnsemble.
 *
 * Results:
 *      If the ensemble is found, its usage information is appended
 *      onto the object "objPtr", and this procedure returns
 *      non-zero.  It is the responsibility of the caller to
 *      initialize and free the object.  If anything goes wrong,
 *      this procedure returns 0.
 *
 * Side effects:
 *      Object passed in is modified.
 *
 *----------------------------------------------------------------------
 */
int
Itcl_GetEnsembleUsage(
    Tcl_Interp *interp,    /* interpreter containing the ensemble */
    CONST char *ensName,   /* name of the ensemble */
    Tcl_Obj *objPtr)       /* returns: summary of usage info */
{
    CONST84 char **nameArgv = NULL;
    int nameArgc;
    Ensemble *ensData;
    Itcl_InterpState state;

    /*
     *  Parse the ensemble name and look for the ensemble.
     *  Save the interpreter state before we do this.  If we get
     *  any errors, we don't want them to affect the interpreter.
     */
    state = Itcl_SaveInterpState(interp, TCL_OK);

    if (Tcl_SplitList(interp, (CONST84 char *)ensName, &nameArgc,
	    &nameArgv) != TCL_OK) {
        goto ensUsageFail;
    }
    if (FindEnsemble(interp, nameArgv, nameArgc, &ensData) != TCL_OK) {
        goto ensUsageFail;
    }
    if (ensData == NULL) {
        goto ensUsageFail;
    }

    /*
     *  Add a summary of usage information to the return buffer.
     */
    GetEnsembleUsage(interp, ensData, objPtr);

    Itcl_DiscardInterpState(state);
    return 1;

ensUsageFail:
    Itcl_RestoreInterpState(interp, state);
    return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * Itcl_GetEnsembleUsageForObj --
 *
 *      Returns a summary of all of the parts of an ensemble and
 *      the meaning of their arguments.  This procedure is just
 *      like Itcl_GetEnsembleUsage, but it determines the desired
 *      ensemble from a command line argument.  The argument should
 *      be the first argument on the command line--the ensemble
 *      command or one of its parts.
 *
 * Results:
 *      If the ensemble is found, its usage information is appended
 *      onto the object "objPtr", and this procedure returns
 *      non-zero.  It is the responsibility of the caller to
 *      initialize and free the object.  If anything goes wrong,
 *      this procedure returns 0.
 *
 * Side effects:
 *      Object passed in is modified.
 *
 *----------------------------------------------------------------------
 */
int
Itcl_GetEnsembleUsageForObj(
    Tcl_Interp *interp,    /* interpreter containing the ensemble */
    Tcl_Obj *ensObjPtr,    /* argument representing ensemble */
    Tcl_Obj *objPtr)       /* returns: summary of usage info */
{
    Ensemble *ensData;
    Tcl_Obj *chainObj;
    Tcl_Command cmd;
    Tcl_CmdInfo infoPtr;

    /*
     *  If the argument is an ensemble part, then follow the chain
     *  back to the command word for the entire ensemble.
     */
    chainObj = ensObjPtr;
#ifdef NOTDEF
    while (chainObj && chainObj->typePtr == &itclEnsInvocType) {
         chainObj = (Tcl_Obj*)chainObj->internalRep.twoPtrValue.ptr2;
    }
#endif

    if (chainObj) {
        cmd = Tcl_GetCommandFromObj(interp, chainObj);
        if (Tcl_GetCommandInfoFromToken(cmd, &infoPtr) != 1) {
            return 0;
        }
        if (infoPtr.deleteProc == DeleteEnsemble) {
            ensData = (Ensemble*)infoPtr.objClientData;
            GetEnsembleUsage(interp, ensData, objPtr);
            return 1;
        }
    }
    return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * GetEnsembleUsage --
 *
 *      
 *      Returns a summary of all of the parts of an ensemble and
 *      the meaning of their arguments.  Each part is listed on
 *      a separate line.  This procedure is used internally to
 *      generate usage information for error messages.
 *
 * Results:
 *      Appends usage information onto the object in "objPtr".
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static void
GetEnsembleUsage(
    Tcl_Interp *interp,
    Ensemble *ensData,     /* ensemble data */
    Tcl_Obj *objPtr)       /* returns: summary of usage info */
{
    char *spaces = "  ";
    int isOpenEnded = 0;

    int i;
    EnsemblePart *ensPart;

    for (i=0; i < ensData->numParts; i++) {
        ensPart = ensData->parts[i];

        if ((*ensPart->name == '@') && (strcmp(ensPart->name,"@error") == 0)) {
            isOpenEnded = 1;
        } else {
            if ((*ensPart->name == '@') &&
	            (strcmp(ensPart->name,"@itcl-builtin_info") == 0)) {
		/* the builtin info command is not reported in [incr tcl] */
	        continue;
	    }
            Tcl_AppendToObj(objPtr, spaces, -1);
            GetEnsemblePartUsage(interp, ensData, ensPart, objPtr);
            spaces = "\n  ";
        }
    }
    if (isOpenEnded) {
        Tcl_AppendToObj(objPtr,
            "\n...and others described on the man page", -1);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * GetEnsemblePartUsage --
 *
 *      Determines the usage for a single part within an ensemble,
 *      and appends a summary onto a dynamic string.  The usage
 *      is a combination of the part name and the argument summary.
 *      It is the caller's responsibility to initialize and free
 *      the dynamic string.
 *
 * Results:
 *      Returns usage information in the object "objPtr".
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static void
GetEnsemblePartUsage(
    Tcl_Interp *interp,
    Ensemble *ensData,
    EnsemblePart *ensPart,   /* ensemble part for usage info */
    Tcl_Obj *objPtr)         /* returns: usage information */
{
    EnsemblePart *part;
    Tcl_Command cmdPtr;
    const char *name;
    Itcl_List trail;
    Itcl_ListElem *elem;
    Tcl_DString buffer;

    /*
     *  Build the trail of ensemble names leading to this part.
     */
    Tcl_DStringInit(&buffer);
    Itcl_InitList(&trail);
    for (part=ensPart; part; part=part->ensemble->parent) {
        Itcl_InsertList(&trail, (ClientData)part);
    }

    while (ensData->parent != NULL) {
        ensData = ensData->parent->ensemble;
    }
    cmdPtr = ensData->cmd;
    name = Tcl_GetCommandName(interp, cmdPtr);
    Tcl_DStringAppendElement(&buffer, name);

    for (elem=Itcl_FirstListElem(&trail); elem; elem=Itcl_NextListElem(elem)) {
        part = (EnsemblePart*)Itcl_GetListValue(elem);
        Tcl_DStringAppendElement(&buffer, part->name);
    }
    Itcl_DeleteList(&trail);

    /*
     *  If the part has usage info, use it directly.
     */
    if (ensPart->usage && *ensPart->usage != '\0') {
        Tcl_DStringAppend(&buffer, " ", 1);
        Tcl_DStringAppend(&buffer, ensPart->usage, -1);
    } else {

        /*
         *  If the part is itself an ensemble, summarize its usage.
         */
        if (ensPart->cmdPtr != NULL) {
	    if (Tcl_IsEnsemble(ensPart->cmdPtr)) {
                Tcl_DStringAppend(&buffer, " option ?arg arg ...?", 21);
	    }
        }
    }

    Tcl_AppendToObj(objPtr, Tcl_DStringValue(&buffer),
        Tcl_DStringLength(&buffer));

    Tcl_DStringFree(&buffer);
}


/*
 *----------------------------------------------------------------------
 *
 * CreateEnsemble --
 *
 *      Creates an ensemble command, or adds a sub-ensemble to an
 *      existing ensemble command.  Works like Itcl_CreateEnsemble,
 *      except that the ensemble name is a single name, not a path.
 *      If a parent ensemble is specified, then a new ensemble is
 *      added to that parent.  If a part already exists with the
 *      same name, it is an error.  If a parent ensemble is not
 *      specified, then a top-level ensemble is created.  If a
 *      command already exists with the same name, it is deleted.
 *
 * Results:
 *      Returns TCL_OK if successful, and TCL_ERROR if anything goes
 *      wrong.
 *
 * Side effects:
 *      If an error is encountered, an error is left as the result
 *      in the interpreter.
 *
 *----------------------------------------------------------------------
 */
static int
CreateEnsemble(
    Tcl_Interp *interp,            /* interpreter to be updated */
    Ensemble *parentEnsData,       /* parent ensemble or NULL */
    CONST84 char *ensName)         /* name of the new ensemble */
{
    Ensemble *ensData;
    EnsemblePart *ensPart;
    Tcl_HashEntry *hPtr;
    Tcl_InterpDeleteProc *procPtr;
    Tcl_DString buffer;
    ItclObjectInfo *infoPtr;
    int isNew;

    /*
     *  Create the data associated with the ensemble.
     */
    infoPtr = Tcl_GetAssocData(interp, ITCL_INTERP_DATA, &procPtr);
    infoPtr->ensembleInfo->numEnsembles++;
    ensData = (Ensemble*)ckalloc(sizeof(Ensemble));
    memset(ensData, 0, sizeof(Ensemble));
    ensData->interp = interp;
    ensData->numParts = 0;
    ensData->maxParts = 10;
    ensData->ensembleId = infoPtr->ensembleInfo->numEnsembles;
    ensData->parts = (EnsemblePart**)ckalloc(
        (unsigned)(ensData->maxParts*sizeof(EnsemblePart*))
    );
    memset(ensData->parts, 0, ensData->maxParts*sizeof(EnsemblePart*));
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, ITCL_COMMANDS_NAMESPACE "::ensembles::", -1);
    char buf[20];
    sprintf(buf, "%d", ensData->ensembleId);
    Tcl_DStringAppend(&buffer, buf, -1);
    ensData->nsPtr = Tcl_CreateNamespace(interp, Tcl_DStringValue(&buffer),
            NULL, NULL);
/* FIX ME !!! */
if (ensData->nsPtr == NULL) {
fprintf(stderr, "error in creating namespace: %s \n", Tcl_DStringValue(&buffer));
}
    Tcl_DStringFree(&buffer);

    /*
     *  If there is no parent data, then this is a top-level
     *  ensemble.  Create the ensemble by installing its access
     *  command.
     *
     *  BE CAREFUL:  Set the string-based proc to the wrapper
     *    procedure TclInvokeObjectCommand.  Otherwise, the
     *    ensemble command may fail.  For example, it will fail
     *    when invoked as a hidden command.
     */
    if (parentEnsData == NULL) {
	ensData->cmd = Tcl_CreateEnsemble(interp, ensName,
	        Tcl_GetCurrentNamespace(interp), TCL_ENSEMBLE_PREFIX);
        hPtr = Tcl_CreateHashEntry(&infoPtr->ensembleInfo->ensembles,
                (char *)ensData->cmd, &isNew);
	if (hPtr == NULL) {
	    return TCL_ERROR;
	}
        Tcl_SetHashValue(hPtr, (ClientData)ensData);
        Tcl_Obj *unkObjPtr = Tcl_NewStringObj(ITCL_COMMANDS_NAMESPACE, -1);
        Tcl_AppendToObj(unkObjPtr, "::ensembles::unknown", -1);
        Tcl_IncrRefCount(unkObjPtr);
        if (Tcl_SetEnsembleUnknownHandler(NULL, ensData->cmd,
                unkObjPtr) != TCL_OK) {
            return TCL_ERROR;
        }
        Tcl_DecrRefCount(unkObjPtr);

        return TCL_OK;
    }

    /*
     *  Otherwise, this ensemble is contained within another parent.
     *  Install the new ensemble as a part within its parent.
     */
    if (CreateEnsemblePart(interp, parentEnsData, ensName, &ensPart)
        != TCL_OK) {
        DeleteEnsemble((ClientData)ensData);
        return TCL_ERROR;
    }
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, infoPtr->ensembleInfo->ensembleNsPtr->fullName, -1);
    Tcl_DStringAppend(&buffer, "::subensembles::", -1);
    sprintf(buf, "%d", parentEnsData->ensembleId);
    Tcl_DStringAppend(&buffer, buf, -1);
    Tcl_DStringAppend(&buffer, "::", 2);
    Tcl_DStringAppend(&buffer, ensName, -1);
    Tcl_Obj *objPtr;
    objPtr = Tcl_NewStringObj(Tcl_DStringValue(&buffer), -1);
    Tcl_IncrRefCount(objPtr);
    hPtr = Tcl_CreateHashEntry(&infoPtr->ensembleInfo->subEnsembles,
            (char *)objPtr, &isNew);
    if (isNew) {
        Tcl_SetHashValue(hPtr, objPtr);
    }

    ensPart->cmdPtr = Tcl_CreateEnsemble(interp, Tcl_DStringValue(&buffer),
            Tcl_GetCurrentNamespace(interp), TCL_ENSEMBLE_PREFIX);
    hPtr = Tcl_CreateHashEntry(&infoPtr->ensembleInfo->ensembles,
            (char *)ensPart->cmdPtr, &isNew);
    if (hPtr == NULL) {
        return TCL_ERROR;
    }
    Tcl_SetHashValue(hPtr, (ClientData)ensData);
    Tcl_Obj *unkObjPtr = Tcl_NewStringObj(ITCL_COMMANDS_NAMESPACE, -1);
    Tcl_AppendToObj(unkObjPtr, "::ensembles::unknown", -1);
    Tcl_IncrRefCount(unkObjPtr);
    if (Tcl_SetEnsembleUnknownHandler(NULL, ensPart->cmdPtr,
            unkObjPtr) != TCL_OK) {
        return TCL_ERROR;
    }
    Tcl_DecrRefCount(unkObjPtr);

    Tcl_Obj *mapDict;
    Tcl_Obj *toObjPtr;
    Tcl_GetEnsembleMappingDict(NULL, parentEnsData->cmd, &mapDict);
    if (mapDict == NULL) {
        mapDict = Tcl_NewObj();
    }
    toObjPtr = Tcl_NewStringObj(Tcl_DStringValue(&buffer), -1);
    Tcl_DictObjPut(NULL, mapDict, Tcl_NewStringObj(ensName, -1), toObjPtr);
    ensData->cmd		= ensPart->cmdPtr;
//    ensData->cmd		= parentEnsData->cmd;
    ensData->parent		= ensPart;

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * AddEnsemblePart --
 *
 *      Adds a part to an existing ensemble.  Works like
 *      Itcl_AddEnsemblePart, but the part name is a single word,
 *      not a path.
 *
 *      If the ensemble already has a part with the specified name,
 *      this procedure returns an error.  Otherwise, it adds a new
 *      part to the ensemble.
 *
 *      Any client data specified is automatically passed to the
 *      handling procedure whenever the part is invoked.  It is
 *      automatically destroyed by the deleteProc when the part is
 *      deleted.
 *
 * Results:
 *      Returns TCL_OK if successful, along with a pointer to the
 *      new part.  Returns TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *      If an error is encountered, an error is left as the result
 *      in the interpreter.
 *
 *----------------------------------------------------------------------
 */
static int
AddEnsemblePart(
    Tcl_Interp *interp,            /* interpreter to be updated */
    Ensemble* ensData,             /* ensemble that will contain this part */
    CONST char* partName,          /* name of the new part */
    CONST char* usageInfo,         /* usage info for argument list */
    Tcl_ObjCmdProc *objProc,       /* handling procedure for part */
    ClientData clientData,         /* client data associated with part */
    Tcl_CmdDeleteProc *deleteProc, /* procedure used to destroy client data */
    int flags,
    EnsemblePart **rVal)           /* returns: new ensemble part */
{
    Tcl_Obj *mapDict;
    Tcl_Obj *toObjPtr;
    Tcl_Command cmd;
    EnsemblePart *ensPart;

    /*
     *  Install the new part into the part list.
     */
    if (CreateEnsemblePart(interp, ensData, partName, &ensPart) != TCL_OK) {
        return TCL_ERROR;
    }

    if (usageInfo) {
        ensPart->usage = ckalloc((unsigned)(strlen(usageInfo)+1));
        strcpy(ensPart->usage, usageInfo);
    }
    ensPart->namePtr = Tcl_NewStringObj(ensPart->name, -1);
    Tcl_IncrRefCount(ensPart->namePtr);
    ensPart->objProc = objProc;
    ensPart->clientData = clientData;
    ensPart->deleteProc = deleteProc;
    ensPart->flags = flags;

    Tcl_GetEnsembleMappingDict(NULL, ensData->cmd, &mapDict);
    if (mapDict == NULL) {
        mapDict = Tcl_NewObj();
    }
    toObjPtr = Tcl_NewStringObj(ensData->nsPtr->fullName, -1);
    Tcl_IncrRefCount(toObjPtr);
    Tcl_AppendToObj(toObjPtr, "::", 2);
    Tcl_AppendToObj(toObjPtr, partName, -1);
    Tcl_DictObjPut(NULL, mapDict, Tcl_NewStringObj(partName, -1), toObjPtr);
    cmd = Tcl_CreateObjCommand(interp, Tcl_GetString(toObjPtr),
            EnsembleSubCmd, ensPart, NULL);
    if (cmd == NULL) {
        return TCL_ERROR;
    }
    Tcl_SetEnsembleMappingDict(interp, ensData->cmd, mapDict);


    *rVal = ensPart;

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * DeleteEnsemble --
 *
 *      Invoked when the command associated with an ensemble is
 *      destroyed, to delete the ensemble.  Destroys all parts
 *      included in the ensemble, and frees all memory associated
 *      with it.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static void
DeleteEnsemble(
    ClientData clientData)    /* ensemble data */
{
    Ensemble* ensData = (Ensemble*)clientData;

    /*
     *  BE CAREFUL:  Each ensemble part removes itself from the list.
     *    So keep deleting the first part until all parts are gone.
     */
    while (ensData->numParts > 0) {
        DeleteEnsemblePart(ensData->parts[0]);
    }
    ckfree((char*)ensData->parts);
    ckfree((char*)ensData);
}


/*
 *----------------------------------------------------------------------
 *
 * FindEnsemble --
 *
 *      Searches for an ensemble command and follows a path to
 *      sub-ensembles.
 *
 * Results:
 *      Returns TCL_OK if the ensemble was found, along with a
 *      pointer to the ensemble data in "ensDataPtr".  Returns
 *      TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *      If anything goes wrong, this procedure returns an error
 *      message as the result in the interpreter.
 *
 *----------------------------------------------------------------------
 */
static int
FindEnsemble(
    Tcl_Interp *interp,            /* interpreter containing the ensemble */
    CONST84 char **nameArgv,       /* path of names leading to ensemble */
    int nameArgc,                  /* number of strings in nameArgv */
    Ensemble** ensDataPtr)         /* returns: ensemble data */
{
    int i;
    Tcl_Command cmdPtr;
    Ensemble *ensData;
    EnsemblePart *ensPart;
    Tcl_Obj *objPtr;
    Tcl_CmdInfo cmdInfo;
    Tcl_InterpDeleteProc *procPtr;
    Tcl_HashEntry *hPtr;
    ItclObjectInfo *infoPtr;

    *ensDataPtr = NULL;  /* assume that no data will be found */

    /*
     *  If there are no names in the path, then return an error.
     */
    if (nameArgc < 1) {
        Tcl_AppendToObj(Tcl_GetObjResult(interp),
            "invalid ensemble name \"\"", -1);
        return TCL_ERROR;
    }

    /*
     *  Use the first name to find the command for the top-level
     *  ensemble.
     */
    objPtr = Tcl_NewStringObj(nameArgv[0], -1);
    Tcl_IncrRefCount(objPtr);
    cmdPtr = Tcl_FindEnsemble(interp, objPtr, 0);
    Tcl_DecrRefCount(objPtr);

    if (cmdPtr == NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "command \"", nameArgv[0], "\" is not an ensemble",
            (char*)NULL);
        return TCL_ERROR;
    }
    infoPtr = Tcl_GetAssocData(interp, ITCL_INTERP_DATA, &procPtr);
    hPtr = Tcl_FindHashEntry(&infoPtr->ensembleInfo->ensembles, (char *)cmdPtr);
    if (hPtr == NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "command \"", nameArgv[0], "\" is not an ensemble",
            (char*)NULL);
        return TCL_ERROR;
    }
    ensData = (Ensemble *)Tcl_GetHashValue(hPtr);

    /*
     *  Follow the trail of sub-ensemble names.
     */
    for (i=1; i < nameArgc; i++) {
        if (FindEnsemblePart(interp, ensData, nameArgv[i], &ensPart)
            != TCL_OK) {
            return TCL_ERROR;
        }
        if (ensPart == NULL) {
            char *pname = Tcl_Merge(i, nameArgv);
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "invalid ensemble name \"", pname, "\"",
                (char*)NULL);
            ckfree(pname);
            return TCL_ERROR;
        }

        cmdPtr = ensPart->cmdPtr;
        if (cmdPtr == NULL) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "part \"", nameArgv[i], "\" is not an ensemble",
                (char*)NULL);
            return TCL_ERROR;
        }
	if (!Tcl_IsEnsemble(cmdPtr)) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "part \"", nameArgv[i], "\" is not an ensemble",
                (char*)NULL);
            return TCL_ERROR;
	}
        if (Tcl_GetCommandInfoFromToken(cmdPtr, &cmdInfo) != 1) {
            return TCL_ERROR;
        }
        ensData = (Ensemble*)cmdInfo.objClientData;
    }
    *ensDataPtr = ensData;

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * CreateEnsemblePart --
 *
 *      Creates a new part within an ensemble.
 *
 * Results:
 *      If successful, this procedure returns TCL_OK, along with a
 *      pointer to the new part in "ensPartPtr".  If a part with the
 *      same name already exists, this procedure returns TCL_ERROR.
 *
 * Side effects:
 *      If anything goes wrong, this procedure returns an error
 *      message as the result in the interpreter.
 *
 *----------------------------------------------------------------------
 */
static int
CreateEnsemblePart(
    Tcl_Interp *interp,          /* interpreter containing the ensemble */
    Ensemble *ensData,           /* ensemble being modified */
    CONST char* partName,        /* name of the new part */
    EnsemblePart **ensPartPtr)   /* returns: new ensemble part */
{
    int i, pos, size;
    EnsemblePart** partList;
    EnsemblePart* part;

    /*
     *  If a matching entry was found, then return an error.
     */
    if (FindEnsemblePartIndex(ensData, partName, &pos)) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "part \"", partName, "\" already exists in ensemble",
            (char*)NULL);
        return TCL_ERROR;
    }

    /*
     *  Otherwise, make room for a new entry.  Keep the parts in
     *  lexicographical order, so we can search them quickly
     *  later.
     */
    if (ensData->numParts >= ensData->maxParts) {
        size = ensData->maxParts*sizeof(EnsemblePart*);
        partList = (EnsemblePart**)ckalloc((unsigned)2*size);
        memcpy((VOID*)partList, (VOID*)ensData->parts, (size_t)size);
        ckfree((char*)ensData->parts);

        ensData->parts = partList;
        ensData->maxParts *= 2;
    }

    for (i=ensData->numParts; i > pos; i--) {
        ensData->parts[i] = ensData->parts[i-1];
    }
    ensData->numParts++;

    part = (EnsemblePart*)ckalloc(sizeof(EnsemblePart));
    memset(part, 0, sizeof(EnsemblePart));
    part->name = (char*)ckalloc((unsigned)(strlen(partName)+1));
    strcpy(part->name, partName);
    part->ensemble = ensData;

    ensData->parts[pos] = part;

    /*
     *  Compare the new part against the one on either side of
     *  it.  Determine how many letters are needed in each part
     *  to guarantee that an abbreviated form is unique.  Update
     *  the parts on either side as well, since they are influenced
     *  by the new part.
     */
    ComputeMinChars(ensData, pos);
    ComputeMinChars(ensData, pos-1);
    ComputeMinChars(ensData, pos+1);

    *ensPartPtr = part;
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * DeleteEnsemblePart --
 *
 *      Deletes a single part from an ensemble.  The part must have 
 *      been created previously by CreateEnsemblePart.
 *
 *      If the part has a delete proc, then it is called to free the
 *      associated client data.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Delete proc is called.
 *
 *----------------------------------------------------------------------
 */
static void
DeleteEnsemblePart(
    EnsemblePart *ensPart)     /* part being destroyed */
{
    int i, pos;
    Tcl_Command cmdPtr;
    Ensemble *ensData;
    Tcl_CmdInfo cmdInfo;

    cmdPtr = ensPart->cmdPtr;

    /*
     *  If this part has a delete proc, then call it to free
     *  up the client data.
     */
    if (Tcl_GetCommandInfoFromToken(cmdPtr, &cmdInfo) != 1) {
        return;
    }
    if ((cmdInfo.deleteData != NULL) && (cmdInfo.deleteProc != NULL)) {
        (*cmdInfo.deleteProc)(cmdInfo.deleteData);
    }
    ckfree((char*)cmdPtr);

    /*
     *  Find this part within its ensemble, and remove it from
     *  the list of parts.
     */
    if (FindEnsemblePartIndex(ensPart->ensemble, ensPart->name, &pos)) {
        ensData = ensPart->ensemble;
        for (i=pos; i < ensData->numParts-1; i++) {
            ensData->parts[i] = ensData->parts[i+1];
        }
        ensData->numParts--;
    }

    /*
     *  Free the memory associated with the part.
     */
    if (ensPart->usage) {
        ckfree(ensPart->usage);
    }
    ckfree(ensPart->name);
    ckfree((char*)ensPart);
}


/*
 *----------------------------------------------------------------------
 *
 * FindEnsemblePart --
 *
 *      Searches for a part name within an ensemble.  Recognizes
 *      unique abbreviations for part names.
 *
 * Results:
 *      If the part name is not a unique abbreviation, this procedure
 *      returns TCL_ERROR.  Otherwise, it returns TCL_OK.  If the
 *      part can be found, "rensPart" returns a pointer to the part.
 *      Otherwise, it returns NULL.
 *
 * Side effects:
 *      If anything goes wrong, this procedure returns an error
 *      message as the result in the interpreter.
 *
 *----------------------------------------------------------------------
 */
static int
FindEnsemblePart(
    Tcl_Interp *interp,       /* interpreter containing the ensemble */
    Ensemble *ensData,        /* ensemble being searched */
    CONST char* partName,     /* name of the desired part */
    EnsemblePart **rensPart)  /* returns:  pointer to the desired part */
{
    int pos = 0;
    int first, last, nlen;
    int i, cmp;

    *rensPart = NULL;

    /*
     *  Search for the desired part name.
     *  All parts are in lexicographical order, so use a
     *  binary search to find the part quickly.  Match only
     *  as many characters as are included in the specified
     *  part name.
     */
    first = 0;
    last  = ensData->numParts-1;
    nlen  = strlen(partName);

    while (last >= first) {
        pos = (first+last)/2;
        if (*partName == *ensData->parts[pos]->name) {
            cmp = strncmp(partName, ensData->parts[pos]->name, nlen);
            if (cmp == 0) {
                break;    /* found it! */
            }
        }
        else if (*partName < *ensData->parts[pos]->name) {
            cmp = -1;
        }
        else {
            cmp = 1;
        }

        if (cmp > 0) {
            first = pos+1;
        } else {
            last = pos-1;
        }
    }

    /*
     *  If a matching entry could not be found, then quit.
     */
    if (last < first) {
        return TCL_OK;
    }

    /*
     *  If a matching entry was found, there may be some ambiguity
     *  if the user did not specify enough characters.  Find the
     *  top-most match in the list, and see if the part name has
     *  enough characters.  If there are two parts like "foo"
     *  and "food", this allows us to match "foo" exactly.
     */
    if (nlen < ensData->parts[pos]->minChars) {
        while (pos > 0) {
            pos--;
            if (strncmp(partName, ensData->parts[pos]->name, nlen) != 0) {
                pos++;
                break;
            }
        }
    }
    if (nlen < ensData->parts[pos]->minChars) {
        Tcl_Obj *resultPtr = Tcl_NewStringObj((char*)NULL, 0);

        Tcl_AppendStringsToObj(resultPtr,
            "ambiguous option \"", partName, "\": should be one of...",
            (char*)NULL);

        for (i=pos; i < ensData->numParts; i++) {
            if (strncmp(partName, ensData->parts[i]->name, nlen) != 0) {
                break;
            }
            Tcl_AppendToObj(resultPtr, "\n  ", 3); 
            GetEnsemblePartUsage(interp, ensData, ensData->parts[i], resultPtr);
        }
        Tcl_SetObjResult(interp, resultPtr);
        return TCL_ERROR;
    }

    /*
     *  Found a match.  Return the desired part.
     */
    *rensPart = ensData->parts[pos];
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * FindEnsemblePartIndex --
 *
 *      Searches for a part name within an ensemble.  The part name
 *      must be an exact match for an existing part name in the
 *      ensemble.  This procedure is useful for managing (i.e.,
 *      creating and deleting) parts in an ensemble.
 *
 * Results:
 *      If an exact match is found, this procedure returns
 *      non-zero, along with the index of the part in posPtr.
 *      Otherwise, it returns zero, along with an index in posPtr
 *      indicating where the part should be.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static int
FindEnsemblePartIndex(
    Ensemble *ensData,        /* ensemble being searched */
    CONST char *partName,     /* name of desired part */
    int *posPtr)              /* returns: index for part */
{
    int pos = 0;
    int first, last;
    int cmp;

    /*
     *  Search for the desired part name.
     *  All parts are in lexicographical order, so use a
     *  binary search to find the part quickly.
     */
    first = 0;
    last  = ensData->numParts-1;

    while (last >= first) {
        pos = (first+last)/2;
        if (*partName == *ensData->parts[pos]->name) {
            cmp = strcmp(partName, ensData->parts[pos]->name);
            if (cmp == 0) {
                break;    /* found it! */
            }
        }
        else if (*partName < *ensData->parts[pos]->name) {
            cmp = -1;
        }
        else {
            cmp = 1;
        }

        if (cmp > 0) {
            first = pos+1;
        } else {
            last = pos-1;
        }
    }

    if (last >= first) {
        *posPtr = pos;
        return 1;
    }
    *posPtr = first;
    return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * ComputeMinChars --
 *
 *      Compares part names on an ensemble's part list and
 *      determines the minimum number of characters needed for a
 *      unique abbreviation.  The parts on either side of a
 *      particular part index are compared.  As long as there is
 *      a part on one side or the other, this procedure updates
 *      the parts to have the proper minimum abbreviations.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Updates three parts within the ensemble to remember
 *      the minimum abbreviations.
 *
 *----------------------------------------------------------------------
 */
static void
ComputeMinChars(
    Ensemble *ensData,        /* ensemble being modified */
    int pos)                  /* index of part being updated */
{
    int min, max;
    char *p, *q;

    /*
     *  If the position is invalid, do nothing.
     */
    if (pos < 0 || pos >= ensData->numParts) {
        return;
    }

    /*
     *  Start by assuming that only the first letter is required
     *  to uniquely identify this part.  Then compare the name
     *  against each neighboring part to determine the real minimum.
     */
    ensData->parts[pos]->minChars = 1;

    if (pos-1 >= 0) {
        p = ensData->parts[pos]->name;
        q = ensData->parts[pos-1]->name;
        for (min=1; *p == *q && *p != '\0' && *q != '\0'; min++) {
            p++;
            q++;
        }
        if (min > ensData->parts[pos]->minChars) {
            ensData->parts[pos]->minChars = min;
        }
    }

    if (pos+1 < ensData->numParts) {
        p = ensData->parts[pos]->name;
        q = ensData->parts[pos+1]->name;
        for (min=1; *p == *q && *p != '\0' && *q != '\0'; min++) {
            p++;
            q++;
        }
        if (min > ensData->parts[pos]->minChars) {
            ensData->parts[pos]->minChars = min;
        }
    }

    max = strlen(ensData->parts[pos]->name);
    if (ensData->parts[pos]->minChars > max) {
        ensData->parts[pos]->minChars = max;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Itcl_EnsembleCmd --
 *
 *      Invoked by Tcl whenever the user issues the "ensemble"
 *      command to manipulate an ensemble.  Handles the following
 *      syntax:
 *
 *        ensemble <ensName> ?<command> <arg> <arg>...?
 *        ensemble <ensName> {
 *            part <partName> <args> <body>
 *            ensemble <ensName> {
 *                ...
 *            }
 *        }
 *
 *      Finds or creates the ensemble <ensName>, and then executes
 *      the commands to add parts.
 *
 * Results:
 *      Returns TCL_OK if successful, and TCL_ERROR if anything
 *      goes wrong.
 *
 * Side effects:
 *      If anything goes wrong, this procedure returns an error
 *      message as the result in the interpreter.
 *
 *----------------------------------------------------------------------
 */
int
Itcl_EnsembleCmd(
    ClientData clientData,   /* ensemble data */
    Tcl_Interp *interp,      /* current interpreter */
    int objc,                /* number of arguments */
    Tcl_Obj *CONST objv[])   /* argument objects */
{
    int status;
    char *ensName;
    EnsembleParser *ensInfo;
    Ensemble *ensData;
    Ensemble *savedEnsData;
    EnsemblePart *ensPart;
    Tcl_Command cmd;
    Tcl_Obj *objPtr;
    Tcl_HashEntry *hPtr;
    Tcl_InterpDeleteProc *procPtr;
    ItclObjectInfo *infoPtr;

    ItclShowArgs(2, "Itcl_EnsembleCmd", objc, objv);
    /*
     *  Make sure that an ensemble name was specified.
     */
    if (objc < 2) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "wrong # args: should be \"",
            Tcl_GetStringFromObj(objv[0], (int*)NULL),
            " name ?command arg arg...?\"",
            (char*)NULL);
        return TCL_ERROR;
    }

    /*
     *  If this is the "ensemble" command in the main interpreter,
     *  then the client data will be null.  Otherwise, it is
     *  the "ensemble" command in the ensemble body parser, and
     *  the client data indicates which ensemble we are modifying.
     */
    if (clientData) {
        ensInfo = (EnsembleParser*)clientData;
    } else {
        ensInfo = GetEnsembleParser(interp);
    }
    ensData = ensInfo->ensData;

    /*
     *  Find or create the desired ensemble.  If an ensemble is
     *  being built, then this "ensemble" command is enclosed in
     *  another "ensemble" command.  Use the current ensemble as
     *  the parent, and find or create an ensemble part within it.
     */
    ensName = Tcl_GetStringFromObj(objv[1], (int*)NULL);

    if (ensData) {
        if (FindEnsemblePart(interp, ensData, ensName, &ensPart) != TCL_OK) {
            ensPart = NULL;
        }
        if (ensPart == NULL) {
            if (CreateEnsemble(interp, ensData, ensName) != TCL_OK) {
                return TCL_ERROR;
            }
            if (FindEnsemblePart(interp, ensData, ensName, &ensPart)
                    != TCL_OK) {
                Tcl_Panic("Itcl_EnsembleCmd: can't create ensemble");
            }
        }

        cmd = ensPart->cmdPtr;
        infoPtr = Tcl_GetAssocData(interp, ITCL_INTERP_DATA, &procPtr);
        hPtr = Tcl_FindHashEntry(&infoPtr->ensembleInfo->ensembles,
	        (char *)ensPart->cmdPtr);
        if (hPtr == NULL) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "part \"", Tcl_GetStringFromObj(objv[1], (int*)NULL),
                "\" is not an ensemble",
                (char*)NULL);
            return TCL_ERROR;
	}
        ensData = (Ensemble *)Tcl_GetHashValue(hPtr);
    } else {

        /*
         *  Otherwise, the desired ensemble is a top-level ensemble.
         *  Find or create the access command for the ensemble, and
         *  then get its data.
         */
        cmd = Tcl_FindCommand(interp, ensName, (Tcl_Namespace*)NULL, 0);
        if (cmd == NULL) {
            if (CreateEnsemble(interp, (Ensemble*)NULL, ensName)
                != TCL_OK) {
                return TCL_ERROR;
            }
            cmd = Tcl_FindCommand(interp, ensName, (Tcl_Namespace*)NULL, 0);
        }

        if (cmd == NULL) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "command \"", Tcl_GetStringFromObj(objv[1], (int*)NULL),
                "\" is not an ensemble",
                (char*)NULL);
            return TCL_ERROR;
        }
        infoPtr = Tcl_GetAssocData(interp, ITCL_INTERP_DATA, &procPtr);
        hPtr = Tcl_FindHashEntry(&infoPtr->ensembleInfo->ensembles, (char *)cmd);
        if (hPtr == NULL) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "command \"", Tcl_GetStringFromObj(objv[1], (int*)NULL),
                "\" is not an ensemble",
                (char*)NULL);
            return TCL_ERROR;
        }
        ensData = (Ensemble *)Tcl_GetHashValue(hPtr);
    }

    /*
     *  At this point, we have the data for the ensemble that is
     *  being manipulated.  Plug this into the parser, and then
     *  interpret the rest of the arguments in the ensemble parser. 
     */
    status = TCL_OK;
    savedEnsData = ensInfo->ensData;
    ensInfo->ensData = ensData;

    if (objc == 3) {
        status = Tcl_EvalObj(ensInfo->parser, objv[2]);
    } else {
        if (objc > 3) {
            objPtr = Tcl_NewListObj(objc-2, objv+2);
            Tcl_IncrRefCount(objPtr);  /* stop Eval trashing it */
            status = Tcl_EvalObj(ensInfo->parser, objPtr);
            Tcl_DecrRefCount(objPtr);  /* we're done with the object */
        }
    }

    /*
     *  Copy the result from the parser interpreter to the
     *  master interpreter.  If an error was encountered,
     *  copy the error info first, and then set the result.
     *  Otherwise, the offending command is reported twice.
     */
    if (status == TCL_ERROR) {
#ifdef NOTDEF
	/* no longer needed, no extra interpreter !! */
        CONST char *errInfo = Tcl_GetVar2(ensInfo->parser, "::errorInfo",
            (char*)NULL, TCL_GLOBAL_ONLY);

        if (errInfo) {
            Tcl_AddObjErrorInfo(interp, (CONST84 char *)errInfo, -1);
        }
#endif

        if (objc == 3) {
            char msg[128];
            sprintf(msg, "\n    (\"ensemble\" body line %d)",
                ensInfo->parser->errorLine);
            Tcl_AddObjErrorInfo(interp, msg, -1);
        }
    }
    Tcl_SetObjResult(interp, Tcl_GetObjResult(ensInfo->parser));

    ensInfo->ensData = savedEnsData;
    return status;
}


/*
 *----------------------------------------------------------------------
 *
 * GetEnsembleParser --
 *
 *      Returns the slave interpreter that acts as a parser for
 *      the body of an "ensemble" definition.  The first time that
 *      this is called for an interpreter, the parser is created
 *      and registered as associated data.  After that, it is
 *      simply returned.
 *
 * Results:
 *      Returns a pointer to the ensemble parser data structure.
 *
 * Side effects:
 *      On the first call, the ensemble parser is created and
 *      registered as "itcl_ensembleParser" with the interpreter.
 *
 *----------------------------------------------------------------------
 */
static EnsembleParser*
GetEnsembleParser(
    Tcl_Interp *interp)     /* interpreter handling the ensemble */
{
    Tcl_Namespace *nsPtr;
    EnsembleParser *ensInfo;

    /*
     *  Look for an existing ensemble parser.  If it is found,
     *  return it immediately.
     */
    ensInfo = (EnsembleParser*) Tcl_GetAssocData(interp,
        "itcl_ensembleParser", NULL);

    if (ensInfo) {
        return ensInfo;
    }

    /*
     *  Create a slave interpreter that can be used to parse
     *  the body of an ensemble definition.
     */
    ensInfo = (EnsembleParser*)ckalloc(sizeof(EnsembleParser));
    ensInfo->master = interp;
    ensInfo->parser = interp;
    ensInfo->ensData = NULL;

    /*
     *  Remove all namespaces and all normal commands from the
     *  parser interpreter.
     */
    nsPtr = Tcl_GetGlobalNamespace(ensInfo->parser);

#ifdef NOTDEF
    for (hPtr = Tcl_FirstHashEntry(&nsPtr->childTable, &search);
         hPtr != NULL;
         hPtr = Tcl_FirstHashEntry(&nsPtr->childTable, &search)) {

        childNs = (Tcl_Namespace*)Tcl_GetHashValue(hPtr);
        Tcl_DeleteNamespace(childNs);
    }

    for (hPtr = Tcl_FirstHashEntry(&nsPtr->cmdTable, &search);
         hPtr != NULL;
         hPtr = Tcl_FirstHashEntry(&nsPtr->cmdTable, &search)) {

        cmd = (Tcl_Command)Tcl_GetHashValue(hPtr);
        Tcl_DeleteCommandFromToken(ensInfo->parser, cmd);
    }

#endif
    /*
     *  Add the allowed commands to the parser interpreter:
     *  part, delete, ensemble
     */
    Tcl_CreateObjCommand(ensInfo->parser, "part", Itcl_EnsPartCmd,
        (ClientData)ensInfo, (Tcl_CmdDeleteProc*)NULL);

    Tcl_CreateObjCommand(ensInfo->parser, "option", Itcl_EnsPartCmd,
        (ClientData)ensInfo, (Tcl_CmdDeleteProc*)NULL);

    Tcl_CreateObjCommand(ensInfo->parser, "ensemble", Itcl_EnsembleCmd,
        (ClientData)ensInfo, (Tcl_CmdDeleteProc*)NULL);

    /*
     *  Install the parser data, so we'll have it the next time
     *  we call this procedure.
     */
    (void) Tcl_SetAssocData(interp, "itcl_ensembleParser",
            DeleteEnsParser, (ClientData)ensInfo);

    return ensInfo;
}


/*
 *----------------------------------------------------------------------
 *
 * DeleteEnsParser --
 *
 *      Called when an interpreter is destroyed to clean up the
 *      ensemble parser within it.  Destroys the slave interpreter
 *      and frees up the data associated with it.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
	/* ARGSUSED */
static void
DeleteEnsParser(
    ClientData clientData,    /* client data for ensemble-related commands */
    Tcl_Interp *interp)       /* interpreter containing the data */
{
    EnsembleParser* ensInfo = (EnsembleParser*)clientData;
    Tcl_DeleteInterp(ensInfo->parser);
    ckfree((char*)ensInfo);
}


/*
 *----------------------------------------------------------------------
 *
 * Itcl_EnsPartCmd --
 *
 *      Invoked by Tcl whenever the user issues the "part" command
 *      to manipulate an ensemble.  This command can only be used
 *      inside the "ensemble" command, which handles ensembles.
 *      Handles the following syntax:
 *
 *        ensemble <ensName> {
 *            part <partName> <args> <body>
 *        }
 *
 *      Adds a new part called <partName> to the ensemble.  If a
 *      part already exists with that name, it is an error.  The
 *      new part is handled just like an ordinary Tcl proc, with
 *      a list of <args> and a <body> of code to execute.
 *
 * Results:
 *      Returns TCL_OK if successful, and TCL_ERROR if anything
 *      goes wrong.
 *
 * Side effects:
 *      If anything goes wrong, this procedure returns an error
 *      message as the result in the interpreter.
 *
 *----------------------------------------------------------------------
 */
int
Itcl_EnsPartCmd(
    ClientData clientData,   /* ensemble data */
    Tcl_Interp *interp,      /* current interpreter */
    int objc,                /* number of arguments */
    Tcl_Obj *CONST objv[])   /* argument objects */
{
    EnsembleParser *ensInfo = (EnsembleParser*)clientData;
    Ensemble *ensData = (Ensemble*)ensInfo->ensData;

    ItclArgList *arglistPtr;
    int status;
    int argc;
    int maxArgc;

    char *partName;
    char *usage;
    Tcl_Obj *usagePtr;
    Tcl_Proc procPtr;
    Tcl_Command cmdPtr;
    EnsemblePart *ensPart;

    ItclShowArgs(2, "Itcl_EnsPartCmd", objc, objv);
    if (objc != 4) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "wrong # args: should be \"",
            Tcl_GetStringFromObj(objv[0], (int*)NULL),
            " name args body\"",
            (char*)NULL);
        return TCL_ERROR;
    }

    /*
     *  Create a Tcl-style proc definition using the specified args
     *  and body.  This is not a proc in the usual sense.  It belongs
     *  to the namespace that contains the ensemble, but it is
     *  accessed through the ensemble, not through a Tcl command.
     */
    partName = Tcl_GetStringFromObj(objv[1], (int*)NULL);
    cmdPtr = ensData->cmd;

    if (ItclCreateArgList(interp, Tcl_GetString(objv[2]), &argc, &maxArgc,
            &usagePtr, &arglistPtr, NULL, partName) != TCL_OK) {
        return TCL_ERROR;
    }
    Tcl_CmdInfo cmdInfo;
    if (Tcl_GetCommandInfoFromToken(ensData->cmd, &cmdInfo) != 1) {
        return TCL_ERROR;
    }
    if (Tcl_CreateProc(interp, cmdInfo.namespacePtr, partName, objv[2], objv[3],
            &procPtr) != TCL_OK) {
        return TCL_ERROR;
    }

    usage = Tcl_GetString(usagePtr);

    /*
     *  Create a new part within the ensemble.  If successful,
     *  plug the command token into the proc; we'll need it later
     *  if we try to compile the Tcl code for the part.  If
     *  anything goes wrong, clean up before bailing out.
     */
    status = AddEnsemblePart(interp, ensData, partName, usage,
        Tcl_GetObjInterpProc(), (ClientData)procPtr, Tcl_ProcDeleteProc,
        ITCL_ENSEMBLE_ENSEMBLE, &ensPart);

    return status;
}


/*
 *----------------------------------------------------------------------
 *
 * Itcl_EnsembleErrorCmd --
 *
 *      Invoked when the user tries to access an unknown part for
 *      an ensemble.  Acts as the default handler for the "@error"
 *      part.  Generates an error message like:
 *
 *          bad option "foo": should be one of...
 *            info args procname
 *            info body procname
 *            info cmdcount
 *            ...
 *
 * Results:
 *      Always returns TCL_OK.
 *
 * Side effects:
 *      Returns the error message as the result in the interpreter.
 *
 *----------------------------------------------------------------------
 */
	/* ARGSUSED */
int
Itcl_EnsembleErrorCmd(
    ClientData clientData,   /* ensemble info */
    Tcl_Interp *interp,      /* current interpreter */
    int objc,                /* number of arguments */
    Tcl_Obj *CONST objv[])   /* argument objects */
{
    Ensemble *ensData = (Ensemble*)clientData;

    char *cmdName;
    Tcl_Obj *objPtr;

    cmdName = Tcl_GetString(objv[0]);

    objPtr = Tcl_NewStringObj((char*)NULL, 0);
    Tcl_AppendStringsToObj(objPtr,
        "bad option \"", cmdName, "\": should be one of...\n",
        (char*)NULL);
    GetEnsembleUsage(interp, ensData, objPtr);

    Tcl_SetObjResult(interp, objPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * EnsembleSubCmd --
 *
 *----------------------------------------------------------------------
 */

static int
EnsembleSubCmd(
    ClientData clientData,      /* ensPart struct pointer */
    Tcl_Interp *interp,         /* Current interpreter. */
    int objc,                   /* Number of arguments. */
    Tcl_Obj *CONST objv[])      /* Argument objects. */
{
    int result;
    Tcl_Namespace *nsPtr;
    EnsemblePart *ensPart;

    ItclShowArgs(2, "EnsembleSubCmd", objc, objv);
    result = TCL_OK;
    ensPart = (EnsemblePart *)clientData;
    nsPtr = Tcl_GetCurrentNamespace(interp);
    if (ensPart->flags & ITCL_ENSEMBLE_ENSEMBLE) {
/* FIX ME !!! */
if (ensPart->clientData == NULL) {
    return TCL_ERROR;
}
        result = Tcl_InvokeNamespaceProc(interp, (Tcl_Proc)ensPart->clientData,
	        nsPtr, ensPart->namePtr, objc, objv);
    } else {
        result = (*ensPart->objProc)(ensPart->clientData,
	        interp, objc, objv);
    }
    return result;
}
/*
 * ------------------------------------------------------------------------
 *  EnsembleUnknownCmd()
 *
 *  the unknown handler for the ensemble commands
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
EnsembleUnknownCmd(
    ClientData dummy,        /* not used */
    Tcl_Interp *interp,      /* current interpreter */
    int objc,                /* number of arguments */
    Tcl_Obj *CONST objv[])   /* argument objects */
{
    Tcl_Command cmd;
    Tcl_HashEntry *hPtr;
    Tcl_InterpDeleteProc *procPtr;
    ItclObjectInfo *infoPtr;
    EnsemblePart *ensPart;
    Ensemble *ensData;

    ItclShowArgs(2, "EnsembleUnknownCmd", objc, objv);
    cmd = Tcl_GetCommandFromObj(interp, objv[1]);
/* FIX ME !!! */
    if (cmd == NULL) {
fprintf(stderr, "EnsembleUnknownCmd, ensemble not found!%s!\n", Tcl_GetString(objv[1]));
        return TCL_ERROR;
    }
    infoPtr = Tcl_GetAssocData(interp, ITCL_INTERP_DATA, &procPtr);
    hPtr = Tcl_FindHashEntry(&infoPtr->ensembleInfo->ensembles, (char *)cmd);
    if (hPtr == NULL) {
/* FIX ME !!! */
//fprintf(stderr, "EnsembleUnknownCmd, ensemble struct not found!%s!\n", Tcl_GetString(objv[1]));
        return TCL_ERROR;
    }
    ensData = (Ensemble *)Tcl_GetHashValue(hPtr);
    if (objc < 3) {
        /* produce usage message */
        Tcl_Obj *objPtr = Tcl_NewStringObj(
                "wrong # args: should be one of...\n", -1);
        GetEnsembleUsage(interp, ensData, objPtr);
        Tcl_SetResult(interp, Tcl_GetString(objPtr), TCL_DYNAMIC);
        return TCL_ERROR;
    }
    if (FindEnsemblePart(interp, ensData, "@error", &ensPart) != TCL_OK) {
/* FIX ME !!! */
fprintf(stderr, "FindEnsemblePart error\n");
        return TCL_ERROR;
    }
    if (ensPart != NULL) {
        Tcl_Obj *listPtr;

	listPtr = Tcl_NewListObj(0, (Tcl_Obj**)NULL);
	Tcl_ListObjAppendElement(NULL, listPtr, objv[1]);
	Tcl_ListObjAppendElement(NULL, listPtr, Tcl_NewStringObj("@error", -1));
	Tcl_ListObjAppendElement(NULL, listPtr, objv[2]);
        Tcl_IncrRefCount(listPtr);
	Tcl_SetObjResult(interp, listPtr);
        return TCL_OK;
    }

    return Itcl_EnsembleErrorCmd(ensData, interp, objc-2, objv+2);
}