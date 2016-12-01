#define LOG_TAG "invoke"

#include <dslink/log.h>
#include <dslink/ws.h>
#include "client_handler.h"


static
void invokeAddServer(DSLink *link, DSNode *node,
                      json_t *rid, json_t *params, ref_t *stream_ref) {
    int portNo;
    const char *host;
    const char *name;

    (void) node;
    (void) params;
    (void) stream_ref;

    log_info("invokeAddServer\n");

    name = json_string_value(json_object_get(params, "Name"));
    host = json_string_value(json_object_get(params, "Host"));
    portNo = json_integer_value(json_object_get(params, "Port"));
    if(name && host && portNo)
    {

//        log_info("Name: %s\n",name);
//        log_info("Host: %s\n",host);
//        log_info("Port: %d\n",portNo);
        responderInitClient(link, node->parent,name,host,portNo);
    }
    else
    {
        log_warn("Missing  parameter\n");
    }

    //json_t *msg = json_incref(json_object_get(params, "input"));


}


void responderInitAddServer(DSLink *link, DSNode *root) {

    DSNode *addServerNode = dslink_node_create(root, "addserver", "node");
    if (!addServerNode) {
        log_warn("Failed to create add server node action node\n");
        return;
    }

    addServerNode->on_invocation = invokeAddServer;
    dslink_node_set_meta(link, addServerNode, "$name", json_string("Add Server"));
    dslink_node_set_meta(link, addServerNode, "$invokable", json_string("read"));

    json_t *params = json_array();

    json_t *name_row = json_object();
    json_object_set_new(name_row, "name", json_string("Name"));
    json_object_set_new(name_row, "type", json_string("string"));

    json_t *host_row = json_object();
    json_object_set_new(host_row, "name", json_string("Host"));
    json_object_set_new(host_row, "type", json_string("string"));
    json_object_set_new(host_row, "default", json_string("0.0.0.0"));

    json_t *port_row = json_object();
    json_object_set_new(port_row, "name", json_string("Port"));
    json_object_set_new(port_row, "type", json_string("number"));
    json_object_set_new(port_row, "default", json_integer(102));

    json_array_append_new(params, name_row);
    json_array_append_new(params, host_row);
    json_array_append_new(params, port_row);

    dslink_node_set_meta(link, addServerNode, "$params", params);

    if (dslink_node_add_child(link, addServerNode) != 0) {
        log_warn("Failed to add addServerNode action to the root node\n");
        dslink_node_tree_free(link, addServerNode);
        return;
    }
}
