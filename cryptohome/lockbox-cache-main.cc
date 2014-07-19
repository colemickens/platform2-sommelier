// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <unistd.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <chromeos/syslog_logging.h>

#include "cryptohome/lockbox.h"
#include "cryptohome/platform.h"
#include "cryptohome/lockbox-cache-tpm.h"
#include "cryptohome/lockbox-cache.h"

namespace switches {
// Keeps std* open for debugging
static const char *kNvramPath = "nvram";
static const char *kUnlinkNvram = "unlink-nvram";
static const char *kLockboxPath = "lockbox";
static const char *kCachePath = "cache";
}  // namespace switches

namespace {
bool CacheLockbox(cryptohome::Platform* platform,
                  const std::string& nvram_path,
                  const std::string& lockbox_path,
                  const std::string& cache_path) {
  static const uint32_t kBogusNvramIndex = 0;
  cryptohome::LockboxCacheTpm cache_tpm(kBogusNvramIndex, nvram_path);
  cache_tpm.Init(platform, false);
  cryptohome::LockboxCache cache;
  return cache.Initialize(platform, &cache_tpm) &&
         cache.LoadAndVerify(kBogusNvramIndex, lockbox_path) &&
         cache.Write(cache_path);
}
}  // anonymous namespace


int main(int argc, char **argv) {
  CommandLine::Init(argc, argv);

  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);

  // Allow the commands to be configurable.
  CommandLine *cl = CommandLine::ForCurrentProcess();
  std::string nvram_path = cl->GetSwitchValueASCII(switches::kNvramPath);
  std::string lockbox_path = cl->GetSwitchValueASCII(switches::kLockboxPath);
  std::string cache_path = cl->GetSwitchValueASCII(switches::kCachePath);
  if (nvram_path.empty() || lockbox_path.empty() || cache_path.empty()) {
    LOG(ERROR) << "Paths for --cache, --lockbox, and --nvram must be supplied.";
    return 1;
  }

  cryptohome::Platform platform;
  bool ok = CacheLockbox(&platform, nvram_path, lockbox_path, cache_path);
  if (cl->HasSwitch(switches::kUnlinkNvram))
    platform.DeleteFile(nvram_path, false);
  if (!ok)
    platform.DeleteFile(cache_path, false);
  return !ok;
}
