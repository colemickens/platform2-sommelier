// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API for C and C++ bindings for unit tests for the Chromium OS
// 'bootstat' facility.  This interface is meant to be used by unit
// test code only, and is not intended to be exported for general
// use.

#ifndef BOOTSTAT_TEST_H_
#define BOOTSTAT_TEST_H_

#if defined(__cplusplus)
extern "C" {
#endif

extern void bootstat_set_output_directory(const char*);
extern void bootstat_set_uptime_file_name(const char*);
extern void bootstat_set_disk_file_name(const char*);

#if defined(__cplusplus)
}
#endif
#endif  // BOOTSTAT_TEST_H_
