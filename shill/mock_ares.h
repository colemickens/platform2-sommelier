// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_ARES_H_
#define SHILL_MOCK_ARES_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/shill_ares.h"

namespace shill {

class MockAres : public Ares {
 public:
  MockAres();
  ~MockAres() override;

  MOCK_METHOD1(Destroy, void(ares_channel channel));
  MOCK_METHOD5(GetHostByName,
               void(ares_channel channel,
                    const char* hostname,
                    int family,
                    ares_host_callback callback,
                    void* arg));
  MOCK_METHOD3(GetSock,
               int(ares_channel channel, ares_socket_t* socks, int numsocks));
  MOCK_METHOD3(InitOptions,
               int(ares_channel* channelptr,
                   struct ares_options* options,
                   int optmask));
  MOCK_METHOD3(ProcessFd,
               void(ares_channel channel,
                    ares_socket_t read_fd,
                    ares_socket_t write_fd));
  MOCK_METHOD2(SetLocalDev,
               void(ares_channel channel, const char* local_dev_name));
  MOCK_METHOD3(Timeout,
               struct timeval*(ares_channel channel,
                               struct timeval* maxtv,
                               struct timeval* tv));
  MOCK_METHOD2(SetServersCsv, int(ares_channel channel, const char* servers));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAres);
};

}  // namespace shill

#endif  // SHILL_MOCK_ARES_H_
