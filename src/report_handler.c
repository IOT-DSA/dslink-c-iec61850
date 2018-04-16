//
// Created by ali on 21.11.2016.
//


#define LOG_TAG "report_handler"


#include <dslink/log.h>
#include <stdbool.h>
#include <string.h>
#include "iec61850_server_defs.h"
#include "report_handler.h"
#include "dataset_handler.h"


void convertRefToNodeKeyString(char *rcbRef) {
    char *slashPtr = NULL;
    slashPtr = strchr(rcbRef, '/');
    while(slashPtr) {
        *slashPtr = '$';
        slashPtr = strchr(rcbRef, '/');
    }
}
void convertNodeKeyToRefString(char *rcbRef) {
    char *slashPtr = NULL;
    slashPtr = strchr(rcbRef, '$');
    while(slashPtr) {
        *slashPtr = '/';
        slashPtr = strchr(rcbRef, '$');
    }
}
void convertDataSetRef(char *dataSetRef) {
    char *pointPtr = NULL;
    pointPtr = strchr(dataSetRef, '.');
    while(pointPtr) {
        *pointPtr = '$';
        pointPtr = strchr(dataSetRef, '.');
    }
}
void convertDataSetRefToNormal(char *rcbDataSetRef) {
    char *pointPtr = NULL;
    pointPtr = strchr(rcbDataSetRef, '$');
    while(pointPtr) {
        *pointPtr = '.';
        pointPtr = strchr(rcbDataSetRef, '$');
    }
}
bool getDataSetListEnumStrForRCB(DSNode *node, char *enumStr) {
    DSNode *mainDataSetNode = getMainDatasetNode(node);
    if(mainDataSetNode) {
        int i=0;
        dslink_map_foreach(mainDataSetNode->children) {

            DSNode *tempNode = (DSNode *) (entry->value->data);

            if (strncmp(tempNode->name, DATASET_NODE_KEY_INIT, NODE_KEY_INIT_LEN) == 0) {
                i++;
                char *currPtr = enumStr + strlen(enumStr);
                strcat(enumStr,json_string_value(dslink_node_get_path(tempNode,DS_OBJ_REF_NODE_KEY)->value));
                convertDataSetRef(currPtr);
                strcat(enumStr,",");
            }
        }
        if(i>0) {
            enumStr[strlen(enumStr)-1] = ']';
            return true;
        }

    }
    strcat(enumStr,"]");
    return false;
}

DSNode *getURCBFolderNode(DSNode *node) {
    DSNode *serverNode = getServerNode(node);
    DSNode *rcbFolderNode = NULL;
    if(serverNode) {
        rcbFolderNode = dslink_node_get_path(serverNode,URCB_FOLDER_NODE_KEY);
        if(!rcbFolderNode) {
            log_info("URCB Folder node couldn't be found!\n");
        }
    }

    return rcbFolderNode;
}
DSNode *getBRCBFolderNode(DSNode *node) {
    DSNode *serverNode = getServerNode(node);
    DSNode *rcbFolderNode = NULL;
    if(serverNode) {
        rcbFolderNode = dslink_node_get_path(serverNode,BRCB_FOLDER_NODE_KEY);
        if(!rcbFolderNode) {
            log_info("BRCB Folder node couldn't be found!\n");
        }
    }

    return rcbFolderNode;
}

char *getRCBOptFldsString(int optFlds, char *optFldsStr) {
    if(optFlds == 0)
        strcpy(optFldsStr,"NONE");
    else {
        strcpy(optFldsStr,"");
        if (optFlds & RPT_OPT_SEQ_NUM)
            strcat(optFldsStr, "SEQ_NUM,");
        if (optFlds & RPT_OPT_BUFFER_OVERFLOW)
            strcat(optFldsStr, "BUFFER_OVERFLOW,");
        if (optFlds & RPT_OPT_CONF_REV)
            strcat(optFldsStr, "CONF_REV,");
        if (optFlds & RPT_OPT_DATA_REFERENCE)
            strcat(optFldsStr, "DATA_REFERENCE,");
        if (optFlds & RPT_OPT_DATA_SET)
            strcat(optFldsStr, "DATA_SET,");
        if (optFlds & RPT_OPT_ENTRY_ID)
            strcat(optFldsStr, "ENTRY_ID,");
        if (optFlds & RPT_OPT_REASON_FOR_INCLUSION)
            strcat(optFldsStr, "REASON_FOR_INCLUSION,");
        if (optFlds & RPT_OPT_TIME_STAMP)
            strcat(optFldsStr, "TIME_STAMP,");
        optFldsStr[strlen(optFldsStr)-1] = '\0';
    }
    return optFldsStr;
}

