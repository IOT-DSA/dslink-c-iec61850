//
// Created by ali on 12.11.2016.
//

#define LOG_TAG "dataset_handler"


#include <dslink/log.h>
#include <stdbool.h>
#include <string.h>
#include "dataset_handler.h"
#include "iec61850_server_defs.h"
#include "string_utilities.h"

bool getPrepDSListEnumStr(DSNode *node, char *enumStr) {
    DSNode *mainPrepDSNode = getMainPrepDatasetNode(node);
    if(mainPrepDSNode) {
        int i=0;
        dslink_map_foreach(mainPrepDSNode->children) {

            DSNode *tempNode = (DSNode *) (entry->value->data);

            if (strncmp(tempNode->name, DATASET_NODE_KEY_INIT, NODE_KEY_INIT_LEN) == 0) {
                i++;
                strcat(enumStr,tempNode->name + NODE_KEY_INIT_LEN);
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

void convertDataSetRefForNodeKey(char *dsRef) {
    StringUtils_replace(dsRef,'/','$');
}

/*
 * get the original Data Attribute Node from dataset member
 */
DSNode *getDANode(DSNode *node, const char* daRefWithFC) {
//    log_info("DA ref: %s\n",daRefWithFC);
    char tempDAStr[100];
    DSNode *serverNode = getServerNode(node);
    DSNode *scanNode = serverNode;
    char daRef[300];
    char fcStr[5];
    strcpy(daRef,daRefWithFC);
    char *tempPtr = strchr(daRef,'[');
    if(!tempPtr)
        return NULL;
    strncpy(fcStr,tempPtr,4);
    fcStr[4] = '\0';
    *tempPtr = '\0';

    char *headPtr = daRef;
    if(serverNode) {
        //LD
        strcpy(tempDAStr,LOGICAL_DEVICE_NODE_KEY_INIT);
        tempPtr = strchr(headPtr,'/');
        if(!tempPtr)
            return NULL;
        *tempPtr = '\0';
        strcat(tempDAStr,headPtr);
        scanNode = dslink_node_get_path(scanNode,tempDAStr);
        if(!scanNode)
            return NULL;
//        log_info("LD found %s\n",headPtr);
        //LN
        headPtr = tempPtr+1;
        tempPtr = strchr(headPtr,'.');
        if(!tempPtr)
            return NULL;
        *tempPtr = '\0';
        scanNode = dslink_node_get_path(scanNode,headPtr);
        if(!scanNode)
            return NULL;
//        log_info("LN found %s\n",headPtr);
        //DO - DA

        DSNode *newScanNode = NULL;
        headPtr = tempPtr+1;
        tempPtr = strchr(headPtr,'.');
        while(tempPtr) {
            *tempPtr = '\0';
            strcpy(tempDAStr,headPtr);
//            log_info("DO-DA search 1: %s\n",tempDAStr);
            newScanNode = dslink_node_get_path(scanNode,tempDAStr);
            if(newScanNode) {
                scanNode = newScanNode;
            } else {
                strcat(tempDAStr,fcStr);

//                log_info("DO-DA search 2: %s\n",tempDAStr);
                newScanNode = dslink_node_get_path(scanNode,tempDAStr);
                if(newScanNode)
                    scanNode = newScanNode;
                else
                    return NULL;
            }
            headPtr = tempPtr+1;
            tempPtr = strchr(headPtr,'.');
        }
        strcpy(tempDAStr,headPtr);
//        log_info("DO-DA search 3: %s\n",tempDAStr);
        newScanNode = dslink_node_get_path(scanNode,tempDAStr);
        if(newScanNode) {
            scanNode = newScanNode;
        } else {
            strcat(tempDAStr,fcStr);
//            log_info("DO-DA search 4: %s\n",tempDAStr);
            newScanNode = dslink_node_get_path(scanNode,tempDAStr);
            if(newScanNode)
                scanNode = newScanNode;
            else
                return NULL;
        }

    }
    if(!scanNode)
        log_info("DA Node couldn't be found!\n");

    return scanNode;
}

DSNode *getMainDatasetNode(DSNode *node) {
    DSNode *serverNode = getServerNode(node);
    DSNode *datasetNode = NULL;
    if(serverNode) {
        datasetNode = dslink_node_get_path(serverNode,DATASET_FOLDER_NODE_KEY);
        if(!datasetNode)
            log_info("Data Set node not found!\n");
    }

    return datasetNode;
}
DSNode *getMainPrepDatasetNode(DSNode *node) {
    DSNode *serverNode = getServerNode(node);
    DSNode *datasetNode = NULL;
    if(serverNode) {
        datasetNode = dslink_node_get_path(serverNode,DATASET_PREP_FOLDER_NODE_KEY);
        if(!datasetNode)
            log_info("prep dataset node couldn't be found!\n");
    }

    return datasetNode;
}
DSNode *getDataSetNodeFromRef(DSNode *node, const char* dataSetRef) {
    char dsRef[300] = DATASET_NODE_KEY_INIT;
    strcat(dsRef,dataSetRef);
    StringUtils_replace(dsRef,'$','.');
    StringUtils_replace(dsRef,'/','$');
    //log_info("getDataSetNodeFromRef: %s\n",dsRef);
    DSNode *dataSetNode = dslink_node_get_path(getMainDatasetNode(node),dsRef);
    if(!dataSetNode)
        log_info("DataSet Node not found!\n");
    return dataSetNode;
}
void removeDatasetNode(DSLink *link, DSNode *datasetNode) {
    DSNode *datasetFolderNode = datasetNode->parent;

    ref_t *n = dslink_map_remove_get(datasetFolderNode->children, datasetNode->name);
    if (n) {
        dslink_node_tree_free(link, n->data);
        dslink_decref(n);
    }
}
static
void remove_dataset_action(DSLink *link, DSNode *node,
                           json_t *rid, json_t *params, ref_t *stream) {

    IedConnection iedConn = getServerIedConnection(node);
    DSNode *datasetNode = node->parent;
    const char *dsRef = json_string_value(dslink_node_get_path(datasetNode,DS_OBJ_REF_NODE_KEY)->value);
    if(iedConn && dsRef) {
        IedClientError error;
        if(IedConnection_deleteDataSet(iedConn,&error,dsRef)) {
            removeDatasetNode(link, datasetNode);
        } else {
            log_warn("Data set couldn't be removed from server\n");
        }
    }

}
DSNode *createDatasetNode(DSLink *link, DSNode *mainDatasetNode, const char* dataSetRef, bool isDeletable) {
    char dsKey[300] = DATASET_NODE_KEY_INIT;
    strcat(dsKey, dataSetRef);
    convertDataSetRefForNodeKey(dsKey);

    if(mainDatasetNode) {
        DSNode *newDSNode = dslink_node_create(mainDatasetNode, dsKey, "node");
        if (!newDSNode) {
            log_warn("Failed to create the dataset node\n");
            return NULL;
        }
        if (dslink_node_set_meta(link, newDSNode, "$name", json_string(dataSetRef)) != 0) {
            log_warn("Failed to set the name of Dataset\n");
            dslink_node_tree_free(link, newDSNode);
            return NULL;
        }
        if (dslink_node_add_child(link, newDSNode) != 0) {
            log_warn("Failed to add the dataset node to the root\n");
            dslink_node_tree_free(link, newDSNode);
            return NULL;
        }
        /*
        * create Object Reference node
        */
        DSNode *objRefNode = dslink_node_create(newDSNode, DS_OBJ_REF_NODE_KEY, "node");
        if (!objRefNode) {
            log_warn("Failed to create the status node\n");
            return NULL;
        }
        if (dslink_node_set_meta(link, objRefNode, "$name", json_string("Obj. Ref.")) != 0) {
            log_warn("Failed to set the type on the status\n");
            dslink_node_tree_free(link, objRefNode);
            return NULL;
        }
        if (dslink_node_set_meta(link, objRefNode, "$type", json_string("string")) != 0) {
            log_warn("Failed to set the type on the status\n");
            dslink_node_tree_free(link, objRefNode);
            return NULL;
        }
        if (dslink_node_set_value(link, objRefNode, json_string(dataSetRef)) != 0) {
            log_warn("Failed to set the value on the status\n");
            dslink_node_tree_free(link, objRefNode);
            return NULL;
        }
        if (dslink_node_add_child(link, objRefNode) != 0) {
            log_warn("Failed to add the status node to the root\n");
            dslink_node_tree_free(link, objRefNode);
            return NULL;
        }
        /*
        * create Deletable node
        */
        DSNode *isDelNode = dslink_node_create(newDSNode, "deletable", "node");
        if (!isDelNode) {
            log_warn("Failed to create the status node\n");
            return NULL;
        }
        if (dslink_node_set_meta(link, isDelNode, "$name", json_string("Deletable")) != 0) {
            log_warn("Failed to set the type on the status\n");
            dslink_node_tree_free(link, isDelNode);
            return NULL;
        }
        if (dslink_node_set_meta(link, isDelNode, "$type", json_string("boolean")) != 0) {
            log_warn("Failed to set the type on the status\n");
            dslink_node_tree_free(link, isDelNode);
            return NULL;
        }
        if (dslink_node_set_value(link, isDelNode, json_boolean(isDeletable)) != 0) {
            log_warn("Failed to set the value on the status\n");
            dslink_node_tree_free(link, isDelNode);
            return NULL;
        }
        if (dslink_node_add_child(link, isDelNode) != 0) {
            log_warn("Failed to add the status node to the root\n");
            dslink_node_tree_free(link, isDelNode);
            return NULL;
        }
        /*
        * create remove node
        * */
        DSNode *removeNode = dslink_node_create(newDSNode, "removedataset", "node");
        if (!removeNode) {
            log_warn("Failed to create remove action node\n");
            return NULL;
        }

        removeNode->on_invocation = remove_dataset_action;
        dslink_node_set_meta(link, removeNode, "$name", json_string("Remove"));
        dslink_node_set_meta(link, removeNode, "$invokable", json_string("read"));

        if (dslink_node_add_child(link, removeNode) != 0) {
            log_warn("Failed to add remove action to the server node\n");
            dslink_node_tree_free(link, removeNode);
            return NULL;
        }

        return newDSNode;
    }
    return NULL;
}

void addMemberToDatasetNode(DSLink *link, DSNode *newDSNode, const char* memberName) {
    if(newDSNode) {

        char chMemberName[200];
        strcpy(chMemberName, memberName);
        char *slashPtr;
        slashPtr = strchr(chMemberName,'/');
        *slashPtr = '$';
        DSNode *memberNode = dslink_node_create(newDSNode, chMemberName, "node");
        if (!memberNode) {
            log_warn("Failed to create the status node\n");
            return;
        }
        if (dslink_node_set_meta(link, memberNode, "$name", json_string(memberName)) != 0) {
            log_warn("Failed to set the type on the status\n");
            dslink_node_tree_free(link, memberNode);
            return;
        }
        if (dslink_node_add_child(link, memberNode) != 0) {
            log_warn("Failed to add the status node to the root\n");
            dslink_node_tree_free(link, memberNode);
            return;
        }

        DSNode *daNode = getDANode(newDSNode,memberName);
        if(daNode) {
#ifdef DEVEL_DEBUG
            log_info("DA found: %s\n",daNode->name);
#endif
            memberNode->data = dslink_ref(daNode,NULL);
        }
    }
}
static
void rm_member_prepds_action(DSLink *link, DSNode *node,
                          json_t *rid, json_t *params, ref_t *stream) {

    DSNode *datasetNode = node->parent->parent;

    ref_t *n = dslink_map_remove_get(datasetNode->children, node->parent->name);
    if (n) {
        dslink_node_tree_free(link, n->data);
        dslink_decref(n);
    }
}
void addMemberToPrepDatasetNode(DSLink *link, DSNode *prepDSNode, const char* memberName) {
    if(prepDSNode) {
        char chMemberName[400] = DATASET_ELEMENT_NODE_KEY_INIT;
        strcat(chMemberName, memberName);
        char *slashPtr;
        slashPtr = strchr(chMemberName,'/');
        *slashPtr = '$';
        DSNode *memberNode = dslink_node_create(prepDSNode, chMemberName, "node");
        if (!memberNode) {
            log_warn("Failed to create the status node\n");
            return;
        }

        if (dslink_node_set_meta(link, memberNode, "$name", json_string(memberName)) != 0) {
            log_warn("Failed to set the type on the status\n");
            dslink_node_tree_free(link, memberNode);
            return;
        }
        if (dslink_node_add_child(link, memberNode) != 0) {
            log_warn("Failed to add the status node to the root\n");
            dslink_node_tree_free(link, memberNode);
            return;
        }
        /*
        * create remove node
        * */
        DSNode *removeNode = dslink_node_create(memberNode, "rm_mem_prepds", "node");
        if (!removeNode) {
            log_warn("Failed to create remove action node\n");
            return;
        }
        removeNode->on_invocation = rm_member_prepds_action;
        dslink_node_set_meta(link, removeNode, "$name", json_string("Remove"));
        dslink_node_set_meta(link, removeNode, "$invokable", json_string("read"));
        if (dslink_node_add_child(link, removeNode) != 0) {
            log_warn("Failed to add remove action to the server node\n");
            dslink_node_tree_free(link, removeNode);
            return;
        }
    }
}

void removePrepDS(DSLink *link, DSNode *prepDSNode) {
    DSNode *datasetNode = prepDSNode->parent;

    ref_t *n = dslink_map_remove_get(datasetNode->children, prepDSNode->name);
    if (n) {
        dslink_node_tree_free(link, n->data);
        dslink_decref(n);
    }
}
static
void remove_prepds_action(DSLink *link, DSNode *node,
                               json_t *rid, json_t *params, ref_t *stream) {

    removePrepDS(link, node->parent);
//    DSNode *datasetNode = node->parent->parent;
//
//    ref_t *n = dslink_map_remove_get(datasetNode->children, node->parent->name);
//    if (n) {
//        dslink_node_tree_free(link, n->data);
//        dslink_decref(n);
//    }
}
static
void create_ds_onserver_action(DSLink *link, DSNode *node,
                         json_t *rid, json_t *params, ref_t *stream) {

    DSNode *datasetNode = node->parent;
    LinkedList linkedList = LinkedList_create ();

    dslink_map_foreach(datasetNode->children) {
        DSNode *tempNode = (DSNode *) (entry->value->data);
        if(strncmp(tempNode->name, DATASET_ELEMENT_NODE_KEY_INIT, NODE_KEY_INIT_LEN) == 0) {
            LinkedList_add (linkedList,json_string_value(dslink_map_get(tempNode->meta_data,"$name")->data));
        }
    }

    const char *dsRef = json_string_value(dslink_node_get_path(datasetNode,DS_OBJ_REF_NODE_KEY)->value);
    //char *dsName = datasetNode->name+NODE_KEY_INIT_LEN;

    if(LinkedList_size(linkedList) > 0) {
        IedClientError error;
        IedConnection iedConn = getServerIedConnection(node);
        IedConnection_createDataSet (iedConn, &error, dsRef, linkedList);



        if(error != 0) {
            log_warn("Dataset couldn't be created on server %d\n",error);
        } else {

            if(!getDatasetInfoFromServerCreateNode(link,node,iedConn,dsRef)){
                log_warn("Data set couldn't be created!\n");
            } else {
                removePrepDS(link, datasetNode);
            }
        }
    }

}
bool createPrepDatasetNode(DSLink *link, DSNode *lnNode, const char* dsRef) {
    DSNode *datasetNode = getMainDatasetNode(lnNode);
    DSNode *prepDatasetNode = getMainPrepDatasetNode(lnNode);

    char dsKey[300] = DATASET_NODE_KEY_INIT;
    strcat(dsKey, dsRef);
    convertDataSetRefForNodeKey(dsKey);

    if(datasetNode && prepDatasetNode) {
        if (dslink_map_get(datasetNode->children, dsKey) ||
                dslink_map_get(prepDatasetNode->children, dsKey)) {
            log_warn("There is another dataset with the same name: %s\n", dsRef);
            return false;
        } else {

            DSNode *newDSNode = dslink_node_create(prepDatasetNode, dsKey, "node");
            if (!newDSNode) {
                log_warn("Failed to create the dataset node\n");
                return NULL;
            }
            if (dslink_node_set_meta(link, newDSNode, "$name", json_string(dsRef)) != 0) {
                log_warn("Failed to set the name of Dataset\n");
                dslink_node_tree_free(link, newDSNode);
                return NULL;
            }
            if (dslink_node_add_child(link, newDSNode) != 0) {
                log_warn("Failed to add the dataset node to the root\n");
                dslink_node_tree_free(link, newDSNode);
                return NULL;
            }
            /*
            * create Object Reference node
            */
            DSNode *objRefNode = dslink_node_create(newDSNode,DS_OBJ_REF_NODE_KEY, "node");
            if (!objRefNode) {
                log_warn("Failed to create the reference node\n");
                return NULL;
            }
            if (dslink_node_set_meta(link, objRefNode, "$name", json_string("Obj. Ref.")) != 0) {
                log_warn("Failed to set the type on the reference\n");
                dslink_node_tree_free(link, objRefNode);
                return NULL;
            }
            if (dslink_node_set_meta(link, objRefNode, "$type", json_string("string")) != 0) {
                log_warn("Failed to set the type on the reference\n");
                dslink_node_tree_free(link, objRefNode);
                return NULL;
            }
            if (dslink_node_set_value(link, objRefNode, json_string(dsRef)) != 0) {
                log_warn("Failed to set the value on the reference\n");
                dslink_node_tree_free(link, objRefNode);
                return NULL;
            }
            if (dslink_node_add_child(link, objRefNode) != 0) {
                log_warn("Failed to add the reference node\n");
                dslink_node_tree_free(link, objRefNode);
                return NULL;
            }
            /*
            * create create node
            * */
            DSNode *createNode = dslink_node_create(newDSNode, "createdataset", "node");
            if (!createNode) {
                log_warn("Failed to create create dataset action node\n");
                return NULL;
            }
            createNode->on_invocation = create_ds_onserver_action;
            dslink_node_set_meta(link, createNode, "$name", json_string("Create on Server"));
            dslink_node_set_meta(link, createNode, "$invokable", json_string("read"));
            if (dslink_node_add_child(link, createNode) != 0) {
                log_warn("Failed to add clear action to the server node\n");
                dslink_node_tree_free(link, createNode);
                 NULL;
            }

            /*
            * create remove node
            * */
            DSNode *removeNode = dslink_node_create(newDSNode, "removedataset", "node");
            if (!removeNode) {
                log_warn("Failed to create remove action node\n");
                return NULL;
            }

            removeNode->on_invocation = remove_prepds_action;
            dslink_node_set_meta(link, removeNode, "$name", json_string("Remove"));
            dslink_node_set_meta(link, removeNode, "$invokable", json_string("read"));

            if (dslink_node_add_child(link, removeNode) != 0) {
                log_warn("Failed to add remove action to the server node\n");
                dslink_node_tree_free(link, removeNode);
                return NULL;
            }
            return true;
        }
    }
    return false;
}

bool getDatasetInfoFromServerCreateNode(DSLink *link, DSNode *currNode, IedConnection iedConn, const char *dsRef) {

    IedClientError error;
    bool isDeletable;

    LinkedList dataSetMembers = IedConnection_getDataSetDirectory(iedConn, &error, dsRef,
                                                                  &isDeletable);

    if(dataSetMembers && (error == 0)) {

        DSNode *newDSNode = createDatasetNode(link, getMainDatasetNode(currNode), dsRef, isDeletable);

        if(newDSNode) {
            newDSNode->data = dslink_ref(dataSetMembers,NULL);

            LinkedList dataSetMemberRef = LinkedList_getNext(dataSetMembers);

            while (dataSetMemberRef != NULL) {

                char *memberRef = (char *) dataSetMemberRef->data;
#ifdef DEVEL_DEBUG
                log_info("      %s\n", memberRef);
#endif

                addMemberToDatasetNode(link, newDSNode, memberRef);

                dataSetMemberRef = LinkedList_getNext(dataSetMemberRef);
            }

            return true;
        } else
            return false;
    } else
        return false;
}