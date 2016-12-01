#ifndef RESPONDER_INVOKE_H
#define RESPONDER_INVOKE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dslink/dslink.h>
#include <dslink/node.h>

void responderInitAddServer(DSLink *link, DSNode *root);

#ifdef __cplusplus
}
#endif

#endif // RESPONDER_INVOKE_H