char *getRCBTrgOpsString(int trgOps, char *trgOpsStr) {
    if(trgOps == 0)
        strcpy(trgOpsStr,"NONE");
    else {
        strcpy(trgOpsStr,"");
        if(trgOps & TRG_OPT_DATA_CHANGED)
            strcat(trgOpsStr,"DATA_CHANGED,");
        if(trgOps & TRG_OPT_DATA_UPDATE)
            strcat(trgOpsStr,"DATA_UPDATE,");
        if(trgOps & TRG_OPT_GI)
            strcat(trgOpsStr,"GI,");
        if(trgOps & TRG_OPT_INTEGRITY)
            strcat(trgOpsStr,"INTEGRITY,");
        if(trgOps & TRG_OPT_QUALITY_CHANGED)
            strcat(trgOpsStr,"QUALITY_CHANGED,");
        trgOpsStr[strlen(trgOpsStr)-1] = '\0';
    }
    return trgOpsStr;
}

DSNode* createSubNodes(DSLink *link, DSNode *mainNode, const char* key, const char* name, const char* type, json_t *value) {
    DSNode *subNode = dslink_node_create(mainNode, key, "node");
    if (!subNode) {
        log_warn("Failed to create the object reference node\n");
        return NULL;
    }
    if (dslink_node_set_meta(link, subNode, "$name", json_string(name)) != 0) {
        log_warn("Failed to set the type on the status\n");
        dslink_node_tree_free(link, subNode);
        return NULL;
    }
    if (dslink_node_set_meta(link, subNode, "$type", json_string(type)) != 0) {
        log_warn("Failed to set the type on the status\n");
        dslink_node_tree_free(link, subNode);
        return NULL;
    }
    if (dslink_node_set_value(link, subNode, value) != 0) {
        log_warn("Failed to set the value on the status\n");
        dslink_node_tree_free(link, subNode);
        return NULL;
    }
    if (dslink_node_add_child(link, subNode) != 0) {
        log_warn("Failed to add the status node to the root\n");
        dslink_node_tree_free(link, subNode);
        return NULL;
    }

    return subNode;
}
void rcbDeleter(void* ptr) {
    ClientReportControlBlock rcb = (ClientReportControlBlock)ptr; 
    ClientReportControlBlock_destroy(rcb);
}
void setNodeRCBValues(DSLink *link, DSNode *rcbNode, ClientReportControlBlock rcb) {
    if (dslink_node_set_value(link, dslink_node_get_path(rcbNode,RCB_RPT_ID_KEY), json_string(ClientReportControlBlock_getRptId(rcb))) != 0) {
        log_warn("Failed to set the value on the rcb node\n");
    }
    if (dslink_node_set_value(link, dslink_node_get_path(rcbNode,RCB_RPT_ENA_KEY), json_boolean(ClientReportControlBlock_getRptEna(rcb))) != 0) {
        log_warn("Failed to set the value on the rcb node\n");
    }
    if (dslink_node_set_value(link, dslink_node_get_path(rcbNode,RCB_DATASET_KEY), json_string(ClientReportControlBlock_getDataSetReference(rcb))) != 0) {
        log_warn("Failed to set the value on the rcb node\n");
    }
    if (dslink_node_set_value(link, dslink_node_get_path(rcbNode,RCB_CONF_REV_KEY), json_integer(ClientReportControlBlock_getConfRev(rcb))) != 0) {
        log_warn("Failed to set the value on the rcb node\n");
    }
    if (dslink_node_set_value(link, dslink_node_get_path(rcbNode,RCB_BUF_TM_KEY), json_integer(ClientReportControlBlock_getBufTm(rcb))) != 0) {
        log_warn("Failed to set the value on the rcb node\n");
    }
    if (dslink_node_set_value(link, dslink_node_get_path(rcbNode,RCB_INTG_PD_KEY), json_integer(ClientReportControlBlock_getIntgPd(rcb))) != 0) {
        log_warn("Failed to set the value on the rcb node\n");
    }
    char optFldsStr[MAX_OPT_FLDS_STR_LEN];
    if (dslink_node_set_value(link, dslink_node_get_path(rcbNode,RCB_OPT_FLDS_KEY), json_string(getRCBOptFldsString(ClientReportControlBlock_getOptFlds(rcb),optFldsStr))) != 0) {
        log_warn("Failed to set the value on the rcb node\n");
    }
    char trgOpsStr[MAX_TRG_OPS_STR_LEN];
    if (dslink_node_set_value(link, dslink_node_get_path(rcbNode,RCB_TRG_OPS_KEY), json_string(getRCBTrgOpsString(ClientReportControlBlock_getTrgOps(rcb),trgOpsStr))) != 0) {
        log_warn("Failed to set the value on the rcb node\n");
    }
}
static
void readRCBAction(DSLink *link, DSNode *node,
                           json_t *rid, json_t *params, ref_t *stream) {

    (void)rid;
    (void)params;
    (void)stream;

    IedConnection iedConn = getServerIedConnection(node);
    DSNode *rcbNode = node->parent;
    const char *rcbRef = json_string_value(dslink_node_get_path(rcbNode,RCB_OBJ_REF_KEY)->value);
    ClientReportControlBlock rcb = rcbNode->data->data;

    if(iedConn && rcbRef && rcb ) {
        IedClientError error;

        IedConnection_getRCBValues(iedConn, &error, rcbRef, rcb);
        if (error != IED_ERROR_OK) {
            log_info("getRCBValues service error!\n");
        } else {
            setNodeRCBValues(link,rcbNode,rcb);
        }

    }

}
static
void forceGIRCBAction(DSLink *link, DSNode *node,
                   json_t *rid, json_t *params, ref_t *stream) {
  (void)link;
  (void)rid;
  (void)params;
  (void)stream;


    IedConnection iedConn = getServerIedConnection(node);
    DSNode *rcbNode = node->parent;
    const char *rcbRef = json_string_value(dslink_node_get_path(rcbNode,RCB_OBJ_REF_KEY)->value);
//    log_info("Force GI: %s\n",rcbRef);
    ClientReportControlBlock rcb = rcbNode->data->data;

    if(iedConn && rcbRef && rcb ) {
        IedClientError error;

        IedConnection_triggerGIReport(iedConn, &error, rcbRef);
        if (error != IED_ERROR_OK) {
            log_warn("Error triggering a GI report (code: %i)\n", error);
        }

    }

}
static
void setRptEnaAction(DSLink *link, DSNode *node,
                     json_t *rid, json_t *params, ref_t *stream) {

    (void)rid;
    (void)params;
    (void)stream;

    bool boolValue = json_boolean_value(json_object_get(params, DA_VALUE_REF_NODE_KEY));
    IedConnection iedConn = getServerIedConnection(node);
    DSNode *rcbNode = node->parent->parent;
    const char *rcbRef = json_string_value(dslink_node_get_path(rcbNode,RCB_OBJ_REF_KEY)->value);
    ClientReportControlBlock rcb = rcbNode->data->data;

    if(iedConn && rcbRef && rcb ) {
        IedClientError error;
        //log_info("RCB node: %s %d\n",rcbRef, boolValue);
        ClientReportControlBlock_setRptEna(rcb,boolValue);
        if (dslink_node_set_value(link, dslink_node_get_path(rcbNode,RCB_RPT_ENA_KEY), json_boolean(boolValue)) != 0) {
            log_warn("Failed to set the value on the rcb node\n");
        }

        IedConnection_setRCBValues(iedConn,&error,rcb,RCB_ELEMENT_RPT_ENA,true);
        if (error != IED_ERROR_OK) {
            log_info("setRCBValues service error: %d!\n",error);
            IedConnection_getRCBValues(iedConn, &error, rcbRef, rcb);
            setNodeRCBValues(link,rcbNode,rcb);
        }
    }
}
static
void setRptIdAction(DSLink *link, DSNode *node,
                     json_t *rid, json_t *params, ref_t *stream) {

    (void)rid;
    (void)params;
    (void)stream;

    const char *entValue = json_string_value(json_object_get(params, DA_VALUE_REF_NODE_KEY));
    IedConnection iedConn = getServerIedConnection(node);
    DSNode *rcbNode = node->parent->parent;
    const char *rcbRef = json_string_value(dslink_node_get_path(rcbNode,RCB_OBJ_REF_KEY)->value);
    ClientReportControlBlock rcb = rcbNode->data->data;

    if(iedConn && rcbRef && rcb ) {
        IedClientError error;
#ifdef DEVEL_DEBUG
        log_info("RCB node: %s %s\n",rcbRef, entValue);
#endif
        ClientReportControlBlock_setRptId(rcb,entValue);
        if (dslink_node_set_value(link, dslink_node_get_path(rcbNode,RCB_RPT_ID_KEY), json_string(entValue)) != 0) {
            log_warn("Failed to set the value on the rcb node\n");
        }

        IedConnection_setRCBValues(iedConn,&error,rcb,RCB_ELEMENT_RPT_ID,true);
        if (error != IED_ERROR_OK) {
            log_info("setRCBValues service error: %d!\n",error);
            IedConnection_getRCBValues(iedConn, &error, rcbRef, rcb);
            setNodeRCBValues(link,rcbNode,rcb);
        }
    }
}
static
void setDatSetAction(DSLink *link, DSNode *node,
                    json_t *rid, json_t *params, ref_t *stream) {

    (void)rid;
    (void)params;
    (void)stream;

    const char *entValue = json_string_value(json_object_get(params, DA_VALUE_REF_NODE_KEY));
    IedConnection iedConn = getServerIedConnection(node);
    DSNode *rcbNode = node->parent->parent;
    const char *rcbRef = json_string_value(dslink_node_get_path(rcbNode,RCB_OBJ_REF_KEY)->value);
    ClientReportControlBlock rcb = rcbNode->data->data;

    if(iedConn && rcbRef && rcb ) {
        IedClientError error;
        //log_info("RCB node: %s %s\n",rcbRef, entValue);
        ClientReportControlBlock_setDataSetReference(rcb,entValue);
        if (dslink_node_set_value(link, dslink_node_get_path(rcbNode,RCB_DATASET_KEY), json_string(entValue)) != 0) {
            log_warn("Failed to set the value on the rcb node\n");
        }

        IedConnection_setRCBValues(iedConn,&error,rcb,RCB_ELEMENT_DATSET,true);
        if (error != IED_ERROR_OK) {
            log_info("setRCBValues service error: %d!\n",error);
            IedConnection_getRCBValues(iedConn, &error, rcbRef, rcb);
            setNodeRCBValues(link,rcbNode,rcb);
        }
    }
}
static
void rcbDatSetNodeOpenAction(DSLink *link, DSNode *node) {
    (void) link;

    char dataSetListEnumStr[1000] = "enum[";
    if(!getDataSetListEnumStrForRCB(node,dataSetListEnumStr)) {
        log_info("No Data Set!\n");
    }

    //log_info("dataset enum: %s\n",dataSetListEnumStr);

    DSNode *setNode = dslink_node_get_path(node,"set");
    if(setNode) {
        json_t *params = json_array();
        json_t *ds_row = json_object();
        json_object_set_new(ds_row, "name", json_string(DA_VALUE_REF_NODE_KEY));
        json_object_set_new(ds_row, "type", json_string(dataSetListEnumStr));
        json_array_append_new(params, ds_row);

        dslink_node_set_meta(link, setNode, "$params", params);
    }

}
static
void setBufTmAction(DSLink *link, DSNode *node,
                    json_t *rid, json_t *params, ref_t *stream) {

    (void)rid;
    (void)params;
    (void)stream;

    uint32_t entValue = (uint32_t)json_integer_value(json_object_get(params, DA_VALUE_REF_NODE_KEY));
    IedConnection iedConn = getServerIedConnection(node);
    DSNode *rcbNode = node->parent->parent;
    const char *rcbRef = json_string_value(dslink_node_get_path(rcbNode,RCB_OBJ_REF_KEY)->value);
    ClientReportControlBlock rcb = rcbNode->data->data;

    if(iedConn && rcbRef && rcb ) {
        IedClientError error;
#ifdef DEVEL_DEBUG
        log_info("RCB node: %s %u\n",rcbRef, entValue);
#endif

        ClientReportControlBlock_setBufTm(rcb,entValue);
        if (dslink_node_set_value(link, dslink_node_get_path(rcbNode,RCB_BUF_TM_KEY), json_integer(entValue)) != 0) {
            log_warn("Failed to set the value on the rcb node\n");
        }

        IedConnection_setRCBValues(iedConn,&error,rcb,RCB_ELEMENT_BUF_TM,true);
        if (error != IED_ERROR_OK) {
            log_info("setRCBValues service error: %d!\n",error);
            IedConnection_getRCBValues(iedConn, &error, rcbRef, rcb);
            setNodeRCBValues(link,rcbNode,rcb);
        }
    }
}
static
void setIntgPdAction(DSLink *link, DSNode *node,
                    json_t *rid, json_t *params, ref_t *stream) {

    (void)rid;
    (void)params;
    (void)stream;

    uint32_t entValue = (uint32_t)json_integer_value(json_object_get(params, DA_VALUE_REF_NODE_KEY));
    IedConnection iedConn = getServerIedConnection(node);
    DSNode *rcbNode = node->parent->parent;
    const char *rcbRef = json_string_value(dslink_node_get_path(rcbNode,RCB_OBJ_REF_KEY)->value);
    ClientReportControlBlock rcb = rcbNode->data->data;

    if(iedConn && rcbRef && rcb ) {
        IedClientError error;
#ifdef DEVEL_DEBUG
        log_info("RCB node: %s %u\n",rcbRef, entValue);
#endif
        ClientReportControlBlock_setIntgPd(rcb,entValue);
        if (dslink_node_set_value(link, dslink_node_get_path(rcbNode,RCB_INTG_PD_KEY), json_integer(entValue)) != 0) {
            log_warn("Failed to set the value on the rcb node\n");
        }

        IedConnection_setRCBValues(iedConn,&error,rcb,RCB_ELEMENT_INTG_PD,true);
        if (error != IED_ERROR_OK) {
            log_info("setRCBValues service error: %d!\n",error);
            IedConnection_getRCBValues(iedConn, &error, rcbRef, rcb);
            setNodeRCBValues(link,rcbNode,rcb);
        }
    }
}
static
void setOptFldsAction(DSLink *link, DSNode *node,
                     json_t *rid, json_t *params, ref_t *stream) {

    (void)rid;
    (void)params;
    (void)stream;

    int optFlds = 0;
    if(json_boolean_value(json_object_get(params, "SEQ_NUM")))
        optFlds |= RPT_OPT_SEQ_NUM;
    if(json_boolean_value(json_object_get(params, "TIME_STAMP")))
        optFlds |= RPT_OPT_TIME_STAMP;
    if(json_boolean_value(json_object_get(params, "REASON_FOR_INCLUSION")))
        optFlds |= RPT_OPT_REASON_FOR_INCLUSION;
    if(json_boolean_value(json_object_get(params, "ENTRY_ID")))
        optFlds |= RPT_OPT_ENTRY_ID;
    if(json_boolean_value(json_object_get(params, "DATA_SET")))
        optFlds |= RPT_OPT_DATA_SET;
    if(json_boolean_value(json_object_get(params, "BUFFER_OVERFLOW")))
        optFlds |= RPT_OPT_BUFFER_OVERFLOW;
    if(json_boolean_value(json_object_get(params, "CONF_REV")))
        optFlds |= RPT_OPT_CONF_REV;
    if(json_boolean_value(json_object_get(params, "DATA_REFERENCE")))
        optFlds |= RPT_OPT_DATA_REFERENCE;

    IedConnection iedConn = getServerIedConnection(node);
    DSNode *rcbNode = node->parent->parent;
    const char *rcbRef = json_string_value(dslink_node_get_path(rcbNode,RCB_OBJ_REF_KEY)->value);
    ClientReportControlBlock rcb = rcbNode->data->data;

    if(iedConn && rcbRef && rcb ) {
        IedClientError error;
        //log_info("RCB node: %s %d\n",rcbRef, boolValue);
        ClientReportControlBlock_setOptFlds(rcb,optFlds);

        char optFldsStr[MAX_OPT_FLDS_STR_LEN];
        if (dslink_node_set_value(link, dslink_node_get_path(rcbNode,RCB_OPT_FLDS_KEY), json_string(getRCBOptFldsString(optFlds,optFldsStr))) != 0) {
            log_warn("Failed to set the value on the rcb node\n");
        }

        IedConnection_setRCBValues(iedConn,&error,rcb,RCB_ELEMENT_OPT_FLDS,true);
        if (error != IED_ERROR_OK) {
            log_info("setRCBValues service error: %d!\n",error);
            IedConnection_getRCBValues(iedConn, &error, rcbRef, rcb);
            setNodeRCBValues(link,rcbNode,rcb);
        }
    }
}
static
void setTrgOpsAction(DSLink *link, DSNode *node,
                      json_t *rid, json_t *params, ref_t *stream) {

    (void)rid;
    (void)params;
    (void)stream;

    int trgOps = 0;
    if(json_boolean_value(json_object_get(params, "QUALITY_CHANGED")))
        trgOps |= TRG_OPT_QUALITY_CHANGED;
    if(json_boolean_value(json_object_get(params, "INTEGRITY")))
        trgOps |= TRG_OPT_INTEGRITY;
    if(json_boolean_value(json_object_get(params, "GI")))
        trgOps |= TRG_OPT_GI;
    if(json_boolean_value(json_object_get(params, "DATA_UPDATE")))
        trgOps |= TRG_OPT_DATA_UPDATE;
    if(json_boolean_value(json_object_get(params, "DATA_CHANGED")))
        trgOps |= TRG_OPT_DATA_CHANGED;

    IedConnection iedConn = getServerIedConnection(node);
    DSNode *rcbNode = node->parent->parent;
    const char *rcbRef = json_string_value(dslink_node_get_path(rcbNode,RCB_OBJ_REF_KEY)->value);
    ClientReportControlBlock rcb = rcbNode->data->data;

    if(iedConn && rcbRef && rcb ) {
        IedClientError error;
        //log_info("RCB node: %s %d\n",rcbRef, boolValue);
        ClientReportControlBlock_setTrgOps(rcb,trgOps);


        char trgOpsStr[MAX_TRG_OPS_STR_LEN];
        if (dslink_node_set_value(link, dslink_node_get_path(rcbNode,RCB_TRG_OPS_KEY), json_string(getRCBTrgOpsString(trgOps,trgOpsStr))) != 0) {
            log_warn("Failed to set the value on the rcb node\n");
        }
        IedConnection_setRCBValues(iedConn,&error,rcb,RCB_ELEMENT_TRG_OPS,true);
        if (error != IED_ERROR_OK) {
            log_info("setRCBValues service error: %d!\n",error);
            IedConnection_getRCBValues(iedConn, &error, rcbRef, rcb);
            setNodeRCBValues(link,rcbNode,rcb);
        }
    }
}

