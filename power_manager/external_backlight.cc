// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/external_backlight.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dirent.h>
#include <fcntl.h>
#include <glib.h>
#include <inttypes.h>
#include <libudev.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <set>

#include "base/logging.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/power_constants.h"

namespace {

const int kDDCBrightnessIndex = 0x10;  // DDC interface brightness index.
const int kDDCMaxValue = 0xff;         // DDC values are 8 bit.

// udev subsystem names to watch for.
const char kI2CUdevSubsystem[] = "i2c-dev";
const char kDRMUdevSubsystem[] = "drm";

const int kBufSize = 1000;  // Buffer size for reading from an input stream.
const int kScanForDisplaysDelayMs = 2000;
// Time in ms to wait before retrying SendDisplayChangedSignal().
const int kRetrySendDisplayChangedSignalDelayMs = 100;

const int kDDCAddress = 0x37;
const int kDDCWriteAddress = kDDCAddress << 1;
const int kDDCReadAddress = kDDCWriteAddress + 1;
const int kDDCSubAddress = 0x51;
const int kDDCMaxMessageSize = 127;
const int kDDCMessageSizeMask = 0x80;

const int kDDCWritePacketLength = 4;
const int kDDCReadRequestPacketLength = 2;
const int kDDCReadResponsePacketLength = 8;

const int kDDCReadRequestCode = 0x01;
const int kDDCReadResponseCode = 0x02;
const int kDDCWriteCode = 0x03;

const int kDDCSleepUs = 45000;

const std::string ToHex(int value) {
  char buf[100];
  snprintf(buf, sizeof(buf), "0x%x", value);
  return buf;
}

bool DDCAccess(int handle, uint8* buf, int len, bool write) {
  if (len > kDDCMaxMessageSize) {
    LOG(ERROR) << "Message size " << len << " is too large, max is "
               << kDDCMaxMessageSize << ".";
    return false;
  }

  // Set up DDC/CI packet.
  // This is based on both the official VESA DDC/CI 1.1 spec and an
  // unofficial spec found at: http://www.boichat.ch/nicolas/ddcci/specs.html
  unsigned char* i2c_buf = new unsigned char[len + 3];
  CHECK(i2c_buf);
  i2c_buf[0] = write ? kDDCSubAddress : kDDCWriteAddress;
  i2c_buf[1] = kDDCMessageSizeMask | len;
  memcpy(i2c_buf + 2, buf, len);
  // Calculate XOR checksum.
  i2c_buf[len + 2] = write ? kDDCWriteAddress : kDDCReadAddress;
  for (int i = 0; i < len + 2; i++) {
    i2c_buf[len + 2] ^= i2c_buf[i];
  }

  struct i2c_rdwr_ioctl_data msg_rdwr;
  struct i2c_msg             i2cmsg;
  // Set up and send the I2C message
  msg_rdwr.msgs = &i2cmsg;
  msg_rdwr.nmsgs = 1;
  i2cmsg.addr  = kDDCAddress;
  i2cmsg.flags = write ? 0 : I2C_M_RD;
  i2cmsg.len   = len + 3;
  i2cmsg.buf   = i2c_buf;

  if (ioctl(handle, I2C_RDWR, &msg_rdwr) < 0) {
    LOG(ERROR) << (write ? "Writing to " : "Reading from ") << "file"
               << " failed!";
    delete[] i2c_buf;
    return false;
  }
  if (!write)
    memcpy(buf, i2c_buf + 2, len);
  delete[] i2c_buf;
  return true;
}

bool DDCWrite(int handle, uint8 index, uint8 value) {
  uint8 buf[kDDCWritePacketLength];
  buf[0] = kDDCWriteCode;
  buf[1] = index;
  buf[2] = 0;
  buf[3] = value;
  return DDCAccess(handle, buf, kDDCWritePacketLength, true);
}

bool DDCRead(int handle, uint8 index, uint8* value, uint8* max_value) {
  CHECK(value);
  CHECK(max_value);
  // Send out a packet to initiate read.
  uint8 request[kDDCReadRequestPacketLength];
  request[0] = kDDCReadRequestCode;
  request[1] = index;
  if (!DDCAccess(handle, request, kDDCReadRequestPacketLength, true))
    return false;
  usleep(kDDCSleepUs);

  // Read the response.
  uint8 buf[kDDCReadResponsePacketLength];
  memset(buf, 0, kDDCReadResponsePacketLength);
  if (!DDCAccess(handle, buf, kDDCReadResponsePacketLength, false))
    return false;

  // Make sure the packet received is in the correct format.
  uint8 expected[kDDCReadResponsePacketLength];
  memset(expected, 0, kDDCReadResponsePacketLength);
  expected[0] = kDDCReadResponseCode;
  expected[2] = index;
  expected[5] = buf[5];
  expected[7] = buf[7];
  int mismatches = 0;
  for (int i = 0; i < kDDCReadResponsePacketLength; ++i) {
    if (expected[i] == buf[i])
      continue;
    LOG(ERROR) << "DDC read packet mismatch, expected: " << ToHex(expected[i])
               << ", " << "actual: " << ToHex(buf[i]);
    mismatches += 1;
  }
  // If the response packet has the correct format, only then return the values.
  *value = buf[7];
  *max_value = buf[5];
  return true;
}

}  // namespace

