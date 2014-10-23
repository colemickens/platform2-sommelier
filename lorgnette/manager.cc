// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lorgnette/manager.h"

#include <signal.h>
#include <stdint.h>
#include <unistd.h>

#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/stl_util.h>
#include <base/strings/string_split.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/variant_dictionary.h>
#include <chromeos/process.h>

#include "lorgnette/daemon.h"

using base::ScopedFD;
using base::StringPrintf;
using std::map;
using std::string;
using std::vector;

namespace lorgnette {

// static
const char Manager::kScanConverterPath[] = "/usr/bin/pnm2png";
const char Manager::kScanImageFormattedDeviceListCmd[] =
    "--formatted-device-list=%d%%%v%%%m%%%t%n";
const char Manager::kScanImagePath[] = "/usr/bin/scanimage";
const int Manager::kTimeoutAfterKillSeconds = 1;

Manager::Manager(base::Callback<void()> activity_callback)
    : activity_callback_(activity_callback) {}

Manager::~Manager() {}

void Manager::InitDBus(
    chromeos::dbus_utils::ExportedObjectManager *object_manager) {
  dbus_adaptor_.reset(
     new org::chromium::lorgnette::ManagerAdaptor(
         object_manager, object_manager->GetBus(), kManagerServicePath, this));
}

Manager::ScannerInfo Manager::ListScanners(chromeos::ErrorPtr *error) {
  base::FilePath output_path;
  FILE *output_file_handle;
  output_file_handle = base::CreateAndOpenTemporaryFile(&output_path);
  if (!output_file_handle) {
    SetError(__func__, "Unable to create temporary file.", error);
    return ScannerInfo();
  }

  chromeos::ProcessImpl process;
  RunListScannersProcess(fileno(output_file_handle), &process);
  fclose(output_file_handle);
  string scanner_output_string;
  bool read_status = base::ReadFileToString(output_path,
                                            &scanner_output_string);
  const bool recursive_delete = false;
  base::DeleteFile(output_path, recursive_delete);
  if (!read_status) {
    SetError(__func__, "Unable to read scanner list output file", error);
    return ScannerInfo();
  }
  activity_callback_.Run();
  return ScannerInfoFromString(scanner_output_string);
}

void Manager::ScanImage(
    chromeos::ErrorPtr *error,
    const string &device_name,
    const dbus::FileDescriptor &outfd,
    const chromeos::VariantDictionary &scan_properties) {
  int pipe_fds[2];
  if (pipe(pipe_fds) != 0) {
    SetError(__func__, "Unable to create process pipe", error);
    return;
  }

  ScopedFD pipe_fd_input(pipe_fds[0]);
  ScopedFD pipe_fd_output(pipe_fds[1]);
  chromeos::ProcessImpl scan_process;
  chromeos::ProcessImpl convert_process;

  // Since the FileDescriptor object retains ownership of this file descriptor,
  // make a local copy.
  int out_fd = dup(outfd.value());
  RunScanImageProcess(device_name,
                      out_fd,
                      &pipe_fd_input,
                      &pipe_fd_output,
                      scan_properties,
                      &scan_process,
                      &convert_process,
                      error);
  activity_callback_.Run();
}

// static
void Manager::RunListScannersProcess(int fd, chromeos::Process *process) {
  process->AddArg(kScanImagePath);
  process->AddArg(kScanImageFormattedDeviceListCmd);
  process->BindFd(fd, STDOUT_FILENO);
  process->Run();
}

// static
void Manager::RunScanImageProcess(
    const string &device_name,
    int out_fd,
    ScopedFD *pipe_fd_input,
    ScopedFD *pipe_fd_output,
    const chromeos::VariantDictionary &scan_properties,
    chromeos::Process *scan_process,
    chromeos::Process *convert_process,
    chromeos::ErrorPtr *error) {
  scan_process->AddArg(kScanImagePath);
  scan_process->AddArg("-d");
  scan_process->AddArg(device_name);

  for (const auto &property : scan_properties) {
    const string &property_name = property.first;
    const auto &property_value = property.second;
    if (property_name == kScanPropertyMode &&
        property_value.IsTypeCompatible<string>()) {
      string mode = property_value.Get<string>();
      if (mode != kScanPropertyModeColor &&
          mode != kScanPropertyModeGray &&
          mode != kScanPropertyModeLineart) {
        SetError(__func__,
                 StringPrintf("Invalid mode parameter %s", mode.c_str()),
                 error);
        return;
      }
      scan_process->AddArg("--mode");
      scan_process->AddArg(mode);
    } else if (property_name == kScanPropertyResolution &&
               property_value.IsTypeCompatible<uint32_t>()) {
      scan_process->AddArg("--resolution");
      scan_process->AddArg(base::UintToString(
          property_value.Get<unsigned int>()));
    } else {
      SetError(__func__,
               StringPrintf("Invalid scan parameter %s of type %s",
                            property_name.c_str(),
                            property_value.GetType().name()),
               error);
      return;
    }
  }
  scan_process->BindFd(pipe_fd_output->release(), STDOUT_FILENO);

  convert_process->AddArg(kScanConverterPath);
  convert_process->BindFd(pipe_fd_input->release(), STDIN_FILENO);
  convert_process->BindFd(out_fd, STDOUT_FILENO);

  convert_process->Start();
  scan_process->Start();

  int scan_result = scan_process->Wait();
  if (scan_result != 0) {
    SetError(__func__,
             StringPrintf("Scan process exited with result %d", scan_result),
             error);
    // Explicitly kill and reap the converter since we may fail to successfully
    // reap the processes as it exits this scope.
    convert_process->Kill(SIGKILL, kTimeoutAfterKillSeconds);
    return;
  }

  int converter_result = convert_process->Wait();
  if (converter_result != 0) {
    SetError(__func__,
             StringPrintf("Image converter process failed with result %d",
                          converter_result),
             error);
    return;
  }

  LOG(INFO) << __func__ << ": completed image scan and conversion.";
}

// static
Manager::ScannerInfo Manager::ScannerInfoFromString(
    const string &scanner_info_string) {
  vector<string> scanner_output_lines;
  base::SplitString(scanner_info_string, '\n', &scanner_output_lines);

  ScannerInfo scanners;
  for (const auto &line : scanner_output_lines) {
    vector<string> scanner_info_parts;
    base::SplitString(line, '%', &scanner_info_parts);
    if (scanner_info_parts.size() != 4) {
      continue;
    }
    map<string, string> scanner_info;
    scanner_info[kScannerPropertyManufacturer] = scanner_info_parts[1];
    scanner_info[kScannerPropertyModel] = scanner_info_parts[2];
    scanner_info[kScannerPropertyType] = scanner_info_parts[3];
    scanners[scanner_info_parts[0]] = scanner_info;
  }

  return scanners;
}

// static
void Manager::SetError(const string &method,
                       const string &message,
                       chromeos::ErrorPtr *error) {
  chromeos::Error::AddTo(
      error, chromeos::errors::dbus::kDomain, kManagerServiceError, message);
  LOG(ERROR) << method << ": " << message;
}

}  // namespace lorgnette
