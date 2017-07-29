// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_MAITRED_INIT_H_
#define VM_TOOLS_MAITRED_INIT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>
#include <base/threading/thread.h>

namespace vm_tools {
namespace maitred {

// Encapsulates all the functionality for which maitred is responsible when it
// runs as pid 1 on a VM.
class Init final {
 public:
  // Creates a new instance of this class and performs various bits of early
  // setup up like mounting file systems, creating directories, and setting
  // up signal handlers.
  static std::unique_ptr<Init> Create();
  ~Init();

  // Spawn a process with the given argv and environment.  |argv[0]| must be
  // the full path to the program or the name of a program found in PATH.
  bool Spawn(std::vector<std::string> argv,
             std::map<std::string, std::string> env,
             bool respawn,
             bool use_console);

 private:
  Init() = default;
  bool Setup();

  // Worker that lives on a separate thread and is responsible for actually
  // doing all the work.
  class Worker;
  std::unique_ptr<Worker> worker_;

  // The actual worker thread.
  base::Thread worker_thread_{"init worker thread"};

  DISALLOW_COPY_AND_ASSIGN(Init);
};

}  //  namespace maitred
}  // namespace vm_tools

#endif  // VM_TOOLS_MAITRED_INIT_H_
