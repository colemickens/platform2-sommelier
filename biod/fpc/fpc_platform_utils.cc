// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <poll.h>

#include <base/logging.h>
#include <brillo/brillo_export.h>

#include "biod/fpc_biometrics_manager.h"

// The FPC sensor and biometric library requires platform-dependent functions to
// debug in the TEE environment and a timestamping method.
// PAL also requires the library user to provide I/O handlers.

#ifdef __cplusplus
extern "C" {
#endif

BRILLO_EXPORT void fp_pal_logprint(const char* log_message) {
  LOG(ERROR) << log_message;
}

BRILLO_EXPORT void fp_pal_get_timestamp(uint64_t* time) {
#if defined(__x86_64__) || defined(__amd64__)
  // We need to use RDTSC for SGX, clock_gettime() and friends aren't available.
  uint64_t low, high;
  __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
  *time = (high << 32) | low;
#else
#error "fp_pal_get_timestamp() is not implemented."
#endif
}

static int get_fp_sensor_fd() {
  return biod::FpcBiometricsManager::g_sensor_fd;
}

BRILLO_EXPORT int fp_pal_spi_writeread(uint8_t *access_buffer,
                                       size_t access_buffer_size) {
  int fd = get_fp_sensor_fd();
  ssize_t ret = write(fd, access_buffer, access_buffer_size);
  if (ret != access_buffer_size)
    return -1;
  ret = read(fd, access_buffer, access_buffer_size);
  if (ret != access_buffer_size)
    return -1;
  return 0;
}

BRILLO_EXPORT int fp_pal_spi_write(uint8_t *access_buffer,
                                   size_t access_buffer_size) {
  return fp_pal_spi_writeread(access_buffer, access_buffer_size);
}

BRILLO_EXPORT int fp_pal_wait_for_sensor_interrupt(void) {
  struct pollfd pfd;
  pfd.fd = get_fp_sensor_fd();
  if (pfd.fd < 0)
    return -1;
  pfd.events = POLLIN | POLLRDNORM;
  pfd.revents = 0;
  // TODO(b/37939568) libfp v0.8.0: fp_sensor_cancel() is not interrupting the
  // syscall, force a timeout in the mean time to avoid blocking forever.
  int ret = poll(&pfd, 1, biod::FpcBiometricsManager::kIRQTimeoutMs);
  if (ret <= 0)
    return -1;
  if (!(pfd.revents & (POLLIN | POLLRDNORM)))
    return -1;
  return 0;
}

#ifdef __cplusplus
}
#endif
