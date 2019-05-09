// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_UTIL_H_
#define CRASH_REPORTER_UTIL_H_

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <brillo/process.h>
#include <brillo/streams/stream.h>
#include <session_manager/dbus-proxies.h>

namespace util {

// Returns true if integration tests are currently running.
bool IsCrashTestInProgress();

// Returns true if uploading of device coredumps is allowed.
bool IsDeviceCoredumpUploadAllowed();

// Returns true if running on a developer image.
bool IsDeveloperImage();

// Returns true if running on a test image.
bool IsTestImage();

// Returns true if running on an official image.
bool IsOfficialImage();

// Gets a string describing the hardware class of the device. Returns
// "undefined" if this cannot be determined.
std::string GetHardwareClass();

// Returns the boot mode which will either be "dev", "missing-crossystem" (if it
// cannot be determined) or the empty string.
std::string GetBootModeString();

// Tries to find |key| in a key-value file named |base_name| in |directories| in
// the specified order, and writes the value to |value|. This function returns
// as soon as the key is found (i.e. if the key is found in the first directory,
// the remaining directories won't be checked). Returns true on success.
bool GetCachedKeyValue(const base::FilePath& base_name,
                       const std::string& key,
                       const std::vector<base::FilePath>& directories,
                       std::string* value);

// Similar to GetCachedKeyValue(), but this version checks the predefined
// default directories.
bool GetCachedKeyValueDefault(const base::FilePath& base_name,
                              const std::string& key,
                              std::string* value);

// Gets the user crash directories via D-Bus using |session_manager_proxy|.
// Returns true on success. The original contents of |directories| will be lost.
bool GetUserCrashDirectories(
    org::chromium::SessionManagerInterfaceProxyInterface* session_manager_proxy,
    std::vector<base::FilePath>* directories);

// Gzip-compresses |path|, removes the original file, and returns the path of
// the new file. On failure, the original file is left alone and an empty path
// is returned.
base::FilePath GzipFile(const base::FilePath& path);

// Gzip's the |data| passed in and returns the compressed data. Returns an empty
// string on failure.
std::string GzipStream(brillo::StreamPtr data);

// Runs |process| and redirects |fd| to |output|. Returns the exit code, or -1
// if the process failed to start.
int RunAndCaptureOutput(brillo::ProcessImpl* process,
                        int fd,
                        std::string* output);

// Breaks up |error| using std::getline and then does a LOG(ERROR) of each
// individual line.
void LogMultilineError(const std::string& error);

}  // namespace util

#endif  // CRASH_REPORTER_UTIL_H_
