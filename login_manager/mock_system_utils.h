// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_
#define LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_

#include <stdint.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "login_manager/system_utils.h"

namespace login_manager {

class MockSystemUtils : public SystemUtils {
 public:
  MockSystemUtils();
  ~MockSystemUtils() override;

  MOCK_METHOD3(kill, int(pid_t pid, uid_t uid, int signal));
  MOCK_METHOD1(time, time_t(time_t*));  // NOLINT
  MOCK_METHOD0(fork, pid_t(void));
  MOCK_METHOD1(close, int(int fd));
  MOCK_METHOD1(chdir, int(const base::FilePath& path));
  MOCK_METHOD0(setsid, pid_t(void));
  MOCK_METHOD3(execve,
               int(const base::FilePath& exec_file,
                   const char* const argv[],
                   const char* const envp[]));
  MOCK_METHOD0(EnterNewMountNamespace, bool(void));
  MOCK_METHOD2(GetAppOutput,
               bool(const std::vector<std::string>&, std::string*));
  MOCK_METHOD0(GetDevModeState, DevModeState(void));
  MOCK_METHOD0(GetVmState, VmState(void));
  MOCK_METHOD2(ProcessGroupIsGone,
               bool(pid_t child_spec, base::TimeDelta timeout));
  MOCK_METHOD2(ProcessIsGone, bool(pid_t child_spec, base::TimeDelta timeout));
  MOCK_METHOD3(Wait, pid_t(pid_t, base::TimeDelta, int*));
  MOCK_METHOD2(EnsureAndReturnSafeFileSize,
               bool(const base::FilePath& file, int32_t* file_size_32));
  MOCK_METHOD1(Exists, bool(const base::FilePath& file));
  MOCK_METHOD1(DirectoryExists, bool(const base::FilePath& dir));
  MOCK_METHOD1(CreateDir, bool(const base::FilePath& dir));
  MOCK_METHOD3(EnumerateFiles,
               bool(const base::FilePath&, int, std::vector<base::FilePath>*));
  MOCK_METHOD1(GetUniqueFilenameInWriteOnlyTempDir,
               bool(base::FilePath* temp_file_path));
  MOCK_METHOD1(RemoveFile, bool(const base::FilePath& filename));
  MOCK_METHOD2(AtomicFileWrite,
               bool(const base::FilePath& filename, const std::string& data));
  MOCK_METHOD1(AmountOfFreeDiskSpace, int64_t(const base::FilePath& path));
  MOCK_METHOD2(GetGroupInfo,
               bool(const std::string& group_name, gid_t* out_gid));
  MOCK_METHOD3(GetGidAndGroups,
               bool(uid_t uid, gid_t* out_gid, std::vector<gid_t>*));
  MOCK_METHOD3(SetIDs,
               int(uid_t uid, gid_t gid, const std::vector<gid_t>& gids));
  MOCK_METHOD3(ChangeOwner,
               bool(const base::FilePath& filename, pid_t pid, gid_t gid));
  MOCK_METHOD2(SetPosixFilePermissions,
               bool(const base::FilePath& filename, mode_t mode));
  MOCK_METHOD1(CreateServerHandle,
               ScopedPlatformHandle(const NamedPlatformHandle& named_handle));

  MOCK_METHOD2(ReadFileToString, bool(const base::FilePath&, std::string*));
  MOCK_METHOD2(WriteStringToFile,
               bool(const base::FilePath&, const std::string&));
  MOCK_METHOD1(CloseSuperfluousFds,
               void(const base::InjectiveMultimap& saved_mapping));

  MOCK_METHOD2(ChangeBlockedSignals,
               bool(int how, const std::vector<int>& signals));

  MOCK_METHOD2(LaunchAndWait, bool(const std::vector<std::string>&, int*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSystemUtils);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_
