//
// Created by ali on 12.11.2016.
//

#ifndef SDK_DSLINK_C_DATASET_HANDLER_H
#define SDK_DSLINK_C_DATASET_HANDLER_H
#ifdef __cplusplus
extern "C" {
#endif

#include <dslink/dslink.h>
#include <dslink/node.h>
#include "iec61850_client.h"

extern Map *serverMap;

DSNode *getDANode(DSNode *node, const char* daRef);
DSNode *getMainDatasetNode(DSNode *node);
DSNode *getMainPrepDatasetNode(DSNode *node);
DSNode *createDatasetNode(DSLink *link, DSNode *mainDatasetNode, const char* dataSetRef, bool isDeletable);
void addMemberToDatasetNode(DSLink *link, DSNode *newDSNode, const char* memberName);
void addMemberToPrepDatasetNode(DSLink *link, DSNode *newDSNode, const char* memberName);
bool createPrepDatasetNode(DSLink *link, DSNode *lnNode, const char* dsRef);
bool getPrepDSListEnumStr(DSNode *node, char *enumStr);
bool getDatasetInfoFromServerCreateNode(DSLink *link, DSNode *currNode, IedConnection iedConn, const char *dsRef);
DSNode *getDataSetNodeFromRef(DSNode *node, const char* dataSetRef);
#ifdef __cplusplus
}
#endif
#endif //SDK_DSLINK_C_DATASET_HANDLER_H
