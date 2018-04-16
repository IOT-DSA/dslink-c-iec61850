//
// Created by ali on 20.09.2016.
//

#define LOG_TAG "client_handler"

#include <dslink/log.h>
#include <dslink/ws.h>
#include "iec61850_server_defs.h"
#include "dataset_handler.h"
#include "report_handler.h"
#include <mms_value.h>
#include <string_utilities.h>
#include "client_handler.h"
#include "server_status.h"
#include "time_iso8601.h"
#include "hal_thread.h"


void nodeValueRead(DSLink *link, DSNode *node);
Map *serverMap;

void printServerMap() {
    dslink_map_foreach(serverMap) {
        DSNode *n = (DSNode *) (entry->key->data);
        log_info("serverMap: %s\n", n->name);
    }
}

int
setServerMap(DSNode *node, void *serverPtr) {
    int ret;
    if (!serverMap) {

        serverMap = dslink_malloc(sizeof(Map));
        if (!serverMap) {
            log_warn("Ied Connection Map couldn't be initialized!\n");
            return DSLINK_ALLOC_ERR;
        }
        if (dslink_map_init(serverMap,
                            dslink_map_uint32_cmp,
                            dslink_map_uint32_key_len_cal,
                            dslink_map_hash_key) != 0) {
            dslink_free(serverMap);
            serverMap = NULL;
            log_warn("Ied Connection Map couldn't be initialized!\n");
            return DSLINK_ALLOC_ERR;
        }
    }
    if ( (ret = dslink_map_set(serverMap, dslink_ref(node, NULL), dslink_ref(serverPtr, NULL))) != 0 ) {
        log_warn("Ied Connection Map couldn't be set!\n");
        return ret;
    }

#ifdef DEVEL_DEBUG
    log_info("Ied Connection Map set successfully!\n");
#endif
    return 0;
}

static void commandTerminationHandler(void *parameter, ControlObjectClient connection)
{
  (void)parameter;
    LastApplError lastApplError = ControlObjectClient_getLastApplError(connection);

    // if lastApplError.error != 0 this indicates a CommandTermination-
    if (lastApplError.error != 0) {
        log_info("Received CommandTermination-.\n");
        log_info(" LastApplError: %i\n", lastApplError.error);
        log_info("      addCause: %i\n", lastApplError.addCause);
    }
    else {
        log_info("Received CommandTermination+.\n");
    }
}

void setControlStVal(DSLink *link, DSNode *node, const char* doRef) {
  (void) doRef;

    ref_t *tempRef;
    DSNode *currStValNode;
    DSNode *currValueNode;
    if ( (tempRef = dslink_map_get(node->children,"stVal[ST]")) != NULL ) 
    {
        currStValNode = (DSNode*)(tempRef->data);
        if ( (tempRef = dslink_map_get(currStValNode->children,DA_VALUE_REF_NODE_KEY)) != NULL )
        {
            currValueNode = (DSNode*)(tempRef->data);
            nodeValueRead(link,currValueNode);
        }
        else
        {
            log_warn("Error while getting stVal Value node\n");
            return;
        }
    }
    else
    {
        log_warn("Error while getting stVal node\n");
        return;
    }
}

static void controlAction(DSLink *link, DSNode *node,
                       json_t *rid, json_t *params, ref_t *stream) {
    (void)rid;
    (void)stream;

    ref_t *tempRef;
    DSNode *currRefNode;
    tempRef = dslink_map_get(node->parent->children,DA_OBJECT_REF_NODE_KEY);
    if(tempRef)
    {
        currRefNode = (DSNode*)(tempRef->data);
    }
    else
    {
        log_warn("Error while getting Object node\n");
        return;
    }
    const char* currObjRef = json_string_value(currRefNode->value);


    tempRef = dslink_map_get(node->parent->children,"stVal[ST]");
    if(tempRef)
    {
        currRefNode = (DSNode*)(tempRef->data);
    }
    else
    {
        log_warn("Error while getting stVal[ST] node under Control Node\n");
        return;
    }
    tempRef = dslink_map_get(currRefNode->children,"MMS Type");
    if(tempRef)
    {
        currRefNode = (DSNode*)(tempRef->data);
    }
    else
    {
        log_warn("Error while getting MMS Type node\n");
        return;
    }
    const char *currMmsTypeStr = json_string_value(currRefNode->value);

#ifdef DEVEL_DEBUG
    log_info("Control action: %s mms:%s\n",currObjRef,currMmsTypeStr);
#endif

    IedConnection iedConn = getServerIedConnection(node);
    if(iedConn) {


        ControlObjectClient control
                = ControlObjectClient_create(currObjRef, iedConn);
        MmsValue *ctlVal;

        //check mms type
        if(isMmsTypeFromString(currMmsTypeStr,MMS_BOOLEAN)) {
            ctlVal = MmsValue_newBoolean(json_boolean_value(json_object_get(params, DA_VALUE_REF_NODE_KEY)));
        } else if(isMmsTypeFromString(currMmsTypeStr,MMS_INTEGER)) {
            ctlVal = MmsValue_newInteger(32);
            MmsValue_setInt32(ctlVal,(int32_t)json_integer_value(json_object_get(params, DA_VALUE_REF_NODE_KEY)));
        } else {
            log_warn("Unhandled control type\n");
            return;
        }

        if(ControlObjectClient_getControlModel(control) == CONTROL_MODEL_DIRECT_NORMAL) {
            /************************
            * Direct control
            ***********************/
            ControlObjectClient_setOrigin(control, NULL, 3);

            if (ControlObjectClient_operate(control, ctlVal, 0 /* operate now */)) {
                log_info("%s operated successfully\n", currObjRef);
            } else {
                log_info("failed to operate %s\n", currObjRef);
            }

        } else if(ControlObjectClient_getControlModel(control) == CONTROL_MODEL_SBO_NORMAL) {
            /************************
            * Select before operate
            ***********************/
            if (ControlObjectClient_select(control)) {

                if (ControlObjectClient_operate(control, ctlVal, 0 /* operate now */)) {
                    log_info("%s operated successfully\n", currObjRef);
                }
                else {
                    log_info("failed to operate %s!\n",currObjRef);
                }
            }
            else {
                log_info("failed to select %s!\n",currObjRef);
            }

        } else if(ControlObjectClient_getControlModel(control) == CONTROL_MODEL_DIRECT_ENHANCED) {
            /****************************************
             * Direct control with enhanced security
             ****************************************/

            ControlObjectClient_setCommandTerminationHandler(control, commandTerminationHandler, NULL);

            if (ControlObjectClient_operate(control, ctlVal, 0 /* operate now */)) {
                log_info("%s operated successfully\n",currObjRef);
            }
            else {
                log_info("failed to operate %s\n",currObjRef);
            }

            /* Wait for command termination message */
            Thread_sleep(1000);

            //ControlObjectClient_destroy(control);

        } else if(ControlObjectClient_getControlModel(control) == CONTROL_MODEL_SBO_ENHANCED) {
            /***********************************************
             * Select before operate with enhanced security
             ***********************************************/

            ControlObjectClient_setCommandTerminationHandler(control, commandTerminationHandler, NULL);

            if (ControlObjectClient_selectWithValue(control, ctlVal)) {

                if (ControlObjectClient_operate(control, ctlVal, 0 /* operate now */)) {
                    log_info("%s operated successfully\n",currObjRef);
                }
                else {
                    log_info("failed to operate %s!\n",currObjRef);
                }

            }
            else {
                log_info("failed to select %s!\n",currObjRef);
            }

            /* Wait for command termination message */
            Thread_sleep(1000);

        } else if(ControlObjectClient_getControlModel(control) == CONTROL_MODEL_STATUS_ONLY) {
            log_info("Control Model Status Only\n");
        } else {
            log_warn("Undefined control model\n");
        }

        setControlStVal(link,node->parent,currObjRef);

        ControlObjectClient_destroy(control);
        MmsValue_delete(ctlVal);
    }
}
//Called when 'Oper' SDO is found
void handleControllableNode(DSLink *link, DSNode *node, IedConnection con, const char* doRef) {
    IedClientError error;
    bool isStVal = false;

    LinkedList dataAttributesFC = IedConnection_getDataDirectoryFC(con, &error, doRef);
    if(dataAttributesFC != NULL) {
        LinkedList dataAttributeFC = LinkedList_getNext(dataAttributesFC);

        while((dataAttributeFC != NULL) && !isStVal) {
            if (strncmp("stVal[", (char *) dataAttributeFC->data, 6) == 0) {
                isStVal = true;
                char daName[] = "stVal";
                char daRef[129];
                sprintf(daRef, "%s.%s", doRef, daName);
#ifdef DEVEL_DEBUG
                log_info("Controllable Object Found: %s\n",daRef);
#endif
                char fcStr[3];
                FunctionalConstraint fc = getFunctionalConstraint((char*) dataAttributeFC->data,fcStr);
                if(fc != IEC61850_FC_NONE) {
                    MmsVariableSpecification* mmsVarSpec = IedConnection_getVariableSpecification(con,&error,daRef,fc);

#ifdef DEVEL_DEBUG
                    log_info("Controllable object type: %s\n", getMmsTypeString(mmsVarSpec->type));
#endif

                    if((mmsVarSpec->type == MMS_BOOLEAN) || (mmsVarSpec->type == MMS_INTEGER)) {
                        /*
                         * create control node
                         */
                        DSNode *controlNode = dslink_node_create(node, "control", "node");
                        if (!controlNode) {
                            log_warn("Failed to create control node\n");
                            return;
                        }

                        controlNode->on_invocation = controlAction;
                        dslink_node_set_meta(link, controlNode, "$name", json_string("Control"));
                        dslink_node_set_meta(link, controlNode, "$invokable", json_string("read"));

                        json_t *params = json_array();
                        json_t *valueRow = json_object();
                        json_object_set_new(valueRow, "name", json_string(DA_VALUE_REF_NODE_KEY));
                        switch (mmsVarSpec->type) {
                            case MMS_INTEGER:
                                dslink_node_set_meta(link, controlNode, "$editor", json_string("int"));
                                json_object_set_new(valueRow, "type", json_string("number"));
                                break;
                            case MMS_BOOLEAN:
                                json_object_set_new(valueRow, "type", json_string("bool"));
                                break;
                            default:
                                return;
                                break;
                        }

                        json_array_append_new(params, valueRow);
                        dslink_node_set_meta(link, controlNode, "$params", params);

                        if (dslink_node_add_child(link, controlNode) != 0) {
                            log_warn("Failed to add control node\n");
                            dslink_node_tree_free(link, controlNode);
                            return;
                        }

                        DSNode *objRefNode = dslink_node_create(node, DA_OBJECT_REF_NODE_KEY, "node");
                        if (!objRefNode) {
                            log_warn("Failed to create the object node\n");
                            return;
                        }
                        if (dslink_node_set_meta(link, objRefNode, "$type", json_string("string")) != 0) {
                            log_warn("Failed to set the type on the object\n");
                            dslink_node_tree_free(link, objRefNode);
                            return;
                        }
                        if (dslink_node_set_value(link, objRefNode, json_string(doRef)) != 0) {
                            log_warn("Failed to set the value on the object\n");
                            dslink_node_tree_free(link, objRefNode);
                            return;
                        }
                        if (dslink_node_add_child(link, objRefNode) != 0) {
                            log_warn("Failed to add the object node to the root\n");
                            dslink_node_tree_free(link, objRefNode);
                        }
                    }
                }
            } else {
                dataAttributeFC = LinkedList_getNext(dataAttributeFC);
            }
        }
    }
}

