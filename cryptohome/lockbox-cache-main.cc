// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <unistd.h>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <brillo/syslog_logging.h>

#include "cryptohome/lockbox.h"
#include "cryptohome/lockbox-cache.h"
#include "cryptohome/lockbox-cache-tpm.h"
#include "cryptohome/platform.h"

using base::FilePath;

namespace switches {
// Keeps std* open for debugging
static const char *kNvramPath = "nvram";
static const char *kUnlinkNvram = "unlink-nvram";
static const char *kLockboxPath = "lockbox";
static const char *kCachePath = "cache";
}  // namespace switches

namespace {
bool CacheLockbox(cryptohome::Platform* platform,
                  const FilePath& nvram_path,
                  const FilePath& lockbox_path,
                  const FilePath& cache_path) {
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
  base::CommandLine::Init(argc, argv);

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderr);

  // Allow the commands to be configurable.
  base::CommandLine *cl = base::CommandLine::ForCurrentProcess();
  FilePath nvram_path(cl->GetSwitchValueASCII(switches::kNvramPath));
  FilePath lockbox_path(cl->GetSwitchValueASCII(switches::kLockboxPath));
  FilePath cache_path(cl->GetSwitchValueASCII(switches::kCachePath));
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