DSNode *getRCBInfoAndCreateNode(DSLink *link, DSNode *currNode, IedConnection iedConn, const char* rcbRef, bool isBuffered) {
    DSNode *rcbMainFolderNode;
    char rcbNodeKey[200];
    IedClientError error;
    char typeString[50];

    if(isBuffered) {
        strcpy(typeString,"Buffered report control block (BRCB)");
        strcpy(rcbNodeKey, BRCB_NODE_KEY_INIT);
        rcbMainFolderNode = getBRCBFolderNode(currNode);
    }
    else {
        strcpy(typeString,"Unbuffered report control block (URCB)");
        strcpy(rcbNodeKey, URCB_NODE_KEY_INIT);
        rcbMainFolderNode = getURCBFolderNode(currNode);
    }
    strcat(rcbNodeKey, rcbRef);
    convertRefToNodeKeyString(rcbNodeKey);

    /* Read RCB values */
    ClientReportControlBlock rcb =
            IedConnection_getRCBValues(iedConn, &error, rcbRef, NULL);
    if (error != IED_ERROR_OK) {
        log_info("getRCBValues service error!\n");

    }



    if(rcbMainFolderNode && (error == IED_ERROR_OK)) {
        DSNode *newRCBNode = dslink_node_create(rcbMainFolderNode, rcbNodeKey, "node");
        if (!newRCBNode) {
            log_warn("Failed to create the RCB node\n");
            return NULL;
        }
        if (dslink_node_set_meta(link, newRCBNode, "$name", json_string(rcbRef)) != 0) {
            log_warn("Failed to set the name of RCB\n");
            dslink_node_tree_free(link, newRCBNode);
            return NULL;
        }
        if (dslink_node_add_child(link, newRCBNode) != 0) {
            log_warn("Failed to add the RCB node to the root\n");
            dslink_node_tree_free(link, newRCBNode);
            return NULL;
        }
        newRCBNode->data = dslink_ref(rcb,rcbDeleter);
//        /* read data set directory */
//        LinkedList dataSetDirectory =
//                IedConnection_getDataSetDirectory(iedConn, &error, ClientReportControlBlock_getDataSetReference(rcb) , NULL);
//
//        /* Configure the report receiver */
//        IedConnection_installReportHandler(iedConn, rcbRef, ClientReportControlBlock_getRptId(rcb), reportCallbackFunction,
//                                           (void*) dataSetDirectory);

        IedConnection_installReportHandler(iedConn, rcbRef, ClientReportControlBlock_getRptId(rcb), reportCallbackFunction,
                                           (void*) newRCBNode);


        /*
        * create Obj Ref node
        */
        if(!createSubNodes(link,newRCBNode,RCB_OBJ_REF_KEY,"Obj. Ref.","string",json_string(rcbRef))){
            log_warn("RCB Obj. Ref. node couldn't be created\n");
        }

        /*
        * create Type node
        */
        if(!createSubNodes(link,newRCBNode,RCB_TYPE_KEY,"Type","string",json_string(typeString))){
            log_warn("RCB Type node couldn't be created\n");
        }

        /*
        * create Rpt Id node
        */
        DSNode *rptIdNode = createSubNodes(link,newRCBNode,RCB_RPT_ID_KEY,"RptId","string",json_string(ClientReportControlBlock_getRptId(rcb)));
        if(!rptIdNode){
            log_warn("RCB RptId node couldn't be created\n");
        } else {
            DSNode *setNode = dslink_node_create(rptIdNode, "set", "node");
            if (!setNode) {
                log_warn("Failed to create control node\n");
            } else {

                setNode->on_invocation = setRptIdAction;
                dslink_node_set_meta(link, setNode, "$name", json_string("Set"));
                dslink_node_set_meta(link, setNode, "$invokable", json_string("read"));

                json_t *params = json_array();
                json_t *valueRow = json_object();
                json_object_set_new(valueRow, "name", json_string(DA_VALUE_REF_NODE_KEY));
                json_object_set_new(valueRow, "type", json_string("string"));
                json_array_append_new(params, valueRow);
                dslink_node_set_meta(link, setNode, "$params", params);

                if (dslink_node_add_child(link, setNode) != 0) {
                    log_warn("Failed to add set node action to the rcb node\n");
                    dslink_node_tree_free(link, setNode);
                }
            }
        }

        /*
        * create RptEna node
        */
        DSNode *rptEnaNode = createSubNodes(link,newRCBNode,RCB_RPT_ENA_KEY,"RptEna","boolean",json_boolean(ClientReportControlBlock_getRptEna(rcb)));
        if(!rptEnaNode){
            log_warn("RCB RptEna node couldn't be created\n");
        } else {
            DSNode *setNode = dslink_node_create(rptEnaNode, "set", "node");
            if (!setNode) {
                log_warn("Failed to create control node\n");
            } else {

                setNode->on_invocation = setRptEnaAction;
                dslink_node_set_meta(link, setNode, "$name", json_string("Set"));
                dslink_node_set_meta(link, setNode, "$invokable", json_string("read"));

                json_t *params = json_array();
                json_t *valueRow = json_object();
                json_object_set_new(valueRow, "name", json_string(DA_VALUE_REF_NODE_KEY));
                json_object_set_new(valueRow, "type", json_string("bool"));
                json_array_append_new(params, valueRow);
                dslink_node_set_meta(link, setNode, "$params", params);

                if (dslink_node_add_child(link, setNode) != 0) {
                    log_warn("Failed to add set node action to the rcb node\n");
                    dslink_node_tree_free(link, setNode);
                }
            }
        }

        /*
        * create Dataset node
        */
        DSNode *datSetNode = createSubNodes(link,newRCBNode,RCB_DATASET_KEY,"DatSet","string",json_string(ClientReportControlBlock_getDataSetReference(rcb)));
        if(!datSetNode){
            log_warn("RCB DatSet node couldn't be created\n");
        } else {
            DSNode *setNode = dslink_node_create(datSetNode, "set", "node");
            if (!setNode) {
                log_warn("Failed to create control node\n");
            } else {

                datSetNode->on_list_open = rcbDatSetNodeOpenAction;
                setNode->on_invocation = setDatSetAction;
                dslink_node_set_meta(link, setNode, "$name", json_string("Set"));
                dslink_node_set_meta(link, setNode, "$invokable", json_string("read"));

                json_t *params = json_array();
                json_t *valueRow = json_object();
                json_object_set_new(valueRow, "name", json_string(DA_VALUE_REF_NODE_KEY));
                json_object_set_new(valueRow, "type", json_string("enum[]"));
                json_array_append_new(params, valueRow);
                dslink_node_set_meta(link, setNode, "$params", params);

                if (dslink_node_add_child(link, setNode) != 0) {
                    log_warn("Failed to add set node action to the rcb node\n");
                    dslink_node_tree_free(link, setNode);
                }
            }
        }

        /*
        * create Conf Rev node
        */
        if(!createSubNodes(link,newRCBNode,RCB_CONF_REV_KEY,"ConfRev","number",json_integer(ClientReportControlBlock_getConfRev(rcb)))){
            log_warn("RCB RptId node couldn't be created\n");
        }

        /*
        * create BufTm node
        */
        DSNode *bufTmNode = createSubNodes(link,newRCBNode,RCB_BUF_TM_KEY,"BufTm","number",json_integer(ClientReportControlBlock_getBufTm(rcb)));
        if(!bufTmNode){
            log_warn("RCB BufTm node couldn't be created\n");
        } else {
            DSNode *setNode = dslink_node_create(bufTmNode, "set", "node");
            if (!setNode) {
                log_warn("Failed to create control node\n");
            } else {

                setNode->on_invocation = setBufTmAction;
                dslink_node_set_meta(link, setNode, "$name", json_string("Set"));
                dslink_node_set_meta(link, setNode, "$invokable", json_string("read"));

                json_t *params = json_array();
                json_t *valueRow = json_object();
                json_object_set_new(valueRow, "name", json_string(DA_VALUE_REF_NODE_KEY));
                json_object_set_new(valueRow, "type", json_string("number"));
                json_array_append_new(params, valueRow);
                dslink_node_set_meta(link, setNode, "$params", params);

                if (dslink_node_add_child(link, setNode) != 0) {
                    log_warn("Failed to add set node action to the rcb node\n");
                    dslink_node_tree_free(link, setNode);
                }
            }
        }

        /*
        * create IntgPd node
        */
        DSNode *intgPdNode = createSubNodes(link,newRCBNode,RCB_INTG_PD_KEY,"IntgPd","number",json_integer(ClientReportControlBlock_getIntgPd(rcb)));
        if(!intgPdNode){
            log_warn("RCB IntgPd node couldn't be created\n");
        } else {
            DSNode *setNode = dslink_node_create(intgPdNode, "set", "node");
            if (!setNode) {
                log_warn("Failed to create control node\n");
            } else {

                setNode->on_invocation = setIntgPdAction;
                dslink_node_set_meta(link, setNode, "$name", json_string("Set"));
                dslink_node_set_meta(link, setNode, "$invokable", json_string("read"));

                json_t *params = json_array();
                json_t *valueRow = json_object();
                json_object_set_new(valueRow, "name", json_string(DA_VALUE_REF_NODE_KEY));
                json_object_set_new(valueRow, "type", json_string("number"));
                json_array_append_new(params, valueRow);
                dslink_node_set_meta(link, setNode, "$params", params);

                if (dslink_node_add_child(link, setNode) != 0) {
                    log_warn("Failed to add set node action to the rcb node\n");
                    dslink_node_tree_free(link, setNode);
                }
            }
        }

        /*
        * create OptFlds node
        */
        char optFldsStr[MAX_OPT_FLDS_STR_LEN];
        DSNode *optFldsNode = createSubNodes(link,newRCBNode,RCB_OPT_FLDS_KEY,"OptFlds","string", json_string(getRCBOptFldsString(ClientReportControlBlock_getOptFlds(rcb),optFldsStr)));
        if(!optFldsNode){
            log_warn("RCB OptFlds node couldn't be created\n");
        } else {
            DSNode *setNode = dslink_node_create(optFldsNode, "set", "node");
            if (!setNode) {
                log_warn("Failed to create control node\n");
            } else {

                setNode->on_invocation = setOptFldsAction;
                dslink_node_set_meta(link, setNode, "$name", json_string("Set"));
                dslink_node_set_meta(link, setNode, "$invokable", json_string("read"));

                json_t *params = json_array();
                json_t *value1Row = json_object();
                json_object_set_new(value1Row, "name", json_string("SEQ_NUM"));
                json_object_set_new(value1Row, "type", json_string("bool"));
                json_array_append_new(params, value1Row);
                json_t *value2Row = json_object();
                json_object_set_new(value2Row, "name", json_string("BUFFER_OVERFLOW"));
                json_object_set_new(value2Row, "type", json_string("bool"));
                json_array_append_new(params, value2Row);
                json_t *value3Row = json_object();
                json_object_set_new(value3Row, "name", json_string("CONF_REV"));
                json_object_set_new(value3Row, "type", json_string("bool"));
                json_array_append_new(params, value3Row);
                json_t *value4Row = json_object();
                json_object_set_new(value4Row, "name", json_string("DATA_REFERENCE"));
                json_object_set_new(value4Row, "type", json_string("bool"));
                json_array_append_new(params, value4Row);
                json_t *value5Row = json_object();
                json_object_set_new(value5Row, "name", json_string("DATA_SET"));
                json_object_set_new(value5Row, "type", json_string("bool"));
                json_array_append_new(params, value5Row);
                json_t *value6Row = json_object();
                json_object_set_new(value6Row, "name", json_string("ENTRY_ID"));
                json_object_set_new(value6Row, "type", json_string("bool"));
                json_array_append_new(params, value6Row);
                json_t *value7Row = json_object();
                json_object_set_new(value7Row, "name", json_string("REASON_FOR_INCLUSION"));
                json_object_set_new(value7Row, "type", json_string("bool"));
                json_array_append_new(params, value7Row);
                json_t *value8Row = json_object();
                json_object_set_new(value8Row, "name", json_string("TIME_STAMP"));
                json_object_set_new(value8Row, "type", json_string("bool"));
                json_array_append_new(params, value8Row);
                dslink_node_set_meta(link, setNode, "$params", params);

                if (dslink_node_add_child(link, setNode) != 0) {
                    log_warn("Failed to add set node action to the rcb node\n");
                    dslink_node_tree_free(link, setNode);
                }
            }
        }

        /*
        * create TrgOps node
        */
        char trgOpsStr[MAX_TRG_OPS_STR_LEN];
        DSNode *trgOpsNode = createSubNodes(link,newRCBNode,RCB_TRG_OPS_KEY,"TrgOps","string", json_string(getRCBTrgOpsString(ClientReportControlBlock_getTrgOps(rcb),trgOpsStr)));
        if(!trgOpsNode){
            log_warn("RCB TrgOps node couldn't be created\n");
        } else {
            DSNode *setNode = dslink_node_create(trgOpsNode, "set", "node");
            if (!setNode) {
                log_warn("Failed to create control node\n");
            } else {

                setNode->on_invocation = setTrgOpsAction;
                dslink_node_set_meta(link, setNode, "$name", json_string("Set"));
                dslink_node_set_meta(link, setNode, "$invokable", json_string("read"));

                json_t *params = json_array();
                json_t *value1Row = json_object();
                json_object_set_new(value1Row, "name", json_string("DATA_CHANGED"));
                json_object_set_new(value1Row, "type", json_string("bool"));
                json_array_append_new(params, value1Row);
                json_t *value2Row = json_object();
                json_object_set_new(value2Row, "name", json_string("DATA_UPDATE"));
                json_object_set_new(value2Row, "type", json_string("bool"));
                json_array_append_new(params, value2Row);
                json_t *value3Row = json_object();
                json_object_set_new(value3Row, "name", json_string("GI"));
                json_object_set_new(value3Row, "type", json_string("bool"));
                json_array_append_new(params, value3Row);
                json_t *value4Row = json_object();
                json_object_set_new(value4Row, "name", json_string("INTEGRITY"));
                json_object_set_new(value4Row, "type", json_string("bool"));
                json_array_append_new(params, value4Row);
                json_t *value5Row = json_object();
                json_object_set_new(value5Row, "name", json_string("QUALITY_CHANGED"));
                json_object_set_new(value5Row, "type", json_string("bool"));
                json_array_append_new(params, value5Row);

                dslink_node_set_meta(link, setNode, "$params", params);

                if (dslink_node_add_child(link, setNode) != 0) {
                    log_warn("Failed to add set node action to the rcb node\n");
                    dslink_node_tree_free(link, setNode);
                }
            }
        }

        /*
        * create read node
        * */
        DSNode *readNode = dslink_node_create(newRCBNode, "readrcb", "node");
        if (!readNode) {
            log_warn("Failed to create read rcb action node\n");
            return NULL;
        }

        readNode->on_invocation = readRCBAction;
        dslink_node_set_meta(link, readNode, "$name", json_string("Read RCB"));
        dslink_node_set_meta(link, readNode, "$invokable", json_string("read"));

        if (dslink_node_add_child(link, readNode) != 0) {
            log_warn("Failed to add read action to the node\n");
            dslink_node_tree_free(link, readNode);
            return NULL;
        }

        /*
        * create write node
        * */
        DSNode *forceGINode = dslink_node_create(newRCBNode, "forcegi_rcb", "node");
        if (!forceGINode) {
            log_warn("Failed to create force GI rcb action node\n");
            return NULL;
        }

        forceGINode->on_invocation = forceGIRCBAction;
        dslink_node_set_meta(link, forceGINode, "$name", json_string("Force GI"));
        dslink_node_set_meta(link, forceGINode, "$invokable", json_string("read"));

        if (dslink_node_add_child(link, forceGINode) != 0) {
            log_warn("Failed to add force GI action to the node\n");
            dslink_node_tree_free(link, forceGINode);
            return NULL;
        }


        return newRCBNode;
    }
    return NULL;
}
