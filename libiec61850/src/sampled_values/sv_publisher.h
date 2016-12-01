/*
 *  sv_publisher.h
 *
 *  Copyright 2016 Michael Zillgith
 *
 *  This file is part of libIEC61850.
 *
 *  libIEC61850 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  libIEC61850 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libIEC61850.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */


#ifndef LIBIEC61850_SRC_SAMPLED_VALUES_SV_PUBLISHER_H_
#define LIBIEC61850_SRC_SAMPLED_VALUES_SV_PUBLISHER_H_

#include "libiec61850_platform_includes.h"

#ifdef __cplusplus
extern "C" {
#endif


#define IEC61850_SV_SMPSYNC_NOT_SYNCHRONIZED 0
#define IEC61850_SV_SMPSYNC_SYNCED_UNSPEC_LOCAL_CLOCK 1
#define IEC61850_SV_SMPSYNC_SYNCED_GLOBAL_CLOCK 2

#define IEC61850_SV_SMPMOD_PER_NOMINAL_PERIOD 0
#define IEC61850_SV_SMPMOD_SAMPLES_PER_SECOND 1
#define IEC61850_SV_SMPMOD_SECONDS_PER_SAMPLE 2

typedef struct sSampledValuesPublisher* SampledValuesPublisher;

typedef struct sSV_ASDU* SV_ASDU;

SampledValuesPublisher
SampledValuesPublisher_create(const char* interfaceId);

SV_ASDU
SampledValuesPublisher_addASDU(SampledValuesPublisher self, char* svID, char* datset, uint32_t confRev);

void
SampledValuesPublisher_setupComplete(SampledValuesPublisher self);

void
SampledValuesPublisher_publish(SampledValuesPublisher self);

void
SampledValuesPublisher_destroy(SampledValuesPublisher self);

void
SV_ASDU_resetBuffer(SV_ASDU self);

int
SV_ASDU_addINT8(SV_ASDU self);

void
SV_ASDU_setINT8(SV_ASDU self, int index, int8_t value);

int
SV_ASDU_addINT32(SV_ASDU self);

void
SV_ASDU_setINT32(SV_ASDU self, int index, int32_t value);

int
SV_ASDU_addFLOAT(SV_ASDU self);

void
SV_ASDU_setFLOAT(SV_ASDU self, int index, float value);

void
SV_ASDU_setSmpCnt(SV_ASDU self, uint16_t value);

void
SV_ASDU_increaseSmpCnt(SV_ASDU self);

void
SV_ASDU_setRefrTm(SV_ASDU self, uint64_t refrTm);

/**
 * \brief Set the sample mode of the ASDU
 *
 * If not set the transmitted ASDU will not contain an sampleMod value
 *
 * \param self the SV_ASDU
 *
 * \param smpMod one of IEC61850_SV_SMPMOD_PER_NOMINAL_PERIOD, IEC61850_SV_SMPMOD_SAMPLES_PER_SECOND or IEC61850_SV_SMPMOD_SECONDS_PER_SAMPLE
 */
void
SV_ASDU_setSmpMod(SV_ASDU self, uint8_t smpMod);

#ifdef __cplusplus
}
#endif


#endif /* LIBIEC61850_SRC_SAMPLED_VALUES_SV_PUBLISHER_H_ */
