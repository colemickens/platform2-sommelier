// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_PRIVET_DAEMON_STATE_H_
#define BUFFET_PRIVET_DAEMON_STATE_H_

#include <base/files/file_path.h>
#include <base/macros.h>
#include <chromeos/key_value_store.h>

namespace privetd {

namespace state_key {

extern const char kDeviceId[];
extern const char kDeviceName[];
extern const char kDeviceDescription[];
extern const char kDeviceLocation[];

extern const char kWifiHasBeenBootstrapped[];
extern const char kWifiLastConfiguredSSID[];

}  // namespace state_key

class DaemonState : public chromeos::KeyValueStore {
 public:
  explicit DaemonState(const base::FilePath& state_path);
  // Load initial state from disk.
  void Init();
  // Save state to disk.
  void Save() const;

 private:
  const base::FilePath state_path_;

  DISALLOW_COPY_AND_ASSIGN(DaemonState);
};

}  // namespace privetd

#endif  // BUFFET_PRIVET_DAEMON_STATE_H_
