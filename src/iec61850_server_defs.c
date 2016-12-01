//
// Created by ali on 25.09.2016.
//
#define LOG_TAG "iec61850_server_defs"

#include "iec61850_server_defs.h"
#include "lib_memory.h"
#include <dslink/log.h>


#define NUM_OF_MMS_TYPES 16
char *mmsTypeStrs[NUM_OF_MMS_TYPES] = {"MMS_ARRAY",
                                       "MMS_STRUCTURE",
                                       "MMS_BOOLEAN",
                                       "MMS_BIT_STRING",
                                       "MMS_INTEGER",
                                       "MMS_UNSIGNED",
                                       "MMS_FLOAT",
                                       "MMS_OCTET_STRING",
                                       "MMS_VISIBLE_STRING",
                                       "MMS_GENERALIZED_TIME",
                                       "MMS_BINARY_TIME",
                                       "MMS_BCD",
                                       "MMS_OBJ_ID",
                                       "MMS_STRING",
                                       "MMS_UTC_TIME",
                                       "MMS_DATA_ACCESS_ERROR"};
#define NUM_OF_FUNCTIONAL_CONSTRAINTS 18
char *fcExplains[NUM_OF_FUNCTIONAL_CONSTRAINTS] = {"Status information",
                                                   "Measurand - analog value",
                                                   "Setpoint",
                                                   "Substitution",
                                                   "Configuration",
                                                   "Description",
                                                   "Setting group",
                                                   "Setting group editable",
                                                   "Service response / Service tracking",
                                                   "Operate received",
                                                   "Blocking",
                                                   "Extended definition",
                                                   "Control",
                                                   "Unicast SV",
                                                   "Multicast SV",
                                                   "Unbuffered report",
                                                   "Buffered report",
                                                   "Log control block"};

ts61850Server
iec61850ServerCreate() {
    ts61850Server self = (ts61850Server) GLOBAL_CALLOC(1, sizeof(struct s61850Server));

    return self;
}

IedConnection getServerIedConnection(DSNode *node) {
//    while(node->parent->parent)
//        node = node->parent;

    node = getServerNode(node);
    if (node) {
        ts61850Server serverPtr;
        if (serverPtr = (ts61850Server) (dslink_map_get(serverMap, node)->data)) {
            return serverPtr->iedConn;
        }
    }

    return NULL;
}

void
iec61850ServerDestroy(ts61850Server self) {
    IedConnection_destroy(self->iedConn);

    GLOBAL_FREEMEM(self);
}

bool isWritableFC(FunctionalConstraint fc) {
    if ((fc == IEC61850_FC_CF) ||
        (fc == IEC61850_FC_SE) ||
        (fc == IEC61850_FC_SP) ||
        (fc == IEC61850_FC_DC) ||
        (fc == IEC61850_FC_SV)) {
        return true;
    }

    return false;
}

bool isWritableFCWithString(const char *fcStr) {
    if (!strncmp(fcStr, "CF", 2) ||
        !strncmp(fcStr, "SE", 2) ||
        !strncmp(fcStr, "SP", 2) ||
        !strncmp(fcStr, "DC", 2) ||
        !strncmp(fcStr, "SV", 2)) {
        return true;
    }

    return false;
}

char *getMmsTypeString(MmsType _mmsType) {
    if (_mmsType < NUM_OF_MMS_TYPES)
        return mmsTypeStrs[_mmsType];
    else
        return NULL;
}

bool isMmsTypeFromString(const char *mmsTypeStr, MmsType _mmsType) {

    if (_mmsType < NUM_OF_MMS_TYPES) {
        return (strcmp(mmsTypeStr, mmsTypeStrs[_mmsType]) == 0);
    } else
        return false;
}

char *getFCExplainString(FunctionalConstraint _fc) {
    if (_fc < NUM_OF_FUNCTIONAL_CONSTRAINTS)
        return fcExplains[_fc];
    else
        return NULL;
}

