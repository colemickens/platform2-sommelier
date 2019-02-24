// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEV_INSTALL_DEV_INSTALL_H_
#define DEV_INSTALL_DEV_INSTALL_H_

#include <sys/stat.h>

#include <istream>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/macros.h>

namespace dev_install {

class DevInstall {
 public:
  DevInstall();
  explicit DevInstall(const std::string& binhost,
                      const std::string& binhost_version,
                      bool reinstall,
                      bool uninstall,
                      bool yes,
                      bool only_bootstrap);

  // Run the dev_install routine.
  int Run();

  // Exec the helper script.  Mostly for mocking.
  virtual int Exec(const std::vector<const char*>& argv);

  // Whether the system is currently in dev mode.
  virtual bool IsDevMode() const;

  // Prompts the user.
  virtual bool PromptUser(std::istream& input, const std::string& prompt);

  // Delete a path recursively on the same mount point.
  virtual bool DeletePath(const struct stat& base_stat,
                          const base::FilePath& dir);

  // Unittest helpers.
  void SetYesForTest(bool yes) { yes_ = yes; }
  void SetStateDirForTest(const base::FilePath& dir) { state_dir_ = dir; }

 private:
  bool reinstall_;
  bool uninstall_;
  bool yes_;
  bool only_bootstrap_;
  base::FilePath state_dir_;
  std::string binhost_;
  std::string binhost_version_;

  DISALLOW_COPY_AND_ASSIGN(DevInstall);
};

}  // namespace dev_install

#endif  // DEV_INSTALL_DEV_INSTALL_H_