namespace power_manager {

ExternalBacklight::ExternalBacklight()
    : udev_(NULL),
      is_scan_scheduled_(false),
      retry_send_display_changed_source_id_(0) {}

ExternalBacklight::~ExternalBacklight() {
  if (retry_send_display_changed_source_id_) {
    g_source_remove(retry_send_display_changed_source_id_);
    retry_send_display_changed_source_id_ = 0;
  }
  for (I2CDeviceList::iterator iter = display_devices_.begin();
       iter != display_devices_.end();
       ++iter) {
    close(iter->second);
  }
  if (udev_)
    udev_unref(udev_);
}

bool ExternalBacklight::Init() {
  RegisterUdevEventHandler();
  ScanForDisplays();
  // Schedule a backup call to ScanForDisplays() in case the system was slow in
  // picking up some displays.
  is_scan_scheduled_ = true;
  g_timeout_add(kScanForDisplaysDelayMs, ScanForDisplaysThunk, this);
  return true;
}

bool ExternalBacklight::GetMaxBrightnessLevel(int64* max_level) {
  return ReadBrightnessLevels(NULL, max_level);
}

bool ExternalBacklight::GetCurrentBrightnessLevel(int64* current_level) {
  return ReadBrightnessLevels(current_level, NULL);
}

bool ExternalBacklight::SetBrightnessLevel(int64 level) {
  if (!HasValidHandle()) {
    LOG(ERROR) << "No valid display device handle available.";
    return false;
  }
  if (level > kDDCMaxValue || level < 0) {
    LOG(ERROR) << "SetBrightness level " << level << " is invalid.";
    return false;
  }
  int8 level8 = static_cast<int8>(level);
  for (I2CDeviceList::const_iterator iter = display_devices_.begin();
       iter != display_devices_.end();
       ++iter) {
    if (!DDCWrite(iter->second, kDDCBrightnessIndex, level8))
      LOG(WARNING) << "DDC write failed.";
  }
  return true;
}

gboolean ExternalBacklight::UdevEventHandler(GIOChannel* /* source */,
                                             GIOCondition /* condition */,
                                             gpointer data) {
  ExternalBacklight* backlight = static_cast<ExternalBacklight*>(data);

  struct udev_device* dev =
      udev_monitor_receive_device(backlight->udev_monitor_);
  if (dev) {
    LOG(INFO) << "Udev event on ("
              << udev_device_get_subsystem(dev)
              << ") Action "
              << udev_device_get_action(dev)
              << ", Device "
              << udev_device_get_devpath(dev);
    CHECK(std::string(udev_device_get_subsystem(dev)) == kI2CUdevSubsystem ||
          std::string(udev_device_get_subsystem(dev)) == kDRMUdevSubsystem);
    udev_device_unref(dev);

    if (!backlight->is_scan_scheduled_) {
      backlight->is_scan_scheduled_ = true;
      g_timeout_add(kScanForDisplaysDelayMs, ScanForDisplaysThunk, backlight);
    }
  } else {
    LOG(ERROR) << "Can't get receive_device()";
    return false;
  }
  return true;
}

void ExternalBacklight::RegisterUdevEventHandler() {
  // Create the udev object.
  udev_ = udev_new();
  if (!udev_)
    LOG(ERROR) << "Can't create udev object.";

  // Create the udev monitor structure.
  udev_monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
  if (!udev_monitor_) {
    LOG(ERROR) << "Can't create udev monitor.";
    udev_unref(udev_);
  }

  udev_monitor_filter_add_match_subsystem_devtype(udev_monitor_,
                                                  kI2CUdevSubsystem,
                                                  NULL);
  udev_monitor_filter_add_match_subsystem_devtype(udev_monitor_,
                                                  kDRMUdevSubsystem,
                                                  NULL);
  udev_monitor_enable_receiving(udev_monitor_);

  int fd = udev_monitor_get_fd(udev_monitor_);
  GIOChannel* channel = g_io_channel_unix_new(fd);
  g_io_add_watch(channel, G_IO_IN, &(ExternalBacklight::UdevEventHandler),
                 this);
  LOG(INFO) << "Udev controller waiting for events on subsystems "
            << kI2CUdevSubsystem << " and " << kDRMUdevSubsystem;
}

gboolean ExternalBacklight::ScanForDisplays() {
  // Query for display devices using ddc control.
  FILE* fp = popen("ddccontrol -p | grep Device: | cut -f3 -d:", "r");
  is_scan_scheduled_ = false;
  if (!fp) {
    LOG(ERROR) << "Unable to call ddccontrol.";
    return false;
  }

  std::set<std::string> found_devices;
  char buf[kBufSize];
  while (fgets(buf, kBufSize, fp) != NULL) {
    std::string device_name;
    TrimWhitespaceASCII(buf, TRIM_ALL, &device_name);
    found_devices.insert(device_name);

    // Open the device if it doesn't exist already.
    if (display_devices_.count(device_name))
      continue;
    int handle = open(device_name.c_str(), O_RDWR);
    if (handle < 0) {
      LOG(ERROR) << "Unable to open handle to display device " << device_name;
      continue;
    }
    display_devices_[device_name] = handle;
  }
  fclose(fp);

  // If previously found devices don't exist anymore, close them.
  // TODO(sque): make sure this doesn't clobber an existing handle that's
  // being used elsewhere.
  for (I2CDeviceList::iterator iter = display_devices_.begin();
       iter != display_devices_.end(); /* |iter| advanced in loop */ ) {
    if (found_devices.count(iter->first) > 0) {
      ++iter;
      continue;
    }
    // Update the primary device name if the previous primary device is gone.
    if (iter->first == primary_device_)
      primary_device_.clear();
    close(iter->second);
    // Advance iterator to the next device before erasing the current device, to
    // avoid ending up with an invalid iterator.
    display_devices_.erase(iter++);
  }

  // Stop any existing pending SendDisplayChangedSignal events, as this function
  // will issue a new one.
  if (retry_send_display_changed_source_id_) {
    g_source_remove(retry_send_display_changed_source_id_);
    retry_send_display_changed_source_id_ = 0;
  }

  // If no devices remain, signal a display device change and quit.
  if (display_devices_.empty()) {
    SendDisplayChangedSignal();
    return false;
  }

  // Choose a new device as the primary device if the old one was removed, or if
  // none has been found until now.
  if (HasValidHandle())
    return false;
  primary_device_ = display_devices_.begin()->first;
  LOG(INFO) << "Selecting primary display device " << primary_device_;
  if (!HasValidHandle())
    LOG(WARNING) << "Invalid handle for device " << primary_device_;

  // Finally, send signal to indicate the display was updated.
  if (!SendDisplayChangedSignal()) {
    retry_send_display_changed_source_id_ =
        g_timeout_add(kRetrySendDisplayChangedSignalDelayMs,
                      RetrySendDisplayChangedSignalThunk, this);
  }
  return false;
}

bool ExternalBacklight::SendDisplayChangedSignal() {
  int64 current_level = 0, max_level = 0;
  if (!ReadBrightnessLevels(&current_level, &max_level))
    return false;

  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);

