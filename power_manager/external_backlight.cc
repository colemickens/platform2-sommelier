// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/external_backlight.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dirent.h>
#include <fcntl.h>
#include <glib.h>
#include <inttypes.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

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
  sprintf(buf, "0x%x", value);
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

} // namespace

namespace power_manager {

ExternalBacklight::ExternalBacklight()
    : i2c_handle_(-1),
      udev_(NULL),
      is_scan_scheduled_(false) {}

ExternalBacklight::~ExternalBacklight() {
  if (!HasValidHandle())
    return;
  close(i2c_handle_);
  if (udev_)
    udev_unref(udev_);
}

bool ExternalBacklight::Init() {
  RegisterUdevEventHandler();
  ScanForDisplays();
  return true;
}

bool ExternalBacklight::GetBrightness(int64* level, int64* max_level) {
  CHECK(level);
  CHECK(max_level);
  if (!HasValidHandle()) {
    // If there is no valid i2c handle, return some dummy values.  Since display
    // devices may be added at anytime, we don't want to return a failure just
    // because there is no device found.
    LOG(WARNING) << "No valid display device handle available, returning "
                 << "dummy values.";
    *level = 0;
    *max_level = 1;
    return true;
  }

  uint8 level8 = 0;
  uint8 max_level8 = 0;
  if (!DDCRead(i2c_handle_, kDDCBrightnessIndex, &level8, &max_level8)) {
    LOG(WARNING) << "DDC read failed.";
    return false;
  }
  *level = static_cast<int64>(level8);
  *max_level = static_cast<int64>(max_level8);
  return true;
}

bool ExternalBacklight::SetBrightness(int64 level) {
  if (!HasValidHandle()) {
    LOG(ERROR) << "No valid display device handle available.";
    return false;
  }
  if (level > kDDCMaxValue || level < 0) {
    LOG(ERROR) << "SetBrightness level " << level << " is invalid.";
    return false;
  }
  if (!DDCWrite(i2c_handle_, kDDCBrightnessIndex, static_cast<int8>(level))) {
    LOG(WARNING) << "DDC write failed.";
    return false;
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
  if (!udev_monitor_ ) {
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
  char buf[kBufSize];
  std::string new_device;
  std::vector<std::string> device_list;
  while (fgets(buf, kBufSize, fp) != NULL) {
    TrimWhitespaceASCII(buf, TRIM_ALL, &new_device);
    // If a previously found device still exists, there is nothing to do.
    if (!i2c_path_.empty() && i2c_path_ == new_device) {
      LOG(INFO) << i2c_path_ << " still exists, not changing anything.";
      fclose(fp);
      return false;
    }
    device_list.push_back(new_device);
  }
  fclose(fp);

  if (!i2c_path_.empty()) {
    // If a previously found device does not exist anymore, close it.
    // TODO(sque): does this need a semaphore?
    close(i2c_handle_);
    i2c_handle_ = -1;
    i2c_path_.clear();
  }

  if (device_list.empty()) {
    SendDisplayChangedSignal();
    return false;
  }
  // Open a new device if one was found.
  // TODO(sque): Take all new devices, not just one.
  LOG(INFO) << "Selecting external backlight device at " << device_list.front();
  i2c_path_ = device_list.front();
  i2c_handle_ = open(i2c_path_.c_str(), O_RDWR);
  if (!HasValidHandle())
    LOG(WARNING) << "Unable to open display device handle.";
  SendDisplayChangedSignal();
  return false;
}

void ExternalBacklight::SendDisplayChangedSignal() {
  int64 level;
  int64 max_level;
  if (!GetBrightness(&level, &max_level))
    return;

  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);

  DBusError error;
  dbus_error_init(&error);
  DBusMessage *signal = dbus_message_new_signal(kPowerManagerServicePath,
                                                kPowerManagerInterface,
                                                kExternalBacklightUpdate);
  CHECK(signal);
  dbus_message_append_args(signal,
                           DBUS_TYPE_INT64, &level,
                           DBUS_TYPE_INT64, &max_level,
                           DBUS_TYPE_INVALID);
  if (!dbus_connection_send(connection, signal, NULL))
    LOG(ERROR) << "Failed to send signal.";
}

}  // namespace power_manager
