// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INIT_CLOBBER_STATE_H_
#define INIT_CLOBBER_STATE_H_

#include <string>
#include <vector>

#include <base/files/file_path.h>

#include "init/crossystem.h"

class ClobberState {
 public:
  struct Arguments {
    // Run in the context of a factory flow, do not reboot when done.
    bool factory_wipe = false;
    // Less thorough data destruction.
    bool fast_wipe = false;
    // Don't delete the non-active set of kernel/root partitions.
    bool keepimg = false;
    // Preserve some files and VPD keys.
    bool safe_wipe = false;
    // Preserve rollback data, attestation DB, and don't clear TPM
    bool rollback_wipe = false;
  };

  // Extracts ClobberState's arguments from argv.
  static Arguments ParseArgv(int argc, char const* const argv[]);

  // Attempts to increment the contents of |path| by 1. If the contents cannot
  // be read, or if the contents are not an integer, writes '1' to the file.
  static bool IncrementFileCounter(const base::FilePath& path);

  // Given a list of files to preserve (relative to |preserved_files_root|),
  // creates a tar file containing those files at |tar_file_path|.
  // The directory structure of the preserved files is preserved.
  static int PreserveFiles(const base::FilePath& preserved_files_root,
                           const std::vector<base::FilePath>& preserved_files,
                           const base::FilePath& tar_file_path);

  // Ownership of |cros_system| is retained by the caller.
  ClobberState(const Arguments& args, CrosSystem* cros_system);
  int Run();
  bool MarkDeveloperMode();

  // Returns vector of files to be preserved. All FilePaths are relative to
  // stateful_.
  std::vector<base::FilePath> GetPreservedFilesList();

  void SetArgsForTest(const Arguments& args);
  void SetStatefulForTest(base::FilePath stateful_path);

 private:
  bool ClearBiometricSensorEntropy();
  int RunClobberStateShell();
  int Reboot();

  Arguments args_;
  CrosSystem* cros_system_;
  base::FilePath stateful_;
};

#endif  // INIT_CLOBBER_STATE_H_
