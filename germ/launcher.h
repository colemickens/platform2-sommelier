// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_LAUNCHER_H_
#define GERM_LAUNCHER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <base/macros.h>
#include <chromeos/process.h>

namespace germ {

class UidService;

class Launcher {
 public:
  Launcher();
  virtual ~Launcher();

  bool RunInteractive(const std::string& name,
                      const std::vector<std::string>& argv,
                      int* status);
  bool RunDaemonized(const std::string& name,
                     const std::vector<std::string>& argv,
                     pid_t* pid);
  bool Terminate(pid_t pid);

 protected:
  pid_t GetPidFromOutput(const std::string& output);

  // Can be overridden in unit tests.
  virtual std::string ReadFromStdout(chromeos::Process* process);
  virtual std::unique_ptr<chromeos::Process> GetProcessInstance();

 private:
  std::unique_ptr<UidService> uid_service_;
  std::unordered_map<pid_t, std::string> names_;

  DISALLOW_COPY_AND_ASSIGN(Launcher);
};

}  // namespace germ

#endif  // GERM_LAUNCHER_H_
