//
// Created by ali on 21.11.2016.
//

#ifndef SDK_DSLINK_C_REPORT_HANDLER_H
#define SDK_DSLINK_C_REPORT_HANDLER_H
#ifdef __cplusplus
extern "C" {
#endif

#include <dslink/dslink.h>
#include <dslink/node.h>
#include "iec61850_client.h"

#define RCB_TYPE_KEY "rcbtype"
#define RCB_OBJ_REF_KEY "objref"
#define RCB_RPT_ID_KEY "rptid"
#define RCB_RPT_ENA_KEY "rptena"
#define RCB_DATASET_KEY "rcbdatset"
#define RCB_CONF_REV_KEY "rcbconfrev"
#define RCB_OPT_FLDS_KEY "rcboptflds"
#define RCB_BUF_TM_KEY "rcbbuftm"
#define RCB_TRG_OPS_KEY "rcbtrgops"
#define RCB_INTG_PD_KEY "rcbintgpd"

#define MAX_OPT_FLDS_STR_LEN 160
#define MAX_TRG_OPS_STR_LEN 100

typedef struct {
    LinkedList dataSetDirectory;
    DSNode *rcbNode;
}tsReportParam;

extern Map *serverMap;
DSNode *getURCBFolderNode(DSNode *node);
DSNode *getBRCBFolderNode(DSNode *node);
DSNode *getRCBInfoAndCreateNode(DSLink *link, DSNode *currNode, IedConnection iedConn, const char* rcbRef, bool isBuffered);

extern void reportCallbackFunction(void* parameter, ClientReport report);

#ifdef __cplusplus
}
#endif
#endif //SDK_DSLINK_C_REPORT_HANDLER_H