  DBusError error;
  dbus_error_init(&error);
  DBusMessage* signal = dbus_message_new_signal(kPowerManagerServicePath,
                                                kPowerManagerInterface,
                                                kExternalBacklightUpdate);
  CHECK(signal);
  dbus_message_append_args(signal,
                           DBUS_TYPE_INT64, &current_level,
                           DBUS_TYPE_INT64, &max_level,
                           DBUS_TYPE_INVALID);
  if (!dbus_connection_send(connection, signal, NULL))
    LOG(ERROR) << "Failed to send signal.";
  return true;
}

gboolean ExternalBacklight::RetrySendDisplayChangedSignal() {
  LOG(INFO) << "Retrying SendDisplayChangedSignal().";
  if (SendDisplayChangedSignal()) {
    retry_send_display_changed_source_id_ = 0;
    return FALSE;  // If successfully sent signal, do not retry.
  }
  return TRUE;
}

bool ExternalBacklight::ReadBrightnessLevels(int64* current_level,
                                             int64* max_level) {
  if (!HasValidHandle()) {
    // If there is no valid i2c handle, return some dummy values.  Since display
    // devices may be added at anytime, we don't want to return a failure just
    // because there is no device found.
    LOG(WARNING) << "No valid display device handle available, returning "
                 << "dummy values.";
    if (current_level)
      *current_level = 0;
    if (max_level)
      *max_level = 1;
    return true;
  }

  uint8 current_level8 = 0;
  uint8 max_level8 = 0;
  if (!DDCRead(display_devices_[primary_device_], kDDCBrightnessIndex,
               &current_level8, &max_level8)) {
    LOG(WARNING) << "DDC read failed.";
    return false;
  }
  LOG(INFO) << "Read DDC brightness: " << static_cast<int>(current_level8)
            << "/" << static_cast<int>(max_level8);
  if (max_level8 == 0) {
    LOG(ERROR) << "Invalid max brightness level read from DDC.";
    return false;
  }

  if (current_level)
    *current_level = static_cast<int64>(current_level8);
  if (max_level)
    *max_level = static_cast<int64>(max_level8);
  return true;
}

}  // namespace power_manager
