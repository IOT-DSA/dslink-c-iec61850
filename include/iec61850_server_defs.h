//
// Created by ali on 25.09.2016.
//

#ifndef SDK_DSLINK_C_IEC61850_SERVER_DEFS_H
#define SDK_DSLINK_C_IEC61850_SERVER_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "iec61850_client.h"
#include <dslink/node.h>
#include <dslink/dslink.h>

//#define DEVEL_DEBUG

#define MAX_IEC61850_SERVER_HOSTNAME 100

#define NODE_KEY_INIT_LEN 4
#define SERVER_NODE_KEY_INIT "srv_"
#define NO_DELETE_NODE_KEY_INIT "ndl_"
#define LOGICAL_DEVICE_NODE_KEY_INIT "ldn_"
#define DATA_ATTR_NODE_KEY_INIT "dat_"
#define DATASET_NODE_KEY_INIT "dts_"
#define DATASET_ELEMENT_NODE_KEY_INIT "dse_"
#define BRCB_FOLDER_NODE_KEY "brcbfolder"
#define URCB_FOLDER_NODE_KEY "urcbfolder"
#define URCB_NODE_KEY_INIT "urc_"
#define BRCB_NODE_KEY_INIT "brc_"
#define DATASET_FOLDER_NODE_KEY "datasets"
#define DATASET_PREP_FOLDER_NODE_KEY "dataset_prep"
#define DS_OBJ_REF_NODE_KEY "objref"
#define DA_VALUE_REF_NODE_KEY "Value"
#define DA_OBJECT_REF_NODE_KEY "Object"

struct s61850Server{
    char hostname[MAX_IEC61850_SERVER_HOSTNAME];
    int tcpPort;
    IedConnection iedConn;
};

typedef struct s61850Server* ts61850Server;

extern Map *serverMap;

/*
 * Function declarations
 */
ts61850Server iec61850ServerCreate();
void iec61850ServerDestroy(ts61850Server self);
bool isWritableFC(FunctionalConstraint fc);
bool isWritableFCWithString(const char* fcStr);
FunctionalConstraint getFunctionalConstraint(const char* daRef, char *fcStr);
char *getMmsTypeString(MmsType _mmsType);
bool isMmsTypeFromString(const char* mmsTypeStr, MmsType _mmsType);
char *getFCExplainString(FunctionalConstraint _fc);

char *createBitStringArray(int bitStringSize);
void destroyBitStringArray(char* self);
/*
 * returns 0 if fails
 * returns # of bits if ok
 */
int parseBitString(const char *rawString, char *bitStrBuf);

DSLink *getDSLink(DSNode *node);
DSNode *getServerNode(DSNode *node);
IedConnection getServerIedConnection(DSNode *node);

#ifdef __cplusplus
}
#endif

#endif //SDK_DSLINK_C_IEC61850_SERVER_DEFS_H
