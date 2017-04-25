/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef BIOD_FPC_FP_PAL_H_
#define BIOD_FPC_FP_PAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * Print log message to device standard log system.
 */
void fp_pal_logprint(const char* log_message);

/**
 * get timestamp
 * return system time in micro seconds.
 *
 *  [out] reference to store the current time stamp in.
 *
 */
void fp_pal_get_timestamp(uint64_t* time);

/**
 * Spi write sensor data
 *
 * parma [in] access_buffer tx spi access buffer.
 * parma [in] access_buffer_size  size of tx spi access buffer.
 *
 * return 0 on success, negative value on error.
 */
int fp_pal_spi_write(uint8_t *access_buffer, size_t access_buffer_size);

/**
 * Spi write and read sensor data
 *
 * parma [in][out] access_buffer
 * parma [in] access_buffer_size size of spi access buffer.
 *
 * return 0 on success, negative value on error.
 */
int fp_pal_spi_writeread(uint8_t *access_buffer, size_t access_buffer_size);

/**
 * Wait for sensor interrupt.
 * function blocks until a sensor interrupt is received.
 *
 * @return 0 on success else an error occurd.
 */
int fp_pal_wait_for_sensor_interrupt(void);

#ifdef __cplusplus
}
#endif

#endif /* BIOD_FPC_FP_PAL_H_ */
