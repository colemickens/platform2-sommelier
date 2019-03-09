// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/test_util.h"

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <gtest/gtest.h>

using testing::Invoke;
using testing::_;

namespace test_util {

namespace {

std::map<std::string, std::string>* g_active_sessions;

// Implementation of
// SessionManagerInterfaceProxyMock::RetrieveActiveSessions().
bool RetrieveActiveSessionsImpl(
    std::map<std::string, std::string>* out_sessions,
    brillo::ErrorPtr* error,
    int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) {
  DCHECK(g_active_sessions);  // Set in SetActiveSessions().
  *out_sessions = *g_active_sessions;
  return true;
}

}  // namespace

bool CreateFile(const base::FilePath& file_path, base::StringPiece content) {
  if (!base::CreateDirectory(file_path.DirName()))
    return false;
  return base::WriteFile(file_path, content.data(), content.size()) ==
         content.size();
}

void SetActiveSessions(org::chromium::SessionManagerInterfaceProxyMock* mock,
                       const std::map<std::string, std::string>& sessions) {
  if (g_active_sessions)
    delete g_active_sessions;
  g_active_sessions = new std::map<std::string, std::string>(sessions);

  EXPECT_CALL(*mock, RetrieveActiveSessions(_, _, _))
      .WillRepeatedly(Invoke(&RetrieveActiveSessionsImpl));
}

bool DirectoryHasFileWithPattern(const base::FilePath& directory,
                                 const std::string& pattern,
                                 base::FilePath* found_file_path) {
  base::FileEnumerator enumerator(
      directory, false, base::FileEnumerator::FileType::FILES, pattern);
  base::FilePath path = enumerator.Next();
  if (!path.empty() && found_file_path)
    *found_file_path = path;
  return !path.empty();
}

}  // namespace test_util
