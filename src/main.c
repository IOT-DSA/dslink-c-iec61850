#define LOG_TAG "main"

#include <dslink/log.h>
#include <dslink/storage/storage.h>
#include "add_server.h"


// Called to initialize your node structure.
void init(DSLink *link) {
    DSNode *superRoot = link->responder->super_root;

    responderInitAddServer(link, superRoot);

    // add link data
    json_t *linkData = json_object();
    json_object_set_nocheck(linkData, "test", json_true());
    link->link_data = linkData;

    log_info("Initialized!\n");
}

// Called when the DSLink is connected.
void connected(DSLink *link) {
    (void) link;
    log_info("Connected!\n");
}

// Called when the DSLink is disconnected.
// If this was not initiated by dslink_close,
// then a reconnection attempt is made.
void disconnected(DSLink *link) {
    (void) link;
    log_info("Disconnected!\n");
}

// The main function.
int main(int argc, char **argv) {
    DSLinkCallbacks cbs = { // Create our callback struct.
            init, // init_cb
            connected, //on_connected_cb
            disconnected, // on_disconnected_cb
            NULL // on_requester_ready_cb
    };


    // Initializes a DSLink and handles reconnection.
    // Pass command line arguments, our dsId,
    // are we a requester?, are we a responder?, and a reference to our callbacks.
    return dslink_init(argc, argv, "IEC61850", 0, 1, &cbs);
}