void setValueNodeFromMMS(DSLink *link, DSNode *valNode, MmsValue *mmsVal) {

      switch(MmsValue_getType(mmsVal)) {
            case MMS_INTEGER:
            case MMS_UNSIGNED:
                if (dslink_node_set_value(link, valNode, json_integer(MmsValue_toInt64(mmsVal))) != 0) {
                    log_warn("Failed to set the value on the Value Node\n");
                }
                break;
            case MMS_BOOLEAN:
                if (dslink_node_set_value(link, valNode, json_boolean(MmsValue_getBoolean(mmsVal))) != 0) {
                    log_warn("Failed to set the value on the Value Node\n");
                }
                break;
            case MMS_VISIBLE_STRING:
            case MMS_STRING:
                if (dslink_node_set_value(link, valNode, json_string(MmsValue_toString(mmsVal))) != 0) {
                    log_warn("Failed to set the value on the Value Node\n");
                }
                break;
            case MMS_UTC_TIME: {
                char buf[sizeof "2011-10-08T07:07:09.000Z"];
                if (dslink_node_set_value(link, valNode, json_string(
                        getUtcTimeString(MmsValue_getUtcTimeInMs(mmsVal), buf))) != 0) {
                    log_warn("Failed to set the value on the Value Node\n");
                }

            }
                break;
            case MMS_BIT_STRING: {
                int bitStrSize = MmsValue_getBitStringSize(mmsVal);
                char *bitStrBuf = createBitStringArray(bitStrSize);
                for (int i = 0; i < bitStrSize; i++) {
                    if (MmsValue_getBitStringBit(mmsVal, i)) {
                        bitStrBuf[2 * i] = '1';
                    } else {
                        bitStrBuf[2 * i] = '0';
                    }
                    bitStrBuf[(2 * i) + 1] = ',';
                }
                bitStrBuf[(2 * bitStrSize) - 1] = 0;
                if (dslink_node_set_value(link, valNode, json_string(bitStrBuf)) != 0) {
                    log_warn("Failed to set the value on the Value Node\n");
                }

//                    json_t *values = json_array();
//                    for (int i = 0; i < bitStrSize; i++) {
//                        json_t *jval = json_object();
//                        json_object_set_new(jval, "bit", json_boolean(MmsValue_getBitStringBit(value,i)));
//                        json_array_append_new(values, jval);
//
//                    }
//                    if (dslink_node_set_value(link, node->parent, values) != 0) {
//                        log_warn("Failed to set the value on the status\n");
//                    }
                destroyBitStringArray(bitStrBuf);
            }
                break;
            case MMS_FLOAT:
                if (dslink_node_set_value(link, valNode, json_real(MmsValue_toFloat(mmsVal))) != 0) {
                    log_warn("Failed to set the value on the Value Node\n");
                }
                break;
            default:
                break;
        }
}

void nodeValueRead(DSLink *link, DSNode *node) {

    if(!node)
        return;

    ref_t *tempRef;
    DSNode *currFcNode;
    DSNode *currRefNode;
    if( (tempRef = dslink_map_get(node->parent->children,"FC")) != NULL ) 
    {
        currFcNode = (DSNode*)(tempRef->data);
    }
    else
    {
        log_warn("Error while getting FC node\n");
        return;
    }
    if( (tempRef = dslink_map_get(node->parent->children,DA_OBJECT_REF_NODE_KEY)) != NULL )
    {
        currRefNode = (DSNode*)(tempRef->data);
    }
    else
    {
        log_warn("Error while getting Object node\n");
        return;
    }
    FunctionalConstraint currFc = FunctionalConstraint_fromString(json_string_value(currFcNode->value));
    const char* currObjRef = json_string_value(currRefNode->value);

    IedConnection iedConn = getServerIedConnection(node);
    if(iedConn) {
        IedClientError error;
        MmsValue* value = IedConnection_readObject(iedConn, &error, currObjRef, currFc);

        //log_info("Node->parent->name:%s mms:%s error:%d\n",node->parent->name,getMmsTypeString(value->type),error);
        if (value != NULL) {
            setValueNodeFromMMS(link,node,value);
            //log_info("read float value of %s with %s\n",currObjRef,FunctionalConstraint_toString(currFc));
            MmsValue_delete(value);
        }
    } else {
        log_warn("Error while getting ied connection\n");
    }
}

