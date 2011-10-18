
#ifndef TCL_OO_INTERNAL_H
typedef int (TclOO_PreCallProc)(ClientData clientData, Tcl_Interp *interp,
	Tcl_ObjectContext context, Tcl_CallFrame *framePtr, int *isFinished);
typedef int (TclOO_PostCallProc)(ClientData clientData, Tcl_Interp *interp,
	Tcl_ObjectContext context, Tcl_Namespace *namespacePtr, int result);
#endif

#define Itcl_NRAddCallback(interp,procPtr,data0,data1,data2,data3) \
  Itcl_NRAddCallback_(interp, #procPtr, procPtr, data0, data1, data2, data3)
MODULE_SCOPE void Itcl_NRAddCallback_(Tcl_Interp *interp, char *procName,
        Tcl_NRPostProc *procPtr, ClientData data0, ClientData data1,
        ClientData data2, ClientData data3);
MODULE_SCOPE void Itcl_DumpNRCallbacks(Tcl_Interp *interp, char *str);
MODULE_SCOPE int Itcl_NRCallObjProc(ClientData clientData, Tcl_Interp *interp,
        Tcl_ObjCmdProc *objProc, int objc, Tcl_Obj *const *objv);
MODULE_SCOPE int Itcl_NRRunCallbacks(Tcl_Interp *interp, void *rootPtr);
MODULE_SCOPE void * Itcl_GetCurrentCallbackPtr(Tcl_Interp *interp);
MODULE_SCOPE Tcl_Method Itcl_NewProcClassMethod(Tcl_Interp *interp, Tcl_Class clsPtr,
        TclOO_PreCallProc preCallPtr, TclOO_PostCallProc postCallPtr,
        Tcl_ProcErrorProc errProc, ClientData clientData, Tcl_Obj *nameObj,
	Tcl_Obj *argsObj, Tcl_Obj *bodyObj, ClientData *clientData2);
MODULE_SCOPE Tcl_Method Itcl_NewProcMethod(Tcl_Interp *interp, Tcl_Object oPtr,
        TclOO_PreCallProc preCallPtr, TclOO_PostCallProc postCallPtr,
        Tcl_ProcErrorProc errProc, ClientData clientData, Tcl_Obj *nameObj,
	Tcl_Obj *argsObj, Tcl_Obj *bodyObj, ClientData *clientData2);
MODULE_SCOPE int Itcl_PublicObjectCmd(ClientData clientData, Tcl_Interp *interp,
        Tcl_Class clsPtr, int objc, Tcl_Obj *const *objv);
MODULE_SCOPE Tcl_Method Itcl_NewForwardClassMethod(Tcl_Interp *interp,
        Tcl_Class clsPtr, int flags, Tcl_Obj *nameObj, Tcl_Obj *prefixObj);
MODULE_SCOPE Tcl_Method Itcl_NewForwardMethod(Tcl_Interp *interp, Tcl_Object oPtr,
        int flags, Tcl_Obj *nameObj, Tcl_Obj *prefixObj);
MODULE_SCOPE int Itcl_SelfCmd(ClientData clientData, Tcl_Interp *interp,
        int objc, Tcl_Obj *const *objv);
MODULE_SCOPE Tcl_Obj * Itcl_TclOOObjectName(Tcl_Interp *interp, Tcl_Object oPtr);
MODULE_SCOPE int Itcl_InvokeEnsembleMethod(Tcl_Interp *interp, Tcl_Namespace *nsPtr,
    Tcl_Obj *namePtr, Tcl_Proc procPtr, int objc, Tcl_Obj *const *objv);
MODULE_SCOPE int Itcl_InvokeProcedureMethod(ClientData clientData, Tcl_Interp *interp,
	int objc, Tcl_Obj *const *objv);

