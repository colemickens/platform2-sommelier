// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MINIJAIL_H_
#define SHILL_MOCK_MINIJAIL_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/minijail.h"

namespace shill {

class MockMinijail : public Minijail {
 public:
  MockMinijail();
  virtual ~MockMinijail();

  MOCK_METHOD0(New, struct minijail *());
  MOCK_METHOD1(Destroy, void (struct minijail *));

  MOCK_METHOD2(DropRoot, bool(struct minijail *jail, const char *user));
  MOCK_METHOD2(UseCapabilities, void(struct minijail *jail, uint64_t capmask));
  MOCK_METHOD3(Run, bool(struct minijail *jail,
                         std::vector<char *> args, pid_t *pid));
  MOCK_METHOD3(RunAndDestroy, bool(struct minijail *jail,
                                   std::vector<char *> args, pid_t *pid));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMinijail);
};

}  // namespace shill

#endif  // SHILL_MOCK_MINIJAIL_H_