static
void daValueReadAction(DSLink *link, DSNode *node,
                       json_t *rid, json_t *params, ref_t *stream) { 
  (void)rid;
  (void)params;
  (void)stream;

#ifdef DEVEL_DEBUG
    log_info("value read: %s\n", node->path);
#endif
    nodeValueRead(link,node->parent);
}
static
void daValueWriteAction(DSLink *link, DSNode *node,
                        json_t *rid, json_t *params, ref_t *stream) {
  (void)rid;
  (void)stream;
#ifdef DEVEL_DEBUG
    log_info("value write: %s\n", node->parent->name);
#endif
    IedClientError error;
    MmsType currMmsType = MMS_DATA_ACCESS_ERROR;
    ref_t *tempRef;
    DSNode *currFcNode;
    DSNode *currRefNode;
    if( (tempRef = dslink_map_get(node->parent->parent->children,"FC")) != NULL )
    {
        currFcNode = (DSNode*)(tempRef->data);
    }
    else
    {
        log_warn("Error while getting FC node\n");
        return;
    }
    if ( (tempRef = dslink_map_get(node->parent->parent->children,DA_OBJECT_REF_NODE_KEY)) != NULL )
    {
        currRefNode = (DSNode*)(tempRef->data);
    }
    else
    {
        log_warn("Error while getting Object node\n");
        return;
    }
    FunctionalConstraint currFc = FunctionalConstraint_fromString(json_string_value(currFcNode->value));
    const char* currObjRef = json_string_value(currRefNode->value);
    IedConnection iedConn = getServerIedConnection(node);
    if(iedConn) {
        MmsVariableSpecification *mmsVarSpec = IedConnection_getVariableSpecification(iedConn, &error, currObjRef,
                                                                                      currFc);
        if (mmsVarSpec) {
            currMmsType = mmsVarSpec->type;
        } else {
            log_warn("Error getting MMS Variable Specification: %d\n", error);
            return;
        }

#ifdef DEVEL_DEBUG
        log_info("value write: %s %s %s\n", FunctionalConstraint_toString(currFc), currObjRef,
                 getMmsTypeString(currMmsType));
#endif

        switch(currMmsType) {
            case MMS_INTEGER: {
                int32_t entValue = (int32_t)json_number_value(json_object_get(params, DA_VALUE_REF_NODE_KEY));

#ifdef DEVEL_DEBUG
                log_info("write value integer: %d\n",entValue);
#endif

                MmsValue *value = MmsValue_newInteger(32);
                MmsValue_setInt32(value, entValue);
                IedConnection_writeObject(iedConn, &error, currObjRef, currFc, value);

                if (error != IED_ERROR_OK){
                    log_warn("failed to write %s!\n", currObjRef);
                }
                MmsValue_delete(value);
            }
                break;
            case MMS_UNSIGNED: {
                uint32_t entValue = json_number_value(json_object_get(params, DA_VALUE_REF_NODE_KEY));
#ifdef DEVEL_DEBUG
                log_info("write value integer: %u\n",entValue);
#endif
                MmsValue *value = MmsValue_newUnsigned(32);
                MmsValue_setUint32(value,entValue);
                IedConnection_writeObject(iedConn, &error, currObjRef, currFc, value);

                if (error != IED_ERROR_OK){
                    log_warn("failed to write %s!\n", currObjRef);
                }
                MmsValue_delete(value);
            }
                break;
            case MMS_BOOLEAN:
            {
                bool entValue = json_boolean_value(json_object_get(params, DA_VALUE_REF_NODE_KEY));
#ifdef DEVEL_DEBUG
                log_info("write value bool: %d\n",entValue);
#endif
                MmsValue *value = MmsValue_newBoolean(entValue);
                IedConnection_writeObject(iedConn, &error, currObjRef, currFc, value);
                if (error != IED_ERROR_OK){
                    log_warn("failed to write %s error:%d!\n", currObjRef,error);
                }
                MmsValue_delete(value);
            }
                break;
            case MMS_VISIBLE_STRING: {
                const char *strValue = json_string_value(json_object_get(params, DA_VALUE_REF_NODE_KEY));
#ifdef DEVEL_DEBUG
                log_info("write value string: %s\n", strValue);
#endif
                MmsValue *value = MmsValue_newVisibleString(strValue);
                IedConnection_writeObject(iedConn, &error, currObjRef, currFc, value);

                if (error != IED_ERROR_OK){
                    log_warn("failed to write %s!\n", currObjRef);
                }
                MmsValue_delete(value);
            }
                break;
            case MMS_STRING:{
                const char *strValue = json_string_value(json_object_get(params, DA_VALUE_REF_NODE_KEY));
#ifdef DEVEL_DEBUG
                log_info("write value string: %s\n", strValue);
#endif
                MmsValue *value = MmsValue_newMmsString((char*)strValue);
                IedConnection_writeObject(iedConn, &error, currObjRef, currFc, value);

                if (error != IED_ERROR_OK){
                    log_warn("failed to write %s!\n", currObjRef);
                }
                MmsValue_delete(value);
            }
                break;
            case MMS_UTC_TIME: {
                const char *strValue = json_string_value(json_object_get(params, DA_VALUE_REF_NODE_KEY));
                uint64_t msTime;
                msTime = parseIso8601(strValue);
                if(msTime)
                {
                    //log_info("parse iso8601 is ok! time: %lu %s\n",msTime,getUtcTimeString(msTime,buf));

                    MmsValue *value = MmsValue_newUtcTimeByMsTime(msTime);
                    IedConnection_writeObject(iedConn, &error, currObjRef, currFc, value);

                    if (error != IED_ERROR_OK){
                        log_warn("failed to write %s! error code: %d\n", currObjRef,error);
                    }
                    MmsValue_delete(value);
                }
                else{
                    log_info("Parsing iso8601 time format failed!\n");
                }
            }
                break;
            case MMS_BIT_STRING: {
                const char *strValue = json_string_value(json_object_get(params, DA_VALUE_REF_NODE_KEY));
                char *bitStrBuf = NULL;
                int bitStrSize, i;
                bitStrSize = parseBitString(strValue,bitStrBuf);
                if(bitStrSize) {
                    MmsValue *value = MmsValue_newBitString(bitStrSize);
                    for(i = 0 ; i < bitStrSize ; i++) {
                        MmsValue_setBitStringBit(value,i,(bool)bitStrBuf[i]);
                    }
                    IedConnection_writeObject(iedConn, &error, currObjRef, currFc, value);
                    if (error != IED_ERROR_OK){
                        log_warn("failed to write %s! error code: %d\n", currObjRef,error);
                    }
                    MmsValue_delete(value);
                    destroyBitStringArray(bitStrBuf);

                } else {
                    log_info("Invalid bit string csv\n");
                }

            }
                break;
            case MMS_FLOAT: {
                double entValue = json_number_value(json_object_get(params, DA_VALUE_REF_NODE_KEY));
                MmsValue *value = MmsValue_newFloat(entValue);
                IedConnection_writeObject(iedConn, &error, currObjRef, currFc, value);
                if (error != IED_ERROR_OK){
                    log_warn("failed to write %s!\n", currObjRef);
                }
                MmsValue_delete(value);
            }
                break;
            default:
                break;
        }

        nodeValueRead(link,node->parent);
    }
}

void
createValueNode(DSLink *link, DSNode *parentNode, const char* daRef, IedConnection con, FunctionalConstraint _fc, MmsType _mmsType) {
    (void)daRef;
    (void)con;
    /*
     * create Functional Constraint node
     */
    DSNode *valueNode = dslink_node_create(parentNode, DA_VALUE_REF_NODE_KEY, "node");
    if (!valueNode) {
        log_warn("Failed to create the Value node\n");
        return;
    }
    switch(_mmsType)
    {
        case MMS_INTEGER:
            if (dslink_node_set_meta(link, valueNode, "$type", json_string("number")) != 0) {
                log_warn("Failed to set the type on the Value\n");
                dslink_node_tree_free(link, valueNode);
                return;
            }
            break;
        case MMS_UNSIGNED:
            if (dslink_node_set_meta(link, valueNode, "$type", json_string("number")) != 0) {
                log_warn("Failed to set the type on the Value\n");
                dslink_node_tree_free(link, valueNode);
                return;
            }
            break;
        case MMS_BOOLEAN:
            if (dslink_node_set_meta(link, valueNode, "$type", json_string("bool")) != 0) {
                log_warn("Failed to set the type on the Value\n");
                dslink_node_tree_free(link, valueNode);
                return;
            }
            break;
        case MMS_VISIBLE_STRING:
        case MMS_STRING:
            if (dslink_node_set_meta(link, valueNode, "$type", json_string("string")) != 0) {
                log_warn("Failed to set the type on the Value\n");
                dslink_node_tree_free(link, valueNode);
                return;
            }
            break;
        case MMS_FLOAT:
            if (dslink_node_set_meta(link, valueNode, "$type", json_string("real")) != 0) {
                log_warn("Failed to set the type on the status\n");
                dslink_node_tree_free(link, valueNode);
                return;
            }
            break;
        case MMS_UTC_TIME:
            if (dslink_node_set_meta(link, valueNode, "$type", json_string("string")) != 0) {
                log_warn("Failed to set the type on the Value\n");
                dslink_node_tree_free(link, valueNode);
                return;
            }
            break;
        case MMS_BIT_STRING:
            if (dslink_node_set_meta(link, valueNode, "$type", json_string("string")) != 0) {
                log_warn("Failed to set the type on the Value\n");
                dslink_node_tree_free(link, valueNode);
                return;
            }
            break;
        default:
            break;
    }

    if (dslink_node_add_child(link, valueNode) != 0) {
        log_warn("Failed to add the Value node to the root\n");
        dslink_node_tree_free(link, valueNode);
    }

    /*
    * create read node
    * */
    DSNode *valueReadNode = dslink_node_create(valueNode, "readvalue", "node");
    if (!valueReadNode) {
        log_warn("Failed to create valueReadNode action node\n");
        return;
    }

    valueReadNode->on_invocation = daValueReadAction;
    dslink_node_set_meta(link, valueReadNode, "$name", json_string("Read"));
    dslink_node_set_meta(link, valueReadNode, "$invokable", json_string("read"));

    if (dslink_node_add_child(link, valueReadNode) != 0) {
        log_warn("Failed to add daValueReadAction action to the Value node\n");
        dslink_node_tree_free(link, valueReadNode);
        return;
    }

    if(isWritableFC(_fc))
    {
        //log_info("Writable fc: %s\n",FunctionalConstraint_toString(_fc));
        /*
        * create write node
        * */
        DSNode *valueWriteNode = dslink_node_create(valueNode, "writevalue", "node");
        if (!valueWriteNode) {
            log_warn("Failed to create valueWriteNode action node\n");
            return;
        }

        valueWriteNode->on_invocation = daValueWriteAction;
        dslink_node_set_meta(link, valueWriteNode, "$name", json_string("Write"));
        dslink_node_set_meta(link, valueWriteNode, "$invokable", json_string("read"));

        json_t *params = json_array();
        json_t *valueRow = json_object();
        json_object_set_new(valueRow, "name", json_string(DA_VALUE_REF_NODE_KEY));
        switch(_mmsType)
        {
            case MMS_INTEGER:
                dslink_node_set_meta(link, valueWriteNode, "$editor", json_string("int"));
                json_object_set_new(valueRow, "type", json_string("number"));
                break;
            case MMS_UNSIGNED:
                json_object_set_new(valueRow, "type", json_string("number"));
                break;
            case MMS_BOOLEAN:
                json_object_set_new(valueRow, "type", json_string("bool"));
                break;
            case MMS_VISIBLE_STRING:
            case MMS_STRING:
                json_object_set_new(valueRow, "type", json_string("string"));
                break;
            case MMS_FLOAT:
                json_object_set_new(valueRow, "type", json_string("number"));
                break;
            case MMS_UTC_TIME:
                dslink_node_set_meta(link, valueWriteNode, "$editor", json_string("date"));
                json_object_set_new(valueRow, "type", json_string("string"));
                break;
            case MMS_BIT_STRING:
                json_object_set_new(valueRow, "type", json_string("string"));
                break;
            default:
                json_object_set_new(valueRow, "type", json_string("string"));
                break;
        }

        json_array_append_new(params, valueRow);
        dslink_node_set_meta(link, valueWriteNode, "$params", params);

        if (dslink_node_add_child(link, valueWriteNode) != 0) {
            log_warn("Failed to add valueWriteNode action to the Value node\n");
            dslink_node_tree_free(link, valueWriteNode);
            return;
        }
    } else{
#ifdef DEVEL_DEBUG
        log_info("Not writable fc: %s\n",FunctionalConstraint_toString(_fc));
#endif
    }
}

