//
// Created by ali on 04.10.2016.
//

#include "time_iso8601.h"
#include <stdio.h>
#include "conversions.h"

#define DAYFLAG    0x001000
#define DHMSFLAG   0x001111
#define HOURFLAG   0x000100
#define HMSFLAG    0x000111
#define MINFLAG    0x000010
#define MSFLAG     0x000011
#define SECFLAG    0x000001

uint64_t parseIso8601Generic (char *text, time_t *isotime, char *flag) {
    char *c;
    int num;

    struct tm tmstruct;

    unsigned int year = 0;
    unsigned int month = 0;
    unsigned int seconds = 0;
    unsigned int minutes = 0;
    unsigned int hours = 0;
    unsigned int days = 0;
    unsigned int mseconds = 0;
    uint64_t totalTimeMs = 0;

    int dateflags = 0;   /* flag which date component we've seen */

    c = text;
    *isotime = 0;

    if (*c++ == 'P') {
/* duration */
        *flag = 'D';
        while (*c != '\0') {
            num = 0;
            while (*c >= '0' && *c <= '9') {
/* assumes ASCII sequence! */
                num = 10 * num + *c++ - '0';
            }

            switch (*c++) {
                case 'D':
                    if (dateflags & DHMSFLAG) {
/* day, hour, min or sec already set */
                        return 0;
                    } else {
                        dateflags |= DAYFLAG;
                        days = num;
                    }
                    break;
                case 'H':
                    if (dateflags & HMSFLAG) {
/* hour, min or sec already set */
                        return 0;
                    } else {
                        dateflags |= DAYFLAG;
                        hours = num;
                    }
                    break;
                case 'M':
                    if (dateflags & MSFLAG) {
/* min or sec already set */
                        return 0;
                    } else {
                        dateflags |= MINFLAG;
                        minutes = num;
                    }
                    break;
                case 'S':
                    if (dateflags & SECFLAG) {
/* sec already set */
                        return 0;
                    } else {
                        dateflags |= SECFLAG;
                        seconds = num;
                    }
                    break;
                default:
                    return 0;
            }
        }
        *isotime = seconds + 60 * minutes + 3600 * hours +
                  86400 * days;
    } else {
/* point in time, must be one of
    CCYYMMDD
    CCYY-MM-DD
    CCYYMMDDTHHMM
    CCYY-MM-DDTHH:MM
    CCYYMMDDTHHMMSS
    CCYY-MM-DDTHH:MM:SS
*/
        c = text;
        *flag = 'T';

/* NOTE: we have to check for the extended format first,
   because otherwise the separting '-' will be interpreted
   by sscanf as signs of a 1 digit integer .... :-(  */

        if (sscanf(text, "%4u-%2u-%2u", &year, &month, &days) == 3) {
            c += 10;
        } else if (sscanf(text, "%4u%2u%2u", &year, &month, &days) == 3) {
            c += 8;
        } else {
            return 0;
        }

        tmstruct.tm_year = year - 1900;
        tmstruct.tm_mon = month - 1;
        tmstruct.tm_mday = days;

        if (*c == '\0') {
            tmstruct.tm_hour = 0;
            tmstruct.tm_sec = 0;
            tmstruct.tm_min = 0;
            *isotime = timegm(&tmstruct);//mktime(&tmstruct);
        } else if (*c == 'T') {
/* time of day part */
            c++;
            if (sscanf(c, "%2u%2u", &hours, &minutes) == 2) {
                c += 4;
            } else if (sscanf(c, "%2u:%2u", &hours, &minutes) == 2) {
                c += 5;
            } else {
                return 0;
            }

            if (*c == ':') {
                c++;
            }

            if (*c != '\0') {
                if (sscanf(c, "%2u", &seconds) == 1) {
                    c += 2;
                } else {
                    return 0;
                }
//                if (*c != '\0') {      /* something left? */
//                    return 0;
//                }
            }
            tmstruct.tm_hour = hours;
            tmstruct.tm_min = minutes;
            tmstruct.tm_sec = seconds;
            *isotime = timegm(&tmstruct);//mktime(&tmstruct);

            if (*c == '\0') {
                totalTimeMs = *isotime * 1000;
            }
            else if(*c == '.') {
                c += 1;
                if (sscanf(c, "%3u", &mseconds) == 1) {
                    c += 3;
                    totalTimeMs = (*isotime * 1000) + mseconds;
                } else {
                    return 0;
                }
                if (*c != 'Z') {
                    return 0;
                }
            } else {
                return 0;
            }


        } else {
            return 0;
        }
    }

    return totalTimeMs;
}

