// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lorgnette/manager.h"

#include <signal.h>

#include <vector>

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/stl_util.h>
#include <base/string_split.h>
#include <base/string_number_conversions.h>
#include <base/stringprintf.h>
#include <chromeos/process.h>

#include "lorgnette/daemon.h"
#include "lorgnette/minijail.h"

using std::map;
using std::string;
using std::vector;

namespace lorgnette {

// static
const char Manager::kDBusErrorName[] = "org.chromium.lorgnette.Error";
const char Manager::kObjectPath[] = "/org/chromium/lorgnette/Manager";
const char Manager::kScanConverterPath[] = "/usr/bin/pnm2png";
const char Manager::kScanImageFormattedDeviceListCmd[] =
    "--formatted-device-list=%d%%%v%%%m%%%t%n";
const char Manager::kScanImagePath[] = "/usr/bin/scanimage";
const char Manager::kScannerPropertyManufacturer[] = "Manufacturer";
const char Manager::kScannerPropertyModel[] = "Model";
const char Manager::kScannerPropertyType[] = "Type";
const char Manager::kScanPropertyMode[] = "Mode";
const char Manager::kScanPropertyModeColor[] = "Color";
const char Manager::kScanPropertyModeGray[] = "Gray";
const char Manager::kScanPropertyModeLineart[] = "Lineart";
const char Manager::kScanPropertyResolution[] = "Resolution";
const int Manager::kTimeoutAfterKillSeconds = 1;

Manager::Manager(DBus::Connection *conn)
    : DBus::ObjectAdaptor(*conn, kObjectPath),
      minijail_(Minijail::GetInstance()) {}

Manager::~Manager() {}

map<string, map<string, string>> Manager::ListScanners(::DBus::Error &error) {
  map<string, map<string, string>> scanners;

  base::FilePath output_path;
  FILE *output_file_handle;
  output_file_handle = file_util::CreateAndOpenTemporaryFile(&output_path);
  if (!output_file_handle) {
    SetError(__func__, "Unable to create temporary file.", &error);
    return scanners;
  }

  chromeos::ProcessImpl process;
  process.AddArg(kScanImagePath);
  process.AddArg(kScanImageFormattedDeviceListCmd);
  process.BindFd(fileno(output_file_handle), STDOUT_FILENO);
  process.Run();
  fclose(output_file_handle);
  const bool recursive_delete = false;
  string scanner_output_string;
  bool read_status = file_util::ReadFileToString(output_path,
                                                 &scanner_output_string);
  file_util::Delete(output_path, recursive_delete);
  if (!read_status) {
    SetError(__func__, "Unable to read scanner list output file", &error);
    return scanners;
  }
  vector<string> scanner_output_lines;
  base::SplitString(scanner_output_string, '\n', &scanner_output_lines);

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

void Manager::ScanImage(
    const string &device_name,
    const ::DBus::FileDescriptor &outfd,
    const map<string, ::DBus::Variant> &scan_properties,
    ::DBus::Error &error) {
  int pipe_fds[2];
  if (pipe(pipe_fds) != 0) {
    SetError(__func__, "Unable to create process pipe", &error);
    return;
  }

  chromeos::ProcessImpl scan_process;
  scan_process.AddArg(kScanImagePath);
  scan_process.AddArg("-d");
  scan_process.AddArg(device_name);

  for (const auto &property : scan_properties) {
    const string &property_name = property.first;
    const ::DBus::Variant &property_value = property.second;
    if (property_name == kScanPropertyMode &&
        property_value.signature() == ::DBus::type<string>::sig()) {
      string mode = property_value.reader().get_string();
      if (mode != kScanPropertyModeColor &&
          mode != kScanPropertyModeGray &&
          mode != kScanPropertyModeLineart) {
        SetError(__func__,
                 base::StringPrintf("Invalid mode parameter %s", mode.c_str()),
                 error);
        return;
      }
      scan_process.AddArg("--mode");
      scan_process.AddArg(property_value.reader().get_string());
    } else if (property_name == kScanPropertyResolution &&
               property_value.signature() == ::DBus::type<uint32>::sig()) {
      scan_process.AddArg("--resolution");
      scan_process.AddArg(base::UintToString(
          property_value.reader().get_uint32()));
    } else {
      SetError(__func__,
               base::StringPrintf("Invalid scan parameter %s with signature %s",
                                  property_name.c_str(),
                                  property_value.signature().c_str()),
               &error);
      return;
    }
  }
  scan_process.BindFd(pipe_fds[1], STDOUT_FILENO);

  chromeos::ProcessImpl converter_process;
  converter_process.AddArg(kScanConverterPath);
  converter_process.BindFd(pipe_fds[0], STDIN_FILENO);
  converter_process.BindFd(outfd.get(), STDOUT_FILENO);

  converter_process.Start();
  scan_process.Start();

  int scan_result = scan_process.Wait();
  if (scan_result != 0) {
    SetError(__func__,
             StringPrintf("Scan process exited with result %d", scan_result),
             &error);
    // Explicitly kill and reap the converter since we may fail to successfully
    // reap the processes as it exits this scope.
    converter_process.Kill(SIGKILL, kTimeoutAfterKillSeconds);
    return;
  }

  int converter_result = converter_process.Wait();
  if (converter_result != 0) {
    SetError(__func__,
             StringPrintf("Image converter process failed with result %d",
                          converter_result),
             &error);
    return;
  }

  LOG(INFO) << __func__ << ": completed image scan and conversion.";
}

// static
void Manager::SetError(const string &method,
                       const string &message,
                       ::DBus::Error *error) {
  error->set(kDBusErrorName, message.c_str());
  LOG(ERROR) << method << ": " << message;
}

}  // namespace lorgnette