static
void addDAToDSAction(DSLink *link, DSNode *node,
                        json_t *rid, json_t *params, ref_t *stream) {
    (void)rid;

    const char *dsName;

    (void) node;
    (void) params;
    (void) stream;

    dsName = json_string_value(json_object_get(params, "Dataset"));
    if(dsName && (strlen(dsName) > 0))
    {
        char dsKey[200] = DATASET_NODE_KEY_INIT;
        strcat(dsKey,dsName);
        DSNode *mainPrepDSNode = getMainPrepDatasetNode(node);
        DSNode *prepDatasetNode = NULL;
        if(mainPrepDSNode) {
            prepDatasetNode = dslink_node_get_path(mainPrepDSNode,dsKey);
        }
        DSNode *daObjRefNode = dslink_node_get_path(node->parent,DA_OBJECT_REF_NODE_KEY);
        DSNode *daFCNode = dslink_node_get_path(node->parent,"FC");

        if(daFCNode && daObjRefNode && prepDatasetNode) {
            char daRefFull[400];
            strcpy(daRefFull,json_string_value(daObjRefNode->value));
            strcat(daRefFull,"[");
            strcat(daRefFull,json_string_value(daFCNode->value));
            strcat(daRefFull,"]");
            addMemberToPrepDatasetNode(link,prepDatasetNode,daRefFull);
        }

    }
    else
    {
        log_warn("Missing  parameter\n");
    }

}

static
void daNodeListOpenedAction(DSLink *link, DSNode *node) {
    (void) link;
#ifdef DEVEL_DEBUG
    log_info("DA list opened to %s\n", node->path);
#endif

    char prepDSListEnumStr[1000] = "enum[";
    if(!getPrepDSListEnumStr(node,prepDSListEnumStr)) {
#ifdef DEVEL_DEBUG
        log_info("No prep Data Set\n");
#endif
    }

    DSNode *addToDSNode = dslink_node_get_path(node,"adddataset");
    if(addToDSNode) {
        json_t *params = json_array();
        json_t *ds_row = json_object();
        json_object_set_new(ds_row, "name", json_string("Dataset"));
        json_object_set_new(ds_row, "type", json_string(prepDSListEnumStr));
        json_array_append_new(params, ds_row);

        dslink_node_set_meta(link, addToDSNode, "$params", params);
    }

}

DSNode *createDONode(DSLink *link, DSNode *lnNode, const char *doRef, const char *doName) {
    DSNode *newDONode = dslink_node_create(lnNode, doName, "node");
    if (!newDONode) {
        log_warn("Failed to create the DO node\n");
        return NULL;
    }
    if (dslink_node_add_child(link, newDONode) != 0) {
        log_warn("Failed to add the DO node to the root\n");
        dslink_node_tree_free(link, newDONode);
        return NULL;
    }

    /*
     * create Obj. ref. node
     */
    DSNode *refNode = dslink_node_create(newDONode, "Reference", "node");
    if (!refNode) {
        log_warn("Failed to create the Reference node\n");
        return NULL;
    }
    if (dslink_node_set_meta(link, refNode, "$type", json_string("string")) != 0) {
        log_warn("Failed to set the type on the Reference\n");
        dslink_node_tree_free(link, refNode);
        return NULL;
    }
    if (dslink_node_set_value(link, refNode, json_string(doRef)) != 0) {
        log_warn("Failed to set the value on the Reference\n");
        dslink_node_tree_free(link, refNode);
        return NULL;
    }
    if (dslink_node_add_child(link, refNode) != 0) {
        log_warn("Failed to add the Reference node to the root\n");
        dslink_node_tree_free(link, refNode);
    }
    return newDONode;
}

DSNode *createDANode(DSLink *link, DSNode *topNode, const char* daRef, const char *daName, const char *daNameFC, const char *fcStr, FunctionalConstraint fc, const char* mmsTypeStr) {
    (void)daName;
    //for [FC] at the end of the DA name, comment out the above one.
    DSNode *newDANode = dslink_node_create(topNode, daNameFC, "node");
    //DSNode *newDANode = dslink_node_create(topNode, daName, "node");
    if (!newDANode) {
        log_warn("Failed to create the data attribute node\n");
        return NULL;
    }
    if (dslink_node_add_child(link, newDANode) != 0) {
        log_warn("Failed to add the data attribute node to the root\n");
        dslink_node_tree_free(link, newDANode);
        return NULL;
    }

    newDANode->on_list_open = daNodeListOpenedAction;

    /*
     * create FC node
     */
    if(fc != IEC61850_FC_NONE) {
        /*
         * create Functional Constraint node
         */
        DSNode *fcNode = dslink_node_create(newDANode, "FC", "node");
        if (!fcNode) {
            log_warn("Failed to create the FC node\n");
            return NULL;
        }
        if (dslink_node_set_meta(link, fcNode, "$type", json_string("string")) != 0) {
            log_warn("Failed to set the type on the FC\n");
            dslink_node_tree_free(link, fcNode);
            return NULL;
        }
        if (dslink_node_set_value(link, fcNode, json_string(fcStr)) != 0) {
            log_warn("Failed to set the value on the FC\n");
            dslink_node_tree_free(link, fcNode);
            return NULL;
        }
        if (dslink_node_add_child(link, fcNode) != 0) {
            log_warn("Failed to add the FC node to the root\n");
            dslink_node_tree_free(link, fcNode);
            return NULL;
        }
        /*
         * create functional constraint explanation node
         */
        DSNode *fcExpNode = dslink_node_create(newDANode, "Type", "node");
        if (!fcExpNode) {
            log_warn("Failed to create the FC Exp node\n");
            return NULL;
        }
        if (dslink_node_set_meta(link, fcExpNode, "$type", json_string("string")) != 0) {
            log_warn("Failed to set the type on the FC Exp\n");
            dslink_node_tree_free(link, fcExpNode);
            return NULL;
        }
        if (dslink_node_set_value(link, fcExpNode, json_string(getFCExplainString(fc))) != 0) {
            log_warn("Failed to set the value on the FC exp\n");
            dslink_node_tree_free(link, fcExpNode);
            return NULL;
        }
        if (dslink_node_add_child(link, fcExpNode) != 0) {
            log_warn("Failed to add the FC Exp node to the root\n");
            dslink_node_tree_free(link, fcExpNode);
            return NULL;
        }
    } else{
        log_warn("Error while resolving FC!\n");
    }

    /*
     * create Obj. ref. node
     */
    DSNode *refNode = dslink_node_create(newDANode, DA_OBJECT_REF_NODE_KEY, "node");
    if (!refNode) {
        log_warn("Failed to create the Obj Ref node\n");
        return NULL;
    }
    if (dslink_node_set_meta(link, refNode, "$type", json_string("string")) != 0) {
        log_warn("Failed to set the type on the Obj Ref\n");
        dslink_node_tree_free(link, refNode);
        return NULL;
    }
    if (dslink_node_set_value(link, refNode, json_string(daRef)) != 0) {
        log_warn("Failed to set the value on the Obj Ref\n");
        dslink_node_tree_free(link, refNode);
        return NULL;
    }
    if (dslink_node_add_child(link, refNode) != 0) {
        log_warn("Failed to add the Obj Ref node to the root\n");
        dslink_node_tree_free(link, refNode);
        return NULL;
    }
    /*
     * create mms type node
     */
    if(fc != IEC61850_FC_NONE) {
        DSNode *mmsTypeNode = dslink_node_create(newDANode, "MMS Type", "node");
        if (!mmsTypeNode) {
            log_warn("Failed to create the MMS node\n");
            return NULL;
        }
        if (dslink_node_set_meta(link, mmsTypeNode, "$type", json_string("string")) != 0) {
            log_warn("Failed to set the type on the MMS\n");
            dslink_node_tree_free(link, mmsTypeNode);
            return NULL;
        }
        if (dslink_node_set_value(link, mmsTypeNode, json_string(mmsTypeStr)) != 0) {
            log_warn("Failed to set the value on the MMS\n");
            dslink_node_tree_free(link, mmsTypeNode);
            return NULL;
        }
        if (dslink_node_add_child(link, mmsTypeNode) != 0) {
            log_warn("Failed to add the MMS node to the root\n");
            dslink_node_tree_free(link, mmsTypeNode);
            return NULL;
        }
    }
    /*
    * create add to dataset node
    * */
    DSNode *addToDSNode = dslink_node_create(newDANode, "adddataset", "node");
    if (!addToDSNode) {
        log_warn("Failed to create add to dataset action node\n");
        return NULL;
    }
    addToDSNode->on_invocation = addDAToDSAction;
    dslink_node_set_meta(link, addToDSNode, "$name", json_string("Add to dataset"));
    dslink_node_set_meta(link, addToDSNode, "$invokable", json_string("read"));
    json_t *params = json_array();
    json_t *ds_row = json_object();
    json_object_set_new(ds_row, "name", json_string("Dataset"));
    json_object_set_new(ds_row, "type", json_string("enum[]"));
    json_array_append_new(params, ds_row);

    dslink_node_set_meta(link, addToDSNode, "$params", params);
    if (dslink_node_add_child(link, addToDSNode) != 0) {
        log_warn("Failed to add clear action to the server node\n");
        dslink_node_tree_free(link, addToDSNode);
        return NULL;
    }

    return newDANode;

}

