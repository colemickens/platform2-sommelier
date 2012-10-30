// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <gflags/gflags.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/logging.h"

#define PATTERN(i) ((i % 1) ? 0x55555555 : 0xAAAAAAAA)

DEFINE_int64(size, 1024*1024*1024, "Amount of memory to allocate");

void PrintAddrMap(void *vaddr) {
  char filename[32];
  int fd;
  uintptr_t page = reinterpret_cast<uintptr_t>(vaddr) / getpagesize();
  uint64 page_data;

  CHECK_LT(snprintf(filename, sizeof(filename), "/proc/%d/pagemap", getpid()),
           static_cast<int>(sizeof(filename)));
  fd = open(filename, O_RDONLY);
  CHECK(fd);
  CHECK_EQ(static_cast<uintptr_t>(lseek(fd, page * 8, SEEK_SET)), page * 8);
  CHECK_EQ(read(fd, &page_data, 8), 8);
  printf("Vaddr: 0x%p   PFN=0x%llx  shift=0x%llx  present=%lld\n", vaddr,
         page_data & ((1LL << 55) - 1), (page_data & ((0x3fLL << 55))) >> 55,
         (page_data & (1LL << 63)) >> 63);
}

int Suspend(void) {
  // TODO(crosbug.com/36995): Add dbus suspend ability
  return system("powerd_suspend");
}

uint32* Allocate(size_t size) {
  uint32 *ptr;

  ptr = static_cast<uint32 *>(malloc(size));
  CHECK(ptr);
  return ptr;
}

void Fill(uint32 *ptr, size_t size) {
  for (size_t i = 0; i < size / sizeof(*ptr); i++) {
    *(ptr + i) = PATTERN(i);
  }
}

bool Check(uint32 *ptr, size_t size) {
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
  uint32 *ptr;

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
  return 1;
}
