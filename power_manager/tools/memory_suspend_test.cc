// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This #define is needed to avoid an "inttypes.h has already been included
// before this header file, but without __STDC_FORMAT_MACROS defined." error
// from base/format_macros.h when gflags/gflags.h is included first.
#define __STDC_FORMAT_MACROS

#include <stdint.h>

#include <fcntl.h>
#include <gflags/gflags.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>

#include <base/format_macros.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

#define PATTERN(i) ((i % 1) ? 0x55555555 : 0xAAAAAAAA)

DEFINE_int64(size, 1024*1024*1024, "Amount of memory to allocate");
DEFINE_uint64(wakeup_count, 0, "Value read from /sys/power/wakeup_count");

void PrintAddrMap(void *vaddr) {
  int fd;
  uintptr_t page = reinterpret_cast<uintptr_t>(vaddr) / getpagesize();
  uint64_t page_data;

  fd = open("/proc/self/pagemap", O_RDONLY);
  CHECK_GE(fd, 0);
  CHECK_EQ(static_cast<uintptr_t>(lseek64(fd, page * 8, SEEK_SET)), page * 8);
  CHECK_EQ(read(fd, &page_data, 8), 8);
  printf("Vaddr: 0x%p   PFN=0x%llx  shift=%llu  present=%lld\n", vaddr,
         page_data & ((1LL << 55) - 1), (page_data & ((0x3fLL << 55))) >> 55,
         (page_data & (1LL << 63)) >> 63);
}

int Suspend(void) {
  return system(base::StringPrintf(
      "powerd_dbus_suspend --delay=0 --wakeup_count=%" PRIu64,
      FLAGS_wakeup_count).c_str());
}

uint32_t* Allocate(size_t size) {
  uint32_t *ptr;

  ptr = static_cast<uint32_t *>(malloc(size));
  CHECK(ptr);
  return ptr;
}

void Fill(uint32_t *ptr, size_t size) {
  for (size_t i = 0; i < size / sizeof(*ptr); i++) {
    *(ptr + i) = PATTERN(i);
  }
}

bool Check(uint32_t *ptr, size_t size) {
  bool success = true;

  for (size_t i = 0; i < size / sizeof(*ptr); i++) {
    if (*(ptr + i) != PATTERN(i)) {
      printf("Found changed value: Addr=%p val=0x%X, expected=0x%X\n",
             ptr + i, *(ptr + i), PATTERN(i));
      PrintAddrMap(ptr + i);
      success = false;
    }
  }
  return success;
}

int main(int argc, char* argv[]) {
  uint32_t *ptr;

  google::SetUsageMessage("\n"
      "  Fills memory with 0x55/0xAA patterns, performs a suspend, and checks\n"
      "  those patterns after resume. Will return 0 on success, 1 when the\n"
      "  suspend operation fails, and 2 when memory errors were detected.");
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK_EQ(argc, 1) << "Unexpected arguments. Try --help";

  ptr = Allocate(FLAGS_size);
  Fill(ptr, FLAGS_size);
  if (Suspend()) {
    printf("Error suspending\n");
    return 1;
  }
  if (Check(ptr, FLAGS_size))
    return 0;
  // The power_MemorySuspend Autotest depends on this value.
  return 2;
}