//void
//discoverDataDirectory(DSLink *link, DSNode *node, char *doRef, IedConnection con, int spaces) {
//    IedClientError error;
//
//    LinkedList dataAttributes = IedConnection_getDataDirectory(con, &error, doRef);
//    LinkedList dataAttributesFC = IedConnection_getDataDirectoryFC(con, &error, doRef);
//
//    //LinkedList dataAttributes = IedConnection_getDataDirectoryByFC(con, &error, doRef, MX);
//
//    if (dataAttributes != NULL) {
//        LinkedList dataAttribute = LinkedList_getNext(dataAttributes);
//        LinkedList dataAttributeFC = LinkedList_getNext(dataAttributesFC);
//
//        //if last leaf?
//        if (dataAttribute == NULL) {
//
//            ref_t *tempRef;
//            DSNode *currFcNode;
//            DSNode *currRefNode;
//            if(tempRef = dslink_map_get(node->children,"FC"))
//            {
//                currFcNode = (DSNode*)(tempRef->data);
//            }
//            else
//            {
//                log_warn("Error while getting FC node\n");
//                return;
//            }
//            if(tempRef = dslink_map_get(node->children,DA_OBJECT_REF_NODE_KEY))
//            {
//                currRefNode = (DSNode*)(tempRef->data);
//            }
//            else
//            {
//                log_warn("Error while getting Object node\n");
//                return;
//            }
//            FunctionalConstraint currFc = FunctionalConstraint_fromString(json_string_value(currFcNode->value));
//            const char* currObjRef = json_string_value(currRefNode->value);
//
//            if(currFc != IEC61850_FC_NONE) {
//                MmsVariableSpecification *mmsVarSpec = IedConnection_getVariableSpecification(con, &error, currObjRef, currFc);
//                if(mmsVarSpec){
//                    createValueNode(link,node,currObjRef,con,currFc,mmsVarSpec->type);
//                }
//                else{
//                    log_warn("Error getting MMS Variable Specification: %d\n",error);
//                }
//            }
//#ifdef DEVEL_DEBUG
//            log_info("Functional Constraint: %s\nObj.Ref.: %s\n", FunctionalConstraint_toString(currFc),currObjRef);
//#endif
//        }
//
//        while (dataAttribute != NULL) {
//            char *daName = (char *) dataAttribute->data;
//            char daRef[129];
//            sprintf(daRef, "%s.%s", doRef, daName);
//
//            DSNode *newDANode = NULL;
//
//            FunctionalConstraint fc;
//            char fcStr[3];
//            fc = getFunctionalConstraint((char *) dataAttributeFC->data,fcStr);
//
//#ifdef DEVEL_DEBUG
//            log_info("New Data Attribute: %s - %s\n",daRef,fcStr);
//#endif
//
//            if(fc != IEC61850_FC_NONE) {
//                MmsVariableSpecification *mmsVarSpec = IedConnection_getVariableSpecification(con, &error, daRef, fc);
//                newDANode = createDANode(link,node,daRef,daName,(char *) dataAttributeFC->data,fcStr,fc,getMmsTypeString(mmsVarSpec->type));
//            } else {
//                log_warn("FC couldn't be resolved, DA  Node couldn't be created!\n");
//            }
//
//
//            if(newDANode) {
//                if (strcmp("Oper", daName) == 0)
//                    handleControllableNode(link, node, con, doRef);
//            }
//
//            dataAttribute = LinkedList_getNext(dataAttribute);
//            dataAttributeFC = LinkedList_getNext(dataAttributeFC);
//
//            if(newDANode) {
//                discoverDataDirectory(link, newDANode, daRef, con, spaces + 2);
//            }
//        }
//
//    }
//
//
//}

void
discoverDataDirectory(DSLink *link, DSNode *node, char *doRef, IedConnection con, FunctionalConstraint inFC) {
    IedClientError error;

    //LinkedList dataAttributes = IedConnection_getDataDirectory(con, &error, doRef);
    LinkedList dataAttributesFC;
    if(inFC == IEC61850_FC_NONE)
        dataAttributesFC = IedConnection_getDataDirectoryFC(con, &error, doRef);
    else
        dataAttributesFC = IedConnection_getDataDirectoryByFC(con, &error, doRef, inFC);


    if (dataAttributesFC != NULL) {
        //LinkedList dataAttribute = LinkedList_getNext(dataAttributes);
        LinkedList dataAttributeFC = LinkedList_getNext(dataAttributesFC);

        //if last leaf?
        if (dataAttributeFC == NULL) {

            ref_t *tempRef;
            DSNode *currFcNode;
            DSNode *currRefNode;
            if( (tempRef = dslink_map_get(node->children,"FC")) != NULL ) 
            {
                currFcNode = (DSNode*)(tempRef->data);
            }
            else
            {
                log_warn("Error while getting FC node\n");
                return;
            }
            if( (tempRef = dslink_map_get(node->children,DA_OBJECT_REF_NODE_KEY)) != NULL )
            {
                currRefNode = (DSNode*)(tempRef->data);
            }
            else
            {
                log_warn("Error while getting Object node\n");
                return;
            }
            FunctionalConstraint currFc = FunctionalConstraint_fromString(json_string_value(currFcNode->value));
            const char* currObjRef = json_string_value(currRefNode->value);

            if(currFc != IEC61850_FC_NONE) {
                MmsVariableSpecification *mmsVarSpec = IedConnection_getVariableSpecification(con, &error, currObjRef, currFc);
                if(mmsVarSpec){
                    createValueNode(link,node,currObjRef,con,currFc,mmsVarSpec->type);
                }
                else{
                    log_warn("Error getting MMS Variable Specification: %d\n",error);
                }
            }
#ifdef DEVEL_DEBUG
            log_info("Functional Constraint: %s\nObj.Ref.: %s\n", FunctionalConstraint_toString(currFc),currObjRef);
#endif
        }

        while (dataAttributeFC != NULL) {
            char *daName = StringUtils_copyString((const char*)dataAttributeFC->data);
            char *tempPtr = strchr(daName,'[');
            if(tempPtr)
                *tempPtr = '\0';
//            char *daName = (char *) dataAttribute->data;
            char daRef[129];
            sprintf(daRef, "%s.%s", doRef, daName);

            DSNode *newDANode = NULL;

            FunctionalConstraint fc;
            char fcStr[3];
            fc = getFunctionalConstraint((char *) dataAttributeFC->data,fcStr);

#ifdef DEVEL_DEBUG
            log_info("New Data Attribute: %s - %s\n",daRef,fcStr);
#endif

            if(fc != IEC61850_FC_NONE) {
                MmsVariableSpecification *mmsVarSpec = IedConnection_getVariableSpecification(con, &error, daRef, fc);
                newDANode = createDANode(link,node,daRef,daName,(char *) dataAttributeFC->data,fcStr,fc,getMmsTypeString(mmsVarSpec->type));
            } else {
                log_warn("FC couldn't be resolved, DA  Node couldn't be created!\n");
                newDANode = createDONode(link,node,daRef,daName); // means SDO
            }


            if(newDANode) {
                if (strcmp("Oper", daName) == 0)
                    handleControllableNode(link, node, con, doRef);
            }

            //dataAttribute = LinkedList_getNext(dataAttribute);
            dataAttributeFC = LinkedList_getNext(dataAttributeFC);

            if(newDANode) {
                discoverDataDirectory(link, newDANode, daRef, con, fc);
            }
        }

    }


}

void
connect_61850_ied(IedConnection con, const char *hostname, int tcpPort) {
    IedClientError error;

    //iedConn = IedConnection_create();

    IedConnection_connect(con, &error, hostname, tcpPort);
    if (error != IED_ERROR_OK) {
        log_info("IED connection failed!\n");
    } else log_info("IED connection established\n");
}