FunctionalConstraint getFunctionalConstraint(const char *daRef, char *fcStr) {
    char *fcPtr = strchr(daRef, '[');

    if (fcPtr) {
        fcPtr = fcPtr + 1;
        strncpy(fcStr, fcPtr, 2);
        fcStr[2] = '\0';
//        if (!strncmp(fcPtr, "ST", 2)) {
//            return IEC61850_FC_ST;
//        } else if (!strncmp(fcPtr, "MX", 2)) {
//            return IEC61850_FC_MX;
//        } else if (!strncmp(fcPtr, "SP", 2)) {
//            return IEC61850_FC_SP;
//        } else if (!strncmp(fcPtr, "SV", 2)) {
//            return IEC61850_FC_SV;
//        } else if (!strncmp(fcPtr, "CF", 2)) {
//            return IEC61850_FC_CF;
//        } else if (!strncmp(fcPtr, "DC", 2)) {
//            return IEC61850_FC_DC;
//        } else if (!strncmp(fcPtr, "SG", 2)) {
//            return IEC61850_FC_SG;
//        } else if (!strncmp(fcPtr, "SE", 2)) {
//            return IEC61850_FC_SE;
//        } else if (!strncmp(fcPtr, "SR", 2)) {
//            return IEC61850_FC_SR;
//        } else if (!strncmp(fcPtr, "OR", 2)) {
//            return IEC61850_FC_OR;
//        } else if (!strncmp(fcPtr, "BL", 2)) {
//            return IEC61850_FC_BL;
//        } else if (!strncmp(fcPtr, "EX", 2)) {
//            return IEC61850_FC_EX;
//        } else if (!strncmp(fcPtr, "CO", 2)) {
//            return IEC61850_FC_CO;
//        } else if (!strncmp(fcPtr, "US", 2)) {
//            return IEC61850_FC_US;
//        } else if (!strncmp(fcPtr, "MS", 2)) {
//            return IEC61850_FC_MS;
//        } else if (!strncmp(fcPtr, "RP", 2)) {
//            return IEC61850_FC_RP;
//        } else if (!strncmp(fcPtr, "BR", 2)) {
//            return IEC61850_FC_BR;
//        } else if (!strncmp(fcPtr, "LG", 2)) {
//            return IEC61850_FC_LG;
//        } else {
//            return IEC61850_FC_NONE;
//        }
        FunctionalConstraint_fromString(fcStr);
    } else
        return IEC61850_FC_NONE;
}

char *createBitStringArray(int bitStringSize) {
    char *self = (char *) GLOBAL_CALLOC(1, sizeof(char) * bitStringSize * 2);
    return self;
}


void destroyBitStringArray(char *self) {
    GLOBAL_FREEMEM(self);
}

int parseBitString(const char *rawString, char *bitStrBuf) {
    int rawStrSize = (strlen(rawString) + 1) / 2;
    int i = 0, j = 0;

    if (rawStrSize > 0) {
        bitStrBuf = (char *) GLOBAL_CALLOC(1, sizeof(char) * rawStrSize);

        while (rawString[i] != 0) {
            if (rawString[i] == '0') {
                bitStrBuf[j++] = 0;
            } else if (rawString[i] != '1') {
                bitStrBuf[j++] = 1;
            } else {
                GLOBAL_FREEMEM(bitStrBuf);
                return 0;
            }
            i++;
            if (rawString[i] == ',') {
                i++;
            } else if (rawString[i] == 0) {
                if (rawStrSize == j + 1)
                    return rawStrSize;
                else
                    return 0;
            } else {
                GLOBAL_FREEMEM(bitStrBuf);
                return 0;
            }

        }
    } else
        return 0;
}

DSLink *getDSLink(DSNode *node) {
    DSNode *serverNode = getServerNode(node);
    if (serverNode) {
        return (DSLink *) serverNode->data->data;
    }
    return NULL;
}

DSNode *getServerNode(DSNode *node) {
    while (node->parent->parent)
        node = node->parent;

    //TODO: check also if the key begins with SERVER_NODE_KEY_INIT
    if(strncmp(node->name,SERVER_NODE_KEY_INIT,NODE_KEY_INIT_LEN) == 0)
        return node;
    else
        return NULL;
}