uint64_t parseIso8601 (const char *text) {
    const char *c;

    struct tm tmstruct;

    time_t isotime;

    unsigned int year = 0;
    unsigned int month = 0;
    unsigned int seconds = 0;
    unsigned int minutes = 0;
    unsigned int hours = 0;
    unsigned int days = 0;
    unsigned int mseconds = 0;
    uint64_t totalTimeMs = 0;

    c = text;
    isotime = 0;

/* point in time, must be one of
    CCYYMMDD
    CCYY-MM-DD
    CCYYMMDDTHHMM
    CCYY-MM-DDTHH:MM
    CCYYMMDDTHHMMSS
    CCYY-MM-DDTHH:MM:SS
*/

/* NOTE: we have to check for the extended format first,
   because otherwise the separting '-' will be interpreted
   by sscanf as signs of a 1 digit integer .... :-(  */

    if (sscanf(text, "%4u-%2u-%2u", &year, &month, &days) == 3) {
        c += 10;
    } else if (sscanf(text, "%4u%2u%2u", &year, &month, &days) == 3) {
        c += 8;
    } else {
        return 0;
    }

    tmstruct.tm_year = year - 1900;
    tmstruct.tm_mon = month - 1;
    tmstruct.tm_mday = days;

    if (*c == '\0') {
        tmstruct.tm_hour = 0;
        tmstruct.tm_sec = 0;
        tmstruct.tm_min = 0;
        isotime = timegm(&tmstruct);//mktime(&tmstruct);
        totalTimeMs = isotime * 1000;
    } else if (*c == 'T') {
/* time of day part */
        c++;
        if (sscanf(c, "%2u%2u", &hours, &minutes) == 2) {
            c += 4;
        } else if (sscanf(c, "%2u:%2u", &hours, &minutes) == 2) {
            c += 5;
        } else {
            return 0;
        }

        if (*c == ':') {
            c++;
        }

        if (*c != '\0') {
            if (sscanf(c, "%2u", &seconds) == 1) {
                c += 2;
            } else {
                return 0;
            }
        }
        tmstruct.tm_hour = hours;
        tmstruct.tm_min = minutes;
        tmstruct.tm_sec = seconds;
        isotime = timegm(&tmstruct);//mktime(&tmstruct);

        if (*c == 'Z') {
            totalTimeMs = isotime * 1000;
        } else if (*c == '.') {
            c += 1;
            if (sscanf(c, "%3u", &mseconds) == 1) {
                c += 3;
                totalTimeMs = (isotime * 1000) + mseconds;
            } else {
                return 0;
            }
            if (*c != 'Z') {
                return 0;
            }
        } else {
            return 0;
        }


    } else {
        return 0;
    }


    return totalTimeMs;
}

char *getUtcTimeString(uint64_t msTime, char *buffer)
{
//    struct tm *timeTm;
//    timeTm = gmtime(&timeMs);
//    strftime(buf, 25, "%FT%T.", timeTm);
//    Conversions_intToStringBuffer((timeMs % 1000), 3, buf + 20);
//    buf[23] = 'Z';
//    buf[24] = '\0';
//    return buf;

    /*
     * iso8601: 2011-10-08T07:07:09.000Z
     */

    int msPart = (msTime % 1000);

    time_t unixTime = (msTime / 1000);

    struct tm tmTime;

    gmtime_r(&unixTime, &tmTime);

    Conversions_intToStringBuffer(tmTime.tm_year + 1900, 4, (uint8_t*)buffer);
    buffer[4] = '-';
    Conversions_intToStringBuffer(tmTime.tm_mon + 1, 2, (uint8_t*)buffer + 5);
    buffer[7] = '-';
    Conversions_intToStringBuffer(tmTime.tm_mday, 2, (uint8_t*)buffer + 8);
    buffer[10] = 'T';
    Conversions_intToStringBuffer(tmTime.tm_hour, 2, (uint8_t*)buffer + 11);
    buffer[13] = ':';
    Conversions_intToStringBuffer(tmTime.tm_min, 2, (uint8_t*)buffer + 14);
    buffer[16] = ':';
    Conversions_intToStringBuffer(tmTime.tm_sec, 2, (uint8_t*)buffer + 17);
    buffer[19] = '.';
    Conversions_intToStringBuffer(msPart, 3, (uint8_t*)buffer + 20);
    buffer[23] = 'Z';
    buffer[24] = 0;

    return buffer;
}