static
void prepare_dataset_action(DSLink *link, DSNode *node,
                     json_t *rid, json_t *params, ref_t *stream) {
    (void)rid;
    (void)stream;

    const char *name;

    (void) node;
    (void) params;
    (void) stream;

    name = json_string_value(json_object_get(params, "Name"));
    if(name)
    {

        ref_t *tempRef;
        DSNode *currRefNode;
        if( (tempRef = dslink_map_get(node->parent->children,"Reference")) != NULL ) 
        {
            currRefNode = (DSNode*)(tempRef->data);
            const char* currObjRef = json_string_value(currRefNode->value);
            char dsRef[300];
            sprintf(dsRef,"%s.%s",currObjRef,name);


            if(!createPrepDatasetNode(link,node->parent,dsRef)) {
                log_warn("Unable to create dataset prep node\n");
            }
        }
        else
        {
            log_warn("Error while getting LN Reference node\n");
            return;
        }
    }
    else
    {
        log_warn("Missing  parameter\n");
    }

}

DSNode *createLNNode(DSLink *link, DSNode *ldNode, const char *lnRef, const char *lnName) {
    DSNode *newLNNode = dslink_node_create(ldNode, lnName, "node");
    if (!newLNNode) {
        log_warn("Failed to create the LN node\n");
        return NULL;
    }
    if (dslink_node_add_child(link, newLNNode) != 0) {
        log_warn("Failed to add the LN node to the root\n");
        dslink_node_tree_free(link, newLNNode);
        return NULL;
    }
    /*
     * create Obj. ref. node
     */
    DSNode *refNode = dslink_node_create(newLNNode, "Reference", "node");
    if (!refNode) {
        log_warn("Failed to create the Reference node\n");
        return NULL;
    }
    if (dslink_node_set_meta(link, refNode, "$type", json_string("string")) != 0) {
        log_warn("Failed to set the type on the Reference\n");
        dslink_node_tree_free(link, refNode);
        return NULL;
    }
    if (dslink_node_set_value(link, refNode, json_string(lnRef)) != 0) {
        log_warn("Failed to set the value on the Reference\n");
        dslink_node_tree_free(link, refNode);
        return NULL;
    }
    if (dslink_node_add_child(link, refNode) != 0) {
        log_warn("Failed to add the Reference node to the root\n");
        dslink_node_tree_free(link, refNode);
    }
    /*
    * create Prepare Dataset node
    * */
    DSNode *prepDSNode = dslink_node_create(newLNNode, "prepdataset", "node");
    if (!prepDSNode) {
        log_warn("Failed to create Prepare Dataset action node\n");
        return NULL;
    }

    prepDSNode->on_invocation = prepare_dataset_action;
    dslink_node_set_meta(link, prepDSNode, "$name", json_string("Prepare Dataset"));
    dslink_node_set_meta(link, prepDSNode, "$invokable", json_string("read"));

    json_t *params = json_array();

    json_t *name_row = json_object();
    json_object_set_new(name_row, "name", json_string("Name"));
    json_object_set_new(name_row, "type", json_string("string"));

    json_array_append_new(params, name_row);

    dslink_node_set_meta(link, prepDSNode, "$params", params);

    if (dslink_node_add_child(link, prepDSNode) != 0) {
        log_warn("Failed to add discover action to the server node\n");
        dslink_node_tree_free(link, prepDSNode);
        return NULL;
    }

    return newLNNode;
}

DSNode *createLDNode(DSLink *link, DSNode *srvNode, const char *ldName) {
    char ldKey[129] = LOGICAL_DEVICE_NODE_KEY_INIT;
    strcat(ldKey,ldName);
    DSNode *newLDNode = dslink_node_create(srvNode, ldKey, "node");
    if (!newLDNode) {
        log_warn("Failed to create the logical device node\n");
        return NULL;
    }
    if (dslink_node_set_meta(link, newLDNode, "$name", json_string(ldName)) != 0) {
        log_warn("Failed to set the name of LD\n");
        dslink_node_tree_free(link, newLDNode);
        return NULL;
    }
    if (dslink_node_add_child(link, newLDNode) != 0) {
        log_warn("Failed to add the logical device node to the server node\n");
        dslink_node_tree_free(link, newLDNode);
        return NULL;
    }

    return newLDNode;
}

void
createDatasetReportNodes(DSLink *link, DSNode *node) {

    DSNode *newDSNode = dslink_node_create(node, DATASET_FOLDER_NODE_KEY, "node");
    if (!newDSNode) {
        log_warn("Failed to create the Dataset folder node\n");
        return;
    }
    if (dslink_node_set_meta(link, newDSNode, "$name", json_string("Data Sets")) != 0) {
        log_warn("Failed to set the name of Dataset folder\n");
        dslink_node_tree_free(link, newDSNode);
        return;
    }
    if (dslink_node_add_child(link, newDSNode) != 0) {
        log_warn("Failed to add the Dataset folder node to the root\n");
        dslink_node_tree_free(link, newDSNode);
        return;
    }
    ///////////////////////////////////////////////////
    DSNode *newPrepDSNode = dslink_node_create(node, DATASET_PREP_FOLDER_NODE_KEY, "node");
    if (!newPrepDSNode) {
        log_warn("Failed to create the Dataset Prep folder node\n");
        return;
    }
    if (dslink_node_set_meta(link, newPrepDSNode, "$name", json_string("Data Set Prep")) != 0) {
        log_warn("Failed to set the name of Dataset Prep folder\n");
        dslink_node_tree_free(link, newPrepDSNode);
        return;
    }
    if (dslink_node_add_child(link, newPrepDSNode) != 0) {
        log_warn("Failed to add the Dataset Prep folder node to the root\n");
        dslink_node_tree_free(link, newPrepDSNode);
        return;
    }
    /*
        * create Object Reference node
        */
    DSNode *descNode = dslink_node_create(newPrepDSNode, "desc", "node");
    if (!descNode) {
        log_warn("Failed to create the Description node\n");
        return;
    }
    if (dslink_node_set_meta(link, descNode, "$name", json_string("Description")) != 0) {
        log_warn("Failed to set the type on the Description\n");
        dslink_node_tree_free(link, descNode);
        return;
    }
    if (dslink_node_set_meta(link, descNode, "$type", json_string("string")) != 0) {
        log_warn("Failed to set the type on the Description\n");
        dslink_node_tree_free(link, descNode);
        return;
    }
    if (dslink_node_set_value(link, descNode, json_string("Preparation for datasets to be created in the server")) != 0) {
        log_warn("Failed to set the value on the Description\n");
        dslink_node_tree_free(link, descNode);
        return;
    }
    if (dslink_node_add_child(link, descNode) != 0) {
        log_warn("Failed to add the Description node to the root\n");
        dslink_node_tree_free(link, descNode);
        return;
    }
    ///////////////////////////////////
    DSNode *newURCBFolderNode = dslink_node_create(node, URCB_FOLDER_NODE_KEY, "node");
    if (!newURCBFolderNode) {
        log_warn("Failed to create the URCB main folder node\n");
        return;
    }
    if (dslink_node_set_meta(link, newURCBFolderNode, "$name", json_string("Unbuffered Report Control Blocks")) != 0) {
        log_warn("Failed to set the name of URCB main folder node\n");
        dslink_node_tree_free(link, newURCBFolderNode);
        return;
    }
    if (dslink_node_add_child(link, newURCBFolderNode) != 0) {
        log_warn("Failed to add the URCB main folder node to the root\n");
        dslink_node_tree_free(link, newURCBFolderNode);
        return;
    }

    DSNode *newBRCBFolderNode = dslink_node_create(node, BRCB_FOLDER_NODE_KEY, "node");
    if (!newBRCBFolderNode) {
        log_warn("Failed to create the BRCB main folder node\n");
        return;
    }
    if (dslink_node_set_meta(link, newBRCBFolderNode, "$name", json_string("Buffered Report Control Blocks")) != 0) {
        log_warn("Failed to set the name of BRCB main folder node\n");
        dslink_node_tree_free(link, newBRCBFolderNode);
        return;
    }
    if (dslink_node_add_child(link, newBRCBFolderNode) != 0) {
        log_warn("Failed to add the BRCB main folder node to the root\n");
        dslink_node_tree_free(link, newBRCBFolderNode);
        return;
    }
}

