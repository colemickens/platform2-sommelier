// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LORGNETTE_MANAGER_H_
#define LORGNETTE_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <chromeos/variant_dictionary.h>
#include <chromeos/errors/error.h>

#include "lorgnette/dbus_adaptors/org.chromium.lorgnette.Manager.h"

namespace chromeos {
class Process;

namespace dbus_utils {
class ExportedObjectManager;
}  // namespace dbus_utils

}  // namespace chromeos

namespace lorgnette {

class Minijail;

class Manager
    : public org::chromium::lorgnette::ManagerAdaptor::MethodInterface {
 public:
  typedef std::map<std::string, std::map<std::string, std::string>> ScannerInfo;

  explicit Manager(base::Callback<void()> activity_callback);
  virtual ~Manager();

  // Start DBus connection.
  void InitDBus(chromeos::dbus_utils::ExportedObjectManager *object_manager);

  // Implementation of MethodInterface.
  virtual ScannerInfo ListScanners(chromeos::ErrorPtr *error);
  virtual void ScanImage(
      chromeos::ErrorPtr *error,
      const std::string &device_name,
      const dbus::FileDescriptor &outfd,
      const chromeos::VariantDictionary &scan_properties);

 private:
  friend class ManagerTest;

  static const char kScanConverterPath[];
  static const char kScanImageFormattedDeviceListCmd[];
  static const char kScanImagePath[];
  static const int kTimeoutAfterKillSeconds;

  // Sets arguments to scan listing |process|, and runs it, returning its
  // output to |fd|.
  static void RunListScannersProcess(int fd, chromeos::Process *process);

  // Starts a scan on |device_name|, outputting PNG image data to |out_fd|.
  // Uses the |pipe_fd_input| and |pipe_fd_output| to transport image data
  // from |scan_process| to |convert_process|.  Uses information from
  // |scan_properties| to set the arguments to the |scan_process|.  Runs both
  // |scan_process| and |convert_process|.  Returns true if |pipe_fds| were
  // consumed, false otherwise.
  static void RunScanImageProcess(
      const std::string &device_name,
      int out_fd,
      base::ScopedFD *pipe_fd_input,
      base::ScopedFD *pipe_fd_output,
      const chromeos::VariantDictionary &scan_properties,
      chromeos::Process *scan_process,
      chromeos::Process *convert_process,
      chromeos::ErrorPtr *error);

  // Converts the formatted output of "scanimage" to a map of attribute-data
  // mappings suitable for returning to a caller to the ListScanners DBus
  // method.
  static ScannerInfo ScannerInfoFromString(
      const std::string &scanner_info_string);

  // Sets the DBus error message and outputs a message to the logs.
  static void SetError(const std::string &method,
                       const std::string &message,
                       chromeos::ErrorPtr *error);

  std::unique_ptr<org::chromium::lorgnette::ManagerAdaptor> dbus_adaptor_;
  base::Callback<void()> activity_callback_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace lorgnette

#endif  // LORGNETTE_MANAGER_H_
