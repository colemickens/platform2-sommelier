// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_MINIJAIL_MINIJAIL_H_
#define LIBCHROMEOS_CHROMEOS_MINIJAIL_MINIJAIL_H_

#include <vector>

extern "C" {
#include <linux/capability.h>
}

#include <base/lazy_instance.h>
#include <chromeos/libminijail.h>

namespace chromeos {

// A Minijail abstraction allowing Minijail mocking in tests.
class Minijail {
 public:
  virtual ~Minijail();

  // This is a singleton -- use Minijail::GetInstance()->Foo().
  static Minijail* GetInstance();

  // minijail_new
  virtual struct minijail* New();
  // minijail_destroy
  virtual void Destroy(struct minijail* jail);

  // minijail_change_user/minijail_change_group
  virtual bool DropRoot(struct minijail* jail,
                        const char* user,
                        const char* group);

  // minijail_use_seccomp_filter/minijail_no_new_privs/
  // minijail_parse_seccomp_filters
  virtual void UseSeccompFilter(struct minijail* jail, const char* path);

  // minijail_use_caps
  virtual void UseCapabilities(struct minijail* jail, uint64_t capmask);

  // minijail_enter
  virtual void Enter(struct minijail* jail);

  // minijail_run_pid
  virtual bool Run(struct minijail* jail, std::vector<char*> args, pid_t* pid);

  // minijail_run_pid and waitpid
  virtual bool RunSync(struct minijail* jail,
                       std::vector<char*> args,
                       int* status);

  // minijail_run_pid_pipe
  virtual bool RunPipe(struct minijail* jail,
                       std::vector<char*> args,
                       pid_t* pid,
                       int* stdin);

  // minijail_run_pid_pipes
  virtual bool RunPipes(struct minijail* jail,
                        std::vector<char*> args,
                        pid_t* pid,
                        int* stdin,
                        int* stdout,
                        int* stderr);

  // Run() and Destroy()
  virtual bool RunAndDestroy(struct minijail* jail,
                             std::vector<char*> args,
                             pid_t* pid);

  // RunSync() and Destroy()
  virtual bool RunSyncAndDestroy(struct minijail* jail,
                                 std::vector<char*> args,
                                 int* status);

  // RunPipe() and Destroy()
  virtual bool RunPipeAndDestroy(struct minijail* jail,
                                 std::vector<char*> args,
                                 pid_t* pid,
                                 int* stdin);

  // RunPipes() and Destroy()
  virtual bool RunPipesAndDestroy(struct minijail* jail,
                                  std::vector<char*> args,
                                  pid_t* pid,
                                  int* stdin,
                                  int* stdout,
                                  int* stderr);

 protected:
  Minijail();

 private:
  friend struct base::DefaultLazyInstanceTraits<Minijail>;

  DISALLOW_COPY_AND_ASSIGN(Minijail);
};

}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_MINIJAIL_MINIJAIL_H_