void
discover61850Server(DSLink *link, DSNode *node, IedConnection iedConn) {

    createDatasetReportNodes(link, node);

    IedClientError error;

#ifdef DEVEL_DEBUG
    log_info("Get logical device list...\n");
#endif
    LinkedList deviceList = IedConnection_getLogicalDeviceList(iedConn, &error);

    if (error != IED_ERROR_OK) {
        log_info("Failed to read device list (error code: %i)\n", error);
        return;
        //goto cleanup_and_exit;
    }

    LinkedList device = LinkedList_getNext(deviceList);

    while (device != NULL) {
#ifdef DEVEL_DEBUG
        log_info("LD: %s\n", (char *) device->data);
#endif
        ///////////////////////////////////////////////////
        DSNode *newLDNode = createLDNode(link, node, (char*) device->data);
        ///////////////////////////////////////////////////

        LinkedList logicalNodes = IedConnection_getLogicalDeviceDirectory(iedConn, &error,
                                                                          (char *) device->data);

        LinkedList logicalNode = LinkedList_getNext(logicalNodes);

        while (logicalNode != NULL) {
            char lnRef[129];
            sprintf(lnRef, "%s/%s", (char *) device->data, (char *) logicalNode->data);

#ifdef DEVEL_DEBUG
            log_info("  LN: %s\n", (char *) logicalNode->data);
#endif
            ///////////////////////////////////////////////////
            DSNode *newLNNode = createLNNode(link, newLDNode, lnRef, (char *) logicalNode->data);
            ///////////////////////////////////////////////////

            LinkedList dataObjects = IedConnection_getLogicalNodeDirectory(iedConn, &error,
                                                                           lnRef, ACSI_CLASS_DATA_OBJECT);

            LinkedList dataObject = LinkedList_getNext(dataObjects);

            while (dataObject != NULL) {
                char *dataObjectName = (char *) dataObject->data;
                char doRef[129];
                sprintf(doRef, "%s/%s.%s", (char *) device->data, (char *) logicalNode->data, dataObjectName);
#ifdef DEVEL_DEBUG
                log_info("    DO: %s\n", dataObjectName);
#endif
                ///////////////////////////////////////////////////
                DSNode *newDONode = createDONode(link,newLNNode,doRef,dataObjectName);
                ///////////////////////////////////////////////////

                dataObject = LinkedList_getNext(dataObject);

                discoverDataDirectory(link, newDONode, doRef, iedConn, IEC61850_FC_NONE);
            }

            LinkedList_destroy(dataObjects);

            //handle dataset, report

            /*
             * Dataset nodes created
             */
#ifdef DEVEL_DEBUG
            log_info("    Dataset discovery: %s\n",lnRef);
#endif
            LinkedList dataSets = IedConnection_getLogicalNodeDirectory(iedConn, &error, lnRef,
                                                                        ACSI_CLASS_DATA_SET);

            LinkedList dataSet = LinkedList_getNext(dataSets);

            while (dataSet != NULL) {

                char *dataSetName = (char *) dataSet->data;
                char dataSetRef[129];
                sprintf(dataSetRef, "%s.%s", lnRef, dataSetName);

                if(!getDatasetInfoFromServerCreateNode(link,newLNNode,iedConn,dataSetRef)){
                    log_warn("Data set couldn't be created!\n");
                }

                dataSet = LinkedList_getNext(dataSet);
            }

            LinkedList_destroy(dataSets);

            /*
             * Report control blocks
             */

            LinkedList reports = IedConnection_getLogicalNodeDirectory(iedConn, &error, lnRef,
                                                                       ACSI_CLASS_URCB);

            LinkedList report = LinkedList_getNext(reports);

            while (report != NULL) {
                char *reportName = (char *) report->data;
                char rcbRef[200];
                sprintf(rcbRef,"%s.RP.%s",lnRef,reportName);
#ifdef DEVEL_DEBUG
                log_info("    RP: %s   %s\n", reportName, rcbRef);
#endif
                if(!getRCBInfoAndCreateNode(link,node,iedConn,rcbRef,false)) {
                    log_warn("RCB node couldn't be created\n");
                }

                report = LinkedList_getNext(report);
            }

            LinkedList_destroy(reports);

            reports = IedConnection_getLogicalNodeDirectory(iedConn, &error, lnRef,
                                                            ACSI_CLASS_BRCB);

            report = LinkedList_getNext(reports);

            while (report != NULL) {
                char *reportName = (char *) report->data;
                char rcbRef[200];
                sprintf(rcbRef,"%s.BR.%s",lnRef,reportName);
#ifdef DEVEL_DEBUG
                log_info("    BR: %s   %s\n", reportName, rcbRef);
#endif
                if(!getRCBInfoAndCreateNode(link,node,iedConn, rcbRef,true)) {
                    log_warn("RCB node couldn't be created\n");
                }

                report = LinkedList_getNext(report);
            }

            LinkedList_destroy(reports);

            /*
             * Report control blocks ends
             */

            logicalNode = LinkedList_getNext(logicalNode);
        }

        LinkedList_destroy(logicalNodes);

        device = LinkedList_getNext(device);
    }

    LinkedList_destroy(deviceList);

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool isDiscovered(DSNode *node) {
    if(strncmp(node->name,SERVER_NODE_KEY_INIT, NODE_KEY_INIT_LEN) != 0)
        return false;
    else {
        dslink_map_foreach(node->children) {
            DSNode *tempNode = (DSNode *) (entry->value->data);

            if(strncmp(tempNode->name, NO_DELETE_NODE_KEY_INIT, NODE_KEY_INIT_LEN) != 0) {
                return true;
            }
        }
    }
    return false;
}
void removeAllDataModel(DSLink *link, DSNode *node) {

    ref_t *nodeToRemove = NULL;

    dslink_map_foreach(node->children) {
        DSNode *tempNode = (DSNode *) (entry->value->data);

        if (nodeToRemove) {
            dslink_node_tree_free(link, nodeToRemove->data);
            dslink_decref(nodeToRemove);
        }

        if(strncmp(tempNode->name, NO_DELETE_NODE_KEY_INIT, NODE_KEY_INIT_LEN) != 0) {
          nodeToRemove = dslink_map_remove_get(node->children, (char*)tempNode->name);
        }
    }

    if (nodeToRemove) {
        dslink_node_tree_free(link, nodeToRemove->data);
        dslink_decref(nodeToRemove);
    }
}

bool removeServerNode(DSLink *link, DSNode *serverNode) {
    DSNode *rootNode = serverNode->parent;

    //TODO: type cast warning
    //void *key = serverNode->name;
    ref_t *n = dslink_map_remove_get(rootNode->children, (char*)serverNode->name);
    if (n) {
        //remove and get iec61850 server from serverMap
        ref_t *iec61850ServerRef = dslink_map_remove_get(serverMap, serverNode);
        //remove iec61850 server, free memory
        iec61850ServerDestroy((ts61850Server) (iec61850ServerRef->data));
        dslink_node_tree_free(link, n->data);
        dslink_decref(n);
        return true;
    } else {
        return false;
    }
}

static
void edit_server_action(DSLink *link, DSNode *node,
                        json_t *rid, json_t *params, ref_t *stream) {

    (void)rid;
    (void)params;
    (void)stream;

    ts61850Server serverPtr;
    if ( (serverPtr = (ts61850Server) (dslink_map_get(serverMap, (node->parent))->data)) != NULL ) {
#ifdef DEVEL_DEBUG
        log_info("edit_server_action: iedconn host:%s port:%d\n", serverPtr->hostname, serverPtr->tcpPort);
#endif
        const char *newName;
        const char *newHost;
        int newTcpPort;
        newName = json_string_value(json_object_get(params, "Name"));
        newHost = json_string_value(json_object_get(params, "Host"));
        newTcpPort = json_integer_value(json_object_get(params, "Port"));
#ifdef DEVEL_DEBUG
        log_info("Old name: %s  New name: %s new host:%s new port:%d\n", node->parent->name, newName, newHost,
                 newTcpPort);
#endif
        if ((unsigned) strlen(newName) &&
            (unsigned) strlen(newHost) &&
            newTcpPort) {
            if ((strcmp(serverPtr->hostname, newHost) ||
                 strcmp(node->parent->name, newName) ||
                 (serverPtr->tcpPort != newTcpPort))) {
                DSNode *rootNode = node->parent->parent;
                if (!removeServerNode(link, node->parent)) {
                    log_warn("Server node couldn't be removed properly!\n");
                } else {
                    responderInitClient(link, rootNode, newName, newHost, newTcpPort);
                }
            } else {
                log_info("Nothing changed for edit, reconnecting...\n");
                connect_61850_ied(serverPtr->iedConn, newHost, newTcpPort);
            }

        } else {
            log_info("Missing parameter for edit\n");
        }

    } else {
        log_warn("Getting server from map failed!\n");
    }
}

static
void remove_server_action(DSLink *link, DSNode *node,
                          json_t *rid, json_t *params, ref_t *stream) {
    (void) params;
    (void) stream;
    json_t *top = json_object();
    if (!top) {
        return;
    }
    json_t *resps = json_array();
    if (!resps) {
        json_delete(top);
        return;
    }
    json_object_set_new_nocheck(top, "responses", resps);

    json_t *resp = json_object();
    if (!resp) {
        json_delete(top);
        return;
    }
    json_array_append_new(resps, resp);

    json_object_set_nocheck(resp, "rid", rid);
    json_object_set_new_nocheck(resp, "stream", json_string("closed"));
    dslink_ws_send_obj((struct wslay_event_context *) link->_ws, top);
    json_delete(top);

    if (!removeServerNode(link, node->parent)) {
        log_warn("Server node couldn't be removed properly!\n");
    }
}
static
void clear_server_action(DSLink *link, DSNode *node,
                          json_t *rid, json_t *params, ref_t *stream) {
    (void) params;
    (void) stream;
    json_t *top = json_object();
    if (!top) {
        return;
    }
    json_t *resps = json_array();
    if (!resps) {
        json_delete(top);
        return;
    }
    json_object_set_new_nocheck(top, "responses", resps);

    json_t *resp = json_object();
    if (!resp) {
        json_delete(top);
        return;
    }
    json_array_append_new(resps, resp);

    json_object_set_nocheck(resp, "rid", rid);
    json_object_set_new_nocheck(resp, "stream", json_string("closed"));
    dslink_ws_send_obj((struct wslay_event_context *) link->_ws, top);
    json_delete(top);

    removeAllDataModel(link,node->parent);
}
static
void discover_action(DSLink *link, DSNode *node,
                     json_t *rid, json_t *params, ref_t *stream) {
    (void) params;
    (void) stream;
    json_t *top = json_object();
    if (!top) {
        return;
    }
    json_t *resps = json_array();
    if (!resps) {
        json_delete(top);
        return;
    }
    json_object_set_new_nocheck(top, "responses", resps);

    json_t *resp = json_object();
    if (!resp) {
        json_delete(top);
        return;
    }
    json_array_append_new(resps, resp);

    json_object_set_nocheck(resp, "rid", rid);
    json_object_set_new_nocheck(resp, "stream", json_string("closed"));
    dslink_ws_send_obj((struct wslay_event_context *) link->_ws, top);
    json_delete(top);

    if(!isDiscovered(node->parent)) {
        ts61850Server serverPtr;
        if ( (serverPtr = (ts61850Server) (dslink_map_get(serverMap, (node->parent))->data)) != NULL ) {
            discover61850Server(link, node->parent, serverPtr->iedConn);
        } else {
            log_warn("Getting server from map failed!\n");
        }
    } else {
        log_info("Already discovered, clear first!\n");
    }

}

void responderInitClient(DSLink *link, DSNode *root, const char *nodeName, const char *hostName, int tcpPort) {

    char srvKey[129] = SERVER_NODE_KEY_INIT;
    strcat(srvKey,nodeName);
    DSNode *serverNode = dslink_node_create(root, srvKey, "node");
    if (!serverNode) {
        log_warn("Failed to create the server node\n");
        return;
    }

    if(dslink_node_set_meta(link, serverNode, "$name", json_string(nodeName)) != 0 ) {
        log_warn("Failed to set name of server node\n");
        return;
    }

    //serverNode->on_list_open = list_opened;
    //serverNode->on_list_close = list_closed;

    if (dslink_node_add_child(link, serverNode) != 0) {
        log_warn("Failed to add the server node to the root\n");
        dslink_node_tree_free(link, serverNode);
        return;
    }

    serverNode->data = dslink_ref(link,NULL);

    /*
     * create discover node
     * */
    DSNode *discoverNode = dslink_node_create(serverNode, "ndl_discoverserver", "node");
    if (!discoverNode) {
        log_warn("Failed to create discover action node\n");
        return;
    }

    discoverNode->on_invocation = discover_action;
    dslink_node_set_meta(link, discoverNode, "$name", json_string("Discover"));
    dslink_node_set_meta(link, discoverNode, "$invokable", json_string("read"));

    if (dslink_node_add_child(link, discoverNode) != 0) {
        log_warn("Failed to add discover action to the server node\n");
        dslink_node_tree_free(link, discoverNode);
        return;
    }

    /*
     * create edit node
     * */
    DSNode *editServerNode = dslink_node_create(serverNode, "ndl_editserver", "node");
    if (!editServerNode) {
        log_warn("Failed to create edit server node action node\n");
        return;
    }

    editServerNode->on_invocation = edit_server_action;
    dslink_node_set_meta(link, editServerNode, "$name", json_string("Edit"));
    dslink_node_set_meta(link, editServerNode, "$invokable", json_string("read"));

    json_t *params = json_array();

    json_t *name_row = json_object();
    json_object_set_new(name_row, "name", json_string("Name"));
    json_object_set_new(name_row, "type", json_string("string"));
    json_object_set_new(name_row, "default", json_string(serverNode->name + NODE_KEY_INIT_LEN));

    json_t *host_row = json_object();
    json_object_set_new(host_row, "name", json_string("Host"));
    json_object_set_new(host_row, "type", json_string("string"));
    json_object_set_new(host_row, "default", json_string(hostName));

    json_t *port_row = json_object();
    json_object_set_new(port_row, "name", json_string("Port"));
    json_object_set_new(port_row, "type", json_string("number"));
    json_object_set_new(port_row, "default", json_integer(tcpPort));

    json_array_append_new(params, name_row);
    json_array_append_new(params, host_row);
    json_array_append_new(params, port_row);

    dslink_node_set_meta(link, editServerNode, "$params", params);

    if (dslink_node_add_child(link, editServerNode) != 0) {
        log_warn("Failed to add editServerNode action to the root node\n");
        dslink_node_tree_free(link, editServerNode);
        return;
    }

    /*
    * create clear node
    * */
    DSNode *clearNode = dslink_node_create(serverNode, "ndl_clearserver", "node");
    if (!clearNode) {
        log_warn("Failed to create clear action node\n");
        return;
    }
    clearNode->on_invocation = clear_server_action;
    dslink_node_set_meta(link, clearNode, "$name", json_string("Clear"));
    dslink_node_set_meta(link, clearNode, "$invokable", json_string("read"));
    if (dslink_node_add_child(link, clearNode) != 0) {
        log_warn("Failed to add clear action to the server node\n");
        dslink_node_tree_free(link, clearNode);
        return;
    }

    /*
    * create remove node
    * */
    DSNode *removeNode = dslink_node_create(serverNode, "ndl_removeserver", "node");
    if (!removeNode) {
        log_warn("Failed to create remove action node\n");
        return;
    }

    removeNode->on_invocation = remove_server_action;
    dslink_node_set_meta(link, removeNode, "$name", json_string("Remove"));
    dslink_node_set_meta(link, removeNode, "$invokable", json_string("read"));

    if (dslink_node_add_child(link, removeNode) != 0) {
        log_warn("Failed to add remove action to the server node\n");
        dslink_node_tree_free(link, removeNode);
        return;
    }


    /*
     * create new 61850 server
     */

    ts61850Server newServer;
    newServer = iec61850ServerCreate();
    newServer->iedConn = IedConnection_create();
    strcpy(newServer->hostname, hostName);
    newServer->tcpPort = tcpPort;

    //assign 61850 connection to this server node
    if (setServerMap(serverNode, newServer)) {
        log_warn("Setting IED connection map failed!\n");
    }

    connect_61850_ied(newServer->iedConn, hostName, tcpPort);
    serverStatusCreate(link, serverNode);
}

void
reportCallbackFunction(void* parameter, ClientReport report) {
#ifdef DEVEL_DEBUG
    log_info("received report for %s with rptId %s\n", ClientReport_getRcbReference(report),
         ClientReport_getRptId(report));
#endif
    DSNode *rcbNode = (DSNode*) parameter;
//    log_info("RCB Node %s\n",rcbNode->name);
    if(rcbNode) {

        IedConnection iedConn = getServerIedConnection(rcbNode);
        ClientReportControlBlock rcb = rcbNode->data->data;
//        log_info("RCB: %s con: %d\n",rcb->objectReference,IedConnection_getState(iedConn));
        const char *dataSetRef = ClientReportControlBlock_getDataSetReference(rcb);
//        log_info("DataSet: %s\n",dataSetRef);
        DSNode *dataSetNode = getDataSetNodeFromRef(rcbNode,dataSetRef);
        DSLink *link = getDSLink(rcbNode);
        if(iedConn && rcb && dataSetNode && link) {
            LinkedList dataSetDirectory = (LinkedList)dataSetNode->data->data;


            MmsValue *dataSetValues = ClientReport_getDataSetValues(report);
//            if(ClientReport_hasDataReference(report)) {
//                log_info("dataset: %s\n", ClientReport_getDataSetName(report));
//                int i;
//                char buf[100];
//                for (i = 0; i < MmsValue_getArraySize(dataSetValues); i++) {
//                    MmsValue *dataVal = MmsValue_getElement(dataSetValues, i);
//                    log_info("%s: %s\n", ClientReport_getDataReference(report, i),
//                             MmsValue_printToBuffer(dataVal, buf, 90));
//                }
//            }


            if(dataSetDirectory) {

                int i;
                for (i = 0; i < LinkedList_size(dataSetDirectory); i++) {
                    ReasonForInclusion reason = ClientReport_getReasonForInclusion(report, i);

                    if (reason != IEC61850_REASON_NOT_INCLUDED) {

                        LinkedList entry = LinkedList_get(dataSetDirectory, i);

                        char *entryName = (char *) entry->data;

                        DSNode *daNode = getDANode(rcbNode, entryName);
                        if (daNode) {
//                            log_info("DA Node: %s\n", daNode->name);
                            DSNode *daValNode = dslink_node_get_path(daNode,DA_VALUE_REF_NODE_KEY);
                            if(daValNode)
                                setValueNodeFromMMS(link,daValNode,MmsValue_getElement(dataSetValues, i));
                        }

//                        log_info("  %s (included for reason %i)\n", entryName, reason);
                    }
                }
            }
        }
    } else {
        log_info("RCB NODE not found\n");
    }
}
