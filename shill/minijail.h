// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MINIJAIL_H_
#define SHILL_MINIJAIL_H_

#include <vector>

extern "C" {
#include <linux/capability.h>
}

#include <base/lazy_instance.h>
#include <chromeos/libminijail.h>

namespace shill {

// A Minijail abstraction allowing Minijail mocking in tests.
class Minijail {
 public:
  virtual ~Minijail();

  // This is a singleton -- use Minijail::GetInstance()->Foo()
  static Minijail *GetInstance();

  // minijail_new
  virtual struct minijail *New();
  // minijail_destroy
  virtual void Destroy(struct minijail *jail);

  // minijail_change_user/minijail_change_group
  virtual bool DropRoot(struct minijail *jail, const char *user);
  // minijail_use_caps
  virtual void UseCapabilities(struct minijail *jail, uint64_t capmask);

  // minijail_run_pid
  virtual bool Run(struct minijail *jail, std::vector<char *> args, pid_t *pid);

  // minijail_run_pid_pipe
  virtual bool RunPipe(struct minijail *jail, std::vector<char *> args,
                       pid_t *pid, int *stdin);

  // Run() and Destroy()
  virtual bool RunAndDestroy(struct minijail *jail,
                             std::vector<char *> args,
                             pid_t *pid);

  // RunPipe() and Destroy()
  virtual bool RunPipeAndDestroy(struct minijail *jail,
                                 std::vector<char *> args,
                                 pid_t *pid, int *stdin);

 protected:
  Minijail();

 private:
  friend struct base::DefaultLazyInstanceTraits<Minijail>;

  DISALLOW_COPY_AND_ASSIGN(Minijail);
};

}  // namespace shill

#endif  // SHILL_MINIJAIL_H_
