// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef AVTEST_LABEL_DETECT_LABEL_DETECT_H_
#define AVTEST_LABEL_DETECT_LABEL_DETECT_H_
#include <linux/videodev2.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef HAS_VAAPI
#include <va/va.h>
#endif

/* main.c */
extern int verbose;
#define TRACE(...) do { if (verbose) printf(__VA_ARGS__); } while (0)

/* util.c */
extern int do_ioctl(int fd, int request, void* arg);
extern bool is_any_video_device(bool (*func)(int fd));
extern void convert_fourcc_to_str(uint32_t fourcc, char* str);

/* util_v4l2 */
extern bool is_v4l2_support_format(int fd, enum v4l2_buf_type buf_type,
    uint32_t fourcc);
extern bool is_hw_video_acc_device(int fd);

/* util_vaapi */
#ifdef HAS_VAAPI
bool is_vaapi_support_formats(VAProfile* profiles, VAEntrypoint entrypoint,
    unsigned int format);
#endif

/* detectors */
extern bool detect_webcam(void);
extern bool detect_video_acc_h264(void);
extern bool detect_video_acc_vp8(void);
extern bool detect_video_acc_enc_h264(void);
extern bool detect_video_acc_enc_vp8(void);

#endif  // AVTEST_LABEL_DETECT_LABEL_DETECT_H_
