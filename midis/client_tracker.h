// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_CLIENT_TRACKER_H_
#define MIDIS_CLIENT_TRACKER_H_

#include <map>
#include <memory>

#include <base/files/scoped_file.h>
#include <base/memory/weak_ptr.h>

#include "midis/client.h"

namespace midis {

class ClientTracker {
 public:
  ClientTracker();
  bool InitClientTracker();
  void ProcessClient(int fd);

 private:
  std::map<uint32_t, std::unique_ptr<Client>> clients_;
  base::ScopedFD server_fd_;
  int client_id_counter_;

  base::WeakPtrFactory<ClientTracker> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(ClientTracker);
};

}  // namespace midis

#endif  // MIDIS_CLIENT_TRACKER_H_
