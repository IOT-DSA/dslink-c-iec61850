//
// Created by ali on 20.09.2016.
//

#ifndef SDK_DSLINK_C_CLIENT_HANDLER_H
#define SDK_DSLINK_C_CLIENT_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dslink/dslink.h>
#include <dslink/node.h>

void responderInitClient(DSLink *link, DSNode *root, const char* nodeName, const char* hostName, int tcpPort);

#ifdef __cplusplus
}
#endif



#endif //SDK_DSLINK_C_CLIENT_HANDLER_H
