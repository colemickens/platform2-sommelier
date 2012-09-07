// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace cryptohome {

class Platform;

// This class handles recovery of encrypted data from the stateful partition.
// At present, it provides a simple way to export the encrypted data while the
// feature is maturing by checking for the existence of a file on the
// unencrypted portion of stateful.
//
// Once the feature has seen satisfactory airtime and all
// related tooling is robust, this class will implement a tighter mechanism
// for recovering the encrypted data in stateful that requires physical device
// modification or device owner modification:
//   http://crosbug.com/34219
//
class StatefulRecovery {
 public:
  explicit StatefulRecovery(Platform* platform);
  ~StatefulRecovery();

  // Returns true if recovery was requested by the device user.
  virtual bool Requested();
  // Returns true if it successfully recovered stateful contents.
  virtual bool Recover();
  // On Chrome hardware, sets the recovery request field and reboots.
  virtual void PerformReboot();

  static const char *kRecoverSource;
  static const char *kRecoverDestination;
  static const char *kFlagFile;
 private:
  // Returns true if a flag file indicating a recovery request exists and
  // contains the expected content.
  bool ParseFlagFile();

  bool requested_;
  Platform* platform_;
};

}  // namespace cryptohome
