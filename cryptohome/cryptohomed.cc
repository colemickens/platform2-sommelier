// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/service.h"

#include <cstdlib>
#include <string>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <chaps/pkcs11/cryptoki.h>
#include <brillo/syslog_logging.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <openssl/evp.h>

#include "cryptohome/cryptohome_metrics.h"
#include "cryptohome/platform.h"

namespace env {
static const char* kAttestationBasedEnrollmentDataFile = "ABE_DATA_FILE";
}

namespace switches {
// Keeps std* open for debugging.
static const char *kNoCloseOnDaemonize = "noclose";
static const char *kNoLegacyMount = "nolegacymount";
static const char *kDirEncryption = "direncryption";
static const char *kNoDaemonize = "nodaemonize";
}  // namespace switches

static std::string ReadAbeDataFileContents(cryptohome::Platform* platform) {
  std::string data;

  const char* abe_data_file =
      std::getenv(env::kAttestationBasedEnrollmentDataFile);
  if (!abe_data_file)
    return data;

  base::FilePath file_path(abe_data_file);
  if (!platform->ReadFileToString(file_path, &data))
    LOG(FATAL) << "Could not read attestation-based enterprise enrollment data"
                  " in: " << file_path.value();
  return data;
}

int main(int argc, char **argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderr);

  // Read the file before we daemonize so it can be deleted as soon as we exit.
  cryptohome::Platform platform;
  std::string abe_data = ReadAbeDataFileContents(&platform);

  // Allow the commands to be configurable.
  base::CommandLine *cl = base::CommandLine::ForCurrentProcess();
  int noclose = cl->HasSwitch(switches::kNoCloseOnDaemonize);
  bool nolegacymount = cl->HasSwitch(switches::kNoLegacyMount);
  bool direncryption = cl->HasSwitch(switches::kDirEncryption);
  bool daemonize = !cl->HasSwitch(switches::kNoDaemonize);
  if (daemonize) {
    PLOG_IF(FATAL, daemon(0, noclose) == -1) << "Failed to daemonize";
  }

  // Setup threading. This needs to be called before other calls into glib and
  // before multiple threads are created that access dbus.
  dbus_threads_init_default();

  // Initialize OpenSSL.
  OpenSSL_add_all_algorithms();

  cryptohome::ScopedMetricsInitializer metrics_initializer;

  cryptohome::Service* service = cryptohome::Service::CreateDefault(abe_data);

  service->set_legacy_mount(!nolegacymount);
  service->set_force_ecryptfs(!direncryption);

  if (!service->Initialize()) {
    LOG(FATAL) << "Service initialization failed";
    return 1;
  }

  if (!service->Register(brillo::dbus::GetSystemBusConnection())) {
    LOG(FATAL) << "DBUS service registration failed";
    return 1;
  }

  if (!service->Run()) {
    LOG(FATAL) << "Service run failed.";
    return 1;
  }

  // If PKCS #11 was initialized, this will tear it down.
  C_Finalize(NULL);

  return 0;
}
