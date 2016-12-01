//
// Created by ali on 25.09.2016.
//
#define LOG_TAG "client_handler"


#include <dslink/log.h>
#include <iec61850_server_defs.h>
#include "server_status.h"
#include "iec61850_client.h"

static
void serverSubbed(DSLink *link, DSNode *node) {
    (void) link;
#ifdef DEVEL_DEBUG
    log_info("Subscribed to %s\n", node->path);
#endif

    //get iedConn from the node
    ts61850Server serverPtr;
    if (serverPtr = (ts61850Server) (dslink_map_get(serverMap, (node->parent))->data)) {

        if(serverPtr->iedConn) {
            if (IedConnection_getState(serverPtr->iedConn) == IED_STATE_IDLE) {
#ifdef DEVEL_DEBUG
                log_info("IED state IDLE\n");
#endif
                dslink_node_set_value(link, node, json_string("Not connected"));
            } else if (IedConnection_getState(serverPtr->iedConn) == IED_STATE_CONNECTED) {
#ifdef DEVEL_DEBUG
                log_info("IED state CONNECTED\n");
#endif
                dslink_node_set_value(link, node, json_string("Connected"));
            } else if (IedConnection_getState(serverPtr->iedConn) == IED_STATE_CLOSED) {
#ifdef DEVEL_DEBUG
                log_info("IED state NOT CONNECTED\n");
#endif
                dslink_node_set_value(link, node, json_string("Connection closed"));
            } else {
                log_warn("IED state couldn't be realized!\n");
                dslink_node_set_value(link, node, json_string("??"));
            }
        } else{
            log_warn("Getting IED connection from server failed!\n");
        }
    } else{
        log_warn("Getting server from map failed!\n");
    }
}

static
void serverUnsubbed(DSLink *link, DSNode *node) {
    (void) link;
#ifdef DEVEL_DEBUG
    log_info("Unsubscribed to %s\n", node->path);
#endif
}

void serverStatusCreate(DSLink *link, DSNode *root) {
    DSNode *statusNode = dslink_node_create(root, "ndl_connstatus", "node");
    if (!statusNode) {
        log_warn("Failed to create the status node\n");
        return;
    }

    statusNode->on_subscribe = serverSubbed;
    statusNode->on_unsubscribe = serverUnsubbed;
    if (dslink_node_set_meta(link, statusNode, "$type", json_string("string")) != 0) {
        log_warn("Failed to set the type on the status\n");
        dslink_node_tree_free(link, statusNode);
        return;
    }

    if (dslink_node_set_meta(link, statusNode, "$name", json_string("Status")) != 0) {
        log_warn("Failed to set the name on the status\n");
        dslink_node_tree_free(link, statusNode);
        return;
    }

    if (dslink_node_set_value(link, statusNode, json_string("Initializing")) != 0) {
        log_warn("Failed to set the value on the status\n");
        dslink_node_tree_free(link, statusNode);
        return;
    }

    if (dslink_node_add_child(link, statusNode) != 0) {
        log_warn("Failed to add the status node to the root\n");
        dslink_node_tree_free(link, statusNode);
    }
}