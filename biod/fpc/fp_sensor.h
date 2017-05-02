/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef BIOD_FPC_FP_SENSOR_H_
#define BIOD_FPC_FP_SENSOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>

/*
 * Opens the sensor library.
 *
 * fp_sensor_open is called once before subsequent usage of sensor api's.
 * fp_sensor_close must be called when sensor functionality is no longer
 * needed. The fd parameter carries an open file descriptor to the sensor
 * driver.
 *
 * Returns 0 on success, negative error code (such as -ENOMEM) on failure.
 */
int fp_sensor_open(int fd);

/*
 * Closes the sensor library and frees resources held by the library.
 *
 * Returns 0 on success, negative error code on failure.
 */
int fp_sensor_close(void);

/*
 * Retrieves product information about the sensor.
 *
 * Returns 0 on success, negative error code (such as -EINVAL) on failure.
 */
int fp_sensor_get_model(uint32_t *vendor_id, uint32_t *product_id,
                        uint32_t *model_id, uint32_t *version);

/*
 * Retrieves the pixel format used by the sensor library.
 * This is a fourcc value defined by V4L2 API.
 * Could be a new define for biometric sensors or V4L2_PIX_FMT_GREY.
 *
 * Returns 0 on success, negative error code (such as -EINVAL) on failure.
 */
int fp_sensor_get_pixel_format(uint32_t *pixel_format);

/*
 * Returns the size of image data returned from the sensor.
 *
 * Returns negative error code on failure, or size of image data in bytes.
 */
ssize_t fp_sensor_get_image_data_size(void);

/*
 * Retrieves the width and height in pixels of images captured from the sensor.
 *
 * Returns 0 on success, negative error code (such as -EINVAL) on failure.
 */
int fp_sensor_get_image_dimensions(uint32_t *width, uint32_t *height);

/*
 * Acquires a fingerprint image.
 *
 * Function blocks waiting for a finger to be placed on sensor and then
 * captures an image. The operation can be cancelled by calling
 * fp_sensor_cancel. The image_data parameter points to an image data buffer
 * allocated by the caller.
 *
 * Returns:
 * - 0 on success
 * - negative value on error
 * - FP_SENSOR_LOW_IMAGE_QUALITY on image captured but quality is too low
 * - FP_SENSOR_TOO_FAST on finger removed before image was captured
 */
#define FP_SENSOR_LOW_IMAGE_QUALITY    1
#define FP_SENSOR_TOO_FAST             2
int fp_sensor_acquire_image(uint8_t *image_data, size_t size);

/*
 * Waits for finger to be lifted from sensor.
 *
 * The operation blocks as long as a finger is present on the sensor.
 * The operation can be cancelled by calling fp_sensor_cancel.
 *
 * Returns 0 on success, negative error code on failure.
 */
int fp_sensor_wait_finger_up(void);

/*
 * Cancels an ongoing blocking fp lib operation.
 *
 * Returns 0 on success, negative error code on failure.
 */
int fp_sensor_cancel(void);

#ifdef __cplusplus
}
#endif

#endif  /* BIOD_FPC_FP_SENSOR_H_ */
