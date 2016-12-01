//
// Created by ali on 25.09.2016.
//

#ifndef SDK_DSLINK_C_SERVER_STATUS_H
#define SDK_DSLINK_C_SERVER_STATUS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <dslink/dslink.h>
#include <dslink/node.h>

extern Map *serverMap;

void serverStatusCreate(DSLink *link, DSNode *root);

#ifdef __cplusplus
}
#endif

#endif //SDK_DSLINK_C_SERVER_STATUS_H
