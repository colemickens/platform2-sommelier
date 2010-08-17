// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of bootstat_log(), part of the Chromium OS 'bootstat'
// facility.

#include "bootstat.h"

#include <stdio.h>
#include <stddef.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>

//
// Paths to the statistics files we snapshot as part of the data to
// be logged.
//
static const char kUptimeStatisticsFileName[] = "/proc/uptime";

#if defined (__amd64__) || defined (__x86_64__) || defined (__i386__)
static const char kDiskStatisticsFileName[] = "/sys/block/sda/stat";
#elif defined (__arm__)
static const char kDiskStatisticsFileName[] = "/sys/block/mmcblk0/stat";
#else
#error "unknown processor type?"
#endif


//
// Maximum length of any pathname for storing event statistics.
// Arbitrarily chosen, but see the comment below about truncation.
//
#define MAX_STAT_PATH  128

//
// Output file creation mode:  0666, a.k.a. rw-rw-rw-.
//
static const int kFileCreationMode =
    (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);


static void append_logdata(const char* input_path,
                           const char* output_name_prefix,
                           const char* event_name)
{
  char output_path[MAX_STAT_PATH];
  char buffer[256];
  ssize_t num_read;
  int ifd, ofd;

  ifd = open(input_path, O_RDONLY);
  if (ifd < 0) {
    return;
  }

  //
  // We don't want the file name "/tmp/uptime-..." truncated
  // differently from the the name "/tmp/disk-...", so we truncate
  // event_name separately using the "%.*s" format.
  //
  // We expect that BOOTSTAT_MAX_EVENT_LEN is enough smaller than
  // MAX_STAT_PATH that output_path will never be truncated.
  //
  (void) snprintf(output_path, sizeof(output_path), "/tmp/%s-%.*s",
                  output_name_prefix,
                  BOOTSTAT_MAX_EVENT_LEN - 1, event_name);
  ofd = open(output_path, O_WRONLY | O_APPEND | O_CREAT,
             kFileCreationMode);
  if (ofd < 0) {
    (void) close(ifd);
    return;
  }

  while ((num_read = read(ifd, buffer, sizeof(buffer))) > 0) {
    ssize_t num_written = write(ofd, buffer, num_read);
    if (num_written != num_read)
      break;
  }
  (void) close(ofd);
  (void) close(ifd);
}


void bootstat_log(const char* event_name)
{
  append_logdata(kUptimeStatisticsFileName, "uptime", event_name);
  append_logdata(kDiskStatisticsFileName, "disk", event_name);
}
