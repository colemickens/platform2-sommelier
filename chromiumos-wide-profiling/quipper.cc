// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <vector>
#include "common.h"
#include "parser.h"
#include "profiler.h"
#include "uploader.h"

static void Daemonize();
static int ChangePriority(int priority);
static void Cleanup();
static int AtomicAcquireLock();
static void RegisterHandler();
static void Handler(int signum);
// Global to point to quipper lock directory.
const char quipper_data_dir[] = "/tmp/.quipper";

int main(int argc, char* argv[]) {
  // Open connection to syslogd
  openlog(argv[0], LOG_NOWAIT|LOG_PID, LOG_USER);
  // Acquire a lock or die.
  if(AtomicAcquireLock() == QUIPPER_FAIL) {
    return QUIPPER_FAIL;
  }
  // If -DDEBUG is not in CXXFLAGS, wait a bit before running.
  #ifndef DEBUG
    sleep(15);
  #endif
  // Launch as a daemon.
  Daemonize();
  // Try to set our priority to lowest.
  // If this fails, it's not fatal, so we won't bother checking ret.
  ChangePriority(LOWEST_PRIORITY);

  // Location of temporary perf.data
  const std::string output_location = std::string(quipper_data_dir) +
                                      "/perf.data";
  // Location of lsb-release file
  const std::string lsb_location = "/etc/lsb-release";
  // Location of perf binary, now assuming perf is in the path.
  const std::string perf_binary = "perf";
  // Server to hold perf.data.gz
  const std::string server = GAE_SERVER;
  // Profiling event TODO: support multiple events
  const std::string event = "cycles";
  // Profiling frequency in Hz (Higher # = more samples per second)
  const std::string freq = "1000";
  // Profiling duration in seconds
  const std::string time = "2";
  // Sleep time in seconds between record runs.
  // If we're in debug, redefine to a shorter period.
  #ifdef DEBUG
    const int sleep_time = 15;
  #else
    const int sleep_time = 4 * 60 * 60;
  #endif

  // Populate the parser with board and chromeos_version info
  Parser parser(lsb_location);
  parser.ParseLSB();
  // Set up parameters for profiling and uploading.
  Profiler profiler(perf_binary, event, freq, time, output_location);
  Uploader uploader(output_location, parser.board,
                    parser.chromeos_version, server);

  // Main loop. Run until one of the parts breaks.
  // These methods handle their own cleanup and
  // manage internal state if something breaks.
  int return_code = 0;
  while (true) {
    return_code = profiler.DoProfile();
    if (return_code != QUIPPER_SUCCESS) {
      break;
    }
    return_code = uploader.CompressAndUpload();
    if (return_code != QUIPPER_SUCCESS) {
      break;
    }
    sleep(sleep_time);
  }
  // Should only get here if a component fails.
  return QUIPPER_FAIL;
}

static int AtomicAcquireLock() {
  // Temporarily block signals.
  sigset_t sigs;
  sigemptyset(&sigs);
  sigfillset(&sigs);
  sigprocmask(SIG_BLOCK, &sigs, NULL);

  // Initialize our data directory.
  // If this directory exists, ret will be non-zero and mkdir will fail,
  // indicating that quipper is running.
  int ret = mkdir(quipper_data_dir, S_IRWXU);
  if (ret == 0) RegisterHandler();
  sigprocmask(SIG_UNBLOCK, &sigs, NULL);
  if (ret == 0) {
    return QUIPPER_SUCCESS;
  }
  else {
    syslog(LOG_NOTICE, "Error creating lock dir: %s\n", strerror(errno));
    return QUIPPER_FAIL;
  }
}

static int ChangePriority(int priority) {
  // Set our niceness to max, running less overhead.
  int which = PRIO_PGRP;
  int my_pid = getpid();
  int ret = setpriority(which, my_pid, priority);
  if (ret < 0) {
    syslog(LOG_NOTICE, "Could not nice process %d to %d\n", my_pid, priority);
  }
  return ret;
}

static void RegisterHandler() {
  // Establish the signal handler.
  struct sigaction sa;
  sa.sa_handler = Handler;
  sa.sa_flags = 0;
  // Don't block any signals.
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, 0);
  sigaction(SIGTERM, &sa, 0);
}

static void Handler(int signum) {
  Cleanup();
  syslog(LOG_NOTICE, "Killed by signal %d\n", signum);
  closelog();
}

static void Cleanup() {
  // Remove the quipper directory.
  system((std::string("rm -rf ") +
                      std::string(quipper_data_dir)).c_str());
}

static void Daemonize() {
  // Returns nothing, but turns us into a Daemon.
  pid_t pid, sid;
  // Clone ourselves to make a child.
  pid = fork();
  // If the pid is < 0, something went wrong.
  if (pid < 0) exit(EXIT_FAILURE);
  // If the pid we got back was > 0, then clone was successful.
  if (pid > 0) exit(EXIT_SUCCESS);
  // When execution reaches this point, we're the child.
  // Set umask to zero.
  umask(0);
  // Send a message to the syslog daemon that we've started
  syslog(LOG_INFO, "Successfully started daemon.\n");
  // Try to create our own process group.
  sid = setsid();
  if (sid < 0) {
    syslog(LOG_NOTICE, "Could not create process group, sid: %d\n", sid);
    exit(EXIT_FAILURE);
  }
  // Close standard file descriptors
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
}
