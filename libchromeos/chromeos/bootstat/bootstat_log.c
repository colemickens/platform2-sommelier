// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of bootstat_log(), part of the Chromium OS 'bootstat'
// facility.

#include "bootstat.h"
#include "bootstat_test.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static const char kBootstageMarkFile[] = "/sys/kernel/debug/bootstage/mark";

//
// Default path to directory where output statistics will be stored.
//
static const char kDefaultOutputDirectoryName[] = "/tmp";

//
// Paths to the statistics files we snapshot as part of the data to
// be logged.
//
static const char kDefaultUptimeStatisticsFileName[] = "/proc/uptime";

#if defined (__amd64__) || defined (__x86_64__) || defined (__i386__)
static const char kDefaultDiskStatisticsFileName[] = "/sys/block/sda/stat";
#elif defined (__arm__)
static const char kDefaultDiskStatisticsFileName[] = "/sys/block/mmcblk0/stat";
#else
#error "unknown processor type?"
#endif

static const char *output_directory_name = kDefaultOutputDirectoryName;
static const char *uptime_statistics_file_name =
    kDefaultUptimeStatisticsFileName;
static const char *disk_statistics_file_name = kDefaultDiskStatisticsFileName;

static void append_logdata(const char* input_path,
                           const char* output_name_prefix,
                           const char* event_name)
{
  const mode_t kFileCreationMode =
      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  char output_path[PATH_MAX];
  char buffer[256];
  ssize_t num_read;
  int ifd, ofd;
  int output_path_len;

  ifd = open(input_path, O_RDONLY);
  if (ifd < 0) {
    return;
  }

  //
  // For those not up on the more esoteric features of printf
  // formats:  the "%.*s" format is used to truncate the event name
  // to the proper number of characters..
  //
  // The assertion for output_path overflow should only be able to
  // fail if output_directory_name is changed from its default,
  // which can only happen in unit tests, and then only in the event
  // of a serious test bug.
  //
  output_path_len = snprintf(output_path, sizeof(output_path), "%s/%s-%.*s",
                             output_directory_name,
                             output_name_prefix,
                             BOOTSTAT_MAX_EVENT_LEN - 1, event_name);
  assert(output_path_len < sizeof(output_path));
  ofd = open(output_path, O_WRONLY | O_APPEND | O_CREAT | O_NOFOLLOW,
             kFileCreationMode);
  if (ofd < 0) {
    (void)close(ifd);
    return;
  }

  while ((num_read = read(ifd, buffer, sizeof(buffer))) > 0) {
    ssize_t num_written = write(ofd, buffer, num_read);
    if (num_written != num_read)
      break;
  }
  (void)close(ofd);
  (void)close(ifd);
}

static void write_mark(const char *event_name)
{
  ssize_t ret __attribute__((unused));
  int fd = open(kBootstageMarkFile, O_WRONLY);

  if (fd < 0) {
    return;
  }

  /*
   * It's not necessary to check the return value,
   * but the compiler will generate a warning if we don't.
   */
  ret = write(fd, event_name, strlen(event_name));
  close(fd);
}

void bootstat_log(const char* event_name)
{
  write_mark(event_name);
  append_logdata(uptime_statistics_file_name, "uptime", event_name);
  append_logdata(disk_statistics_file_name, "disk", event_name);
}


void bootstat_set_output_directory(const char* dirname)
{
  if (dirname != NULL)
    output_directory_name = dirname;
  else
    output_directory_name = kDefaultOutputDirectoryName;
}


void bootstat_set_uptime_file_name(const char* filename)
{
  if (filename != NULL)
    uptime_statistics_file_name = filename;
  else
    uptime_statistics_file_name = kDefaultUptimeStatisticsFileName;
}


void bootstat_set_disk_file_name(const char* filename)
{
  if (filename != NULL)
    disk_statistics_file_name = filename;
  else
    disk_statistics_file_name = kDefaultDiskStatisticsFileName;
}
