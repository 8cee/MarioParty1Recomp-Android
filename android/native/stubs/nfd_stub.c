#include "nfd.h"
#include <stdlib.h>
#include <stdio.h>

static char g_error[128] = "Native file dialogs are unavailable on Android";
const char* NFD_GetError(void) { return g_error; }
void NFD_ClearError(void) { g_error[0] = 0; }
nfdresult_t NFD_Init(void) { return NFD_OKAY; }
void NFD_Quit(void) {}
void NFD_FreePathN(nfdnchar_t* p) { free(p); }
nfdresult_t NFD_OpenDialogN(nfdnchar_t** out, const nfdnfilteritem_t* f, nfdfiltersize_t n, const nfdnchar_t* p) {
    (void)f;(void)n;(void)p; if(out)*out=NULL; return NFD_CANCEL;
}
nfdresult_t NFD_OpenDialogMultipleN(const nfdpathset_t** out, const nfdnfilteritem_t* f, nfdfiltersize_t n, const nfdnchar_t* p) {
    (void)f;(void)n;(void)p; if(out)*out=NULL; return NFD_CANCEL;
}
nfdresult_t NFD_SaveDialogN(nfdnchar_t** out, const nfdnfilteritem_t* f, nfdfiltersize_t n, const nfdnchar_t* p, const nfdnchar_t* name) {
    (void)f;(void)n;(void)p;(void)name; if(out)*out=NULL; return NFD_CANCEL;
}
nfdresult_t NFD_PickFolderN(nfdnchar_t** out, const nfdnchar_t* p) {
    (void)p; if(out)*out=NULL; return NFD_CANCEL;
}
nfdresult_t NFD_PathSet_GetCount(const nfdpathset_t* p, nfdpathsetsize_t* count) {
    (void)p; if(count)*count=0; return NFD_OKAY;
}
nfdresult_t NFD_PathSet_GetPathN(const nfdpathset_t* p, nfdpathsetsize_t i, nfdnchar_t** out) {
    (void)p;(void)i; if(out)*out=NULL; return NFD_ERROR;
}
void NFD_PathSet_Free(const nfdpathset_t* p) { (void)p; }
