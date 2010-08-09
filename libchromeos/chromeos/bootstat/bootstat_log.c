// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stddef.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "bootstat.h"

//
// Paths to the statistics files we snapshot as part of the data to
// be logged.
//
#define UPTIME    "/proc/uptime"

#if defined (__amd64__) || defined (__x86_64__)
#define ROOTDEV   "sda"
#endif

#if defined (__arm__)
#define ROOTDEV   "mmcblk0"
#endif

#ifdef ROOTDEV
#define ROOTSTAT  "/sys/block/" ROOTDEV "/stat"
#endif

//
// Length of the longest valid string naming an event, not counting
// the terminating NUL character.  Arbitrarily chosen, but intended
// to be somewhat shorter than any valid file or path name.
//
#define MAX_EVENT_LEN  63

//
// Maximum length of any pathname for storing event statistics.
// Also arbitrarily chosen, but see the comment below about
// truncation.
//
#define MAX_STAT_PATH  128

//
// Output file creation mode:  0666, a.k.a. rw-rw-rw-.
//
#define OFMODE  (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)

static void append_logdata(const char *ifname,
                           const char *ofprefix, const char *event_id)
{
  char      ofpath[MAX_STAT_PATH];
  char      buffer[256];
  ssize_t   nbytes;
  int       ifd, ofd;

  ifd = open(ifname, O_RDONLY);
  if (ifd < 0) {
    return;
  }

  //
  // We don't want the file name "/tmp/uptime-..." truncated
  // differently from the the name "/tmp/disk-...", so we truncate
  // event_id separately using the "%.*s" format.
  //
  // We expect that MAX_STAT_LEN is enough smaller than
  // MAX_STAT_PATH that ofpath will never be truncated.  However,
  // to be safe, we also stuff our own terminating '\0', since
  // snprintf() won't put it there in the case of truncation.
  //
  (void) snprintf(ofpath, sizeof (ofpath) - 1, "/tmp/%s-%.*s",
                  ofprefix, MAX_EVENT_LEN, event_id);
  ofpath[sizeof (ofpath) - 1] = '\0';
  ofd = open(ofpath, O_WRONLY | O_APPEND | O_CREAT, OFMODE);
  if (ofd < 0) {
    (void) close(ifd);
    return;
  }

  while ((nbytes = read(ifd, buffer, sizeof (buffer))) > 0) {
    ssize_t wnbytes = write(ofd, buffer, nbytes);
    if (wnbytes != nbytes)
      break;
  }
  (void) close(ofd);
  (void) close(ifd);
}


void bootstat_log(const char *event_id)
{
  append_logdata(UPTIME, "uptime", event_id);
#ifdef ROOTSTAT
  append_logdata(ROOTSTAT, "disk", event_id);
#endif
}
