// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INIT_CLOBBER_STATE_H_
#define INIT_CLOBBER_STATE_H_

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

  static Arguments ParseArgv(int argc, char const* const argv[]);

  explicit ClobberState(const Arguments& args);
  int Run();

 private:
  int RunClobberStateShell();
  int Reboot();
  Arguments args_;
};

#endif  // INIT_CLOBBER_STATE_H_
