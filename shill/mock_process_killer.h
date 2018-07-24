// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_PROCESS_KILLER_H_
#define SHILL_MOCK_PROCESS_KILLER_H_

#include <gmock/gmock.h>

#include "shill/process_killer.h"

namespace shill {

class MockProcessKiller : public ProcessKiller {
 public:
  MockProcessKiller();
  ~MockProcessKiller() override;

  MOCK_METHOD2(Wait, bool(int pid, const base::Closure& callback));
  MOCK_METHOD2(Kill, void(int pid, const base::Closure& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProcessKiller);
};

}  // namespace shill

#endif  // SHILL_MOCK_PROCESS_KILLER_H_
