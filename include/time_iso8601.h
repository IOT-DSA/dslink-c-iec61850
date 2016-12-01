//
// Created by ali on 04.10.2016.
//

#ifndef SDK_DSLINK_C_TIME_ISO8601_H
#define SDK_DSLINK_C_TIME_ISO8601_H

#include <time.h>
#include <stdint.h>

/*
 * returns mseconds time if successes
 * returns 0 if fails
 */
uint64_t parseIso8601Generic (char *text, time_t *isotime, char *flag);

/*
 * returns mseconds time if successes
 * returns 0 if fails
 */
uint64_t parseIso8601 (const char *text);

char *getUtcTimeString(uint64_t msTime, char *buffer);

#endif //SDK_DSLINK_C_TIME_ISO8601_H
