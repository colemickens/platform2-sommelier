// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_DAEMON_H_
#define GERM_DAEMON_H_

namespace germ {

class Daemon {
 public:
  Daemon() {}
  ~Daemon() {}

  int Run();
};

}  // namespace germ

#endif  // GERM_DAEMON_H_
