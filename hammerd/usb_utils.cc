// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hammerd/usb_utils.h"

#include <stdio.h>

#include <memory>

#include <base/logging.h>
#include <base/strings/stringprintf.h>

namespace hammerd {
namespace {
const int kError = -1;
const unsigned int kTimeoutMs = 1000;  // Default timeout value.
bool is_libusb_inited = false;
}  // namespace

bool InitLibUSB() {
  DLOG(INFO) << "Call InitLibUSB";
  if (is_libusb_inited) {
    DLOG(INFO) << "libusb is already initialized. Ignored.";
    return true;
  }
  int r = libusb_init(nullptr);
  if (r < 0) {
    LogUSBError("libusb_init", r);
    return false;
  }
  is_libusb_inited = true;
  return true;
}

void ExitLibUSB() {
  DLOG(INFO) << "Call ExitLibUSB";
  if (!is_libusb_inited) {
    LOG(INFO) << "libusb is not initialized. Ignored.";
    return;
  }
  libusb_exit(nullptr);
  is_libusb_inited = false;
}

void LogUSBError(const char* func_name, int return_code) {
  LOG(ERROR) << func_name << " returned " << return_code << " ("
             << libusb_strerror(static_cast<libusb_error>(return_code)) << ")";
}

UsbEndpoint::UsbEndpoint() : devh_(nullptr), ep_num_(0), chunk_len_(0) {}

UsbEndpoint::~UsbEndpoint() {
  Close();
  ExitLibUSB();
}

bool UsbEndpoint::Connect() {
  // NOTE: If multiple interfaces matched the VID/PID, then we connect the first
  // found one and ignore others.
  if (IsConnected()) {
    LOG(INFO) << "Already initialized. Ignore.";
    return true;
  }

  InitLibUSB();
  LOG(INFO) << base::StringPrintf(
      "open_device %x:%x", kUsbVidGoogle, kUsbPidGoogle);
  // TODO(akahuang): Make VID/PID configurable.
  devh_ =
      libusb_open_device_with_vid_pid(nullptr, kUsbVidGoogle, kUsbPidGoogle);
  if (!devh_) {
    LOG(ERROR) << "Can't find device.";
    Close();
    return false;
  }
  LOG(INFO) << "Get dev handle: " << devh_;

  int iface_num = FindInterface();
  if (iface_num < 0) {
    LOG(ERROR) << "USB FW update not supported by that device";
    Close();
    return false;
  }
  if (!chunk_len_) {
    LOG(ERROR) << "wMaxPacketSize isn't valid";
    Close();
    return false;
  }

  LOG(INFO) << "found interface " << iface_num << ", endpoint "
            << static_cast<int>(ep_num_) << ", chunk_len " << chunk_len_;

  libusb_set_auto_detach_kernel_driver(devh_, 1);
  int r = libusb_claim_interface(devh_, iface_num);
  if (r < 0) {
    LogUSBError("libusb_claim_interface", r);
    Close();
    return false;
  }
  // Flush data on the endpoint. This helps recover in case of error.
  std::unique_ptr<uint8_t[]> buf(new uint8_t[GetChunkLength()]);
  while (Receive(buf.get(), 0, true, 10) != kError) {
    LOG(INFO) << "Flush data...";
  }
  LOG(INFO) << "USB Endpoint is initialized successfully.";
  return true;
}

// Release USB device.
void UsbEndpoint::Close() {
  if (devh_ != nullptr) {
    libusb_close(devh_);
    devh_ = nullptr;
  }
  ep_num_ = 0;
  chunk_len_ = 0;
}

bool UsbEndpoint::IsConnected() const {
  return (devh_ != nullptr);
}

int UsbEndpoint::Transfer(const void* outbuf,
                          int outlen,
                          void* inbuf,
                          int inlen,
                          bool allow_less,
                          unsigned int timeout_ms) {
  if (Send(outbuf, outlen, timeout_ms) != outlen) {
    return kError;
  }
  if (inlen == 0) {
    return 0;
  }
  return Receive(inbuf, inlen, allow_less, timeout_ms);
}

int UsbEndpoint::Send(const void* outbuf, int outlen, unsigned int timeout_ms) {
  // BulkTransfer() does not modify the buffer while using LIBUSB_ENDPOINT_OUT
  // direction mask.
  int actual = BulkTransfer(
      const_cast<void*>(outbuf), LIBUSB_ENDPOINT_OUT, outlen, timeout_ms);
  DLOG(INFO) << "Sent " << actual << "/" << outlen << " bytes";
  if (actual != outlen) {
    LOG(ERROR) << "Failed to send the complete data.";
  }
  return actual;
}

int UsbEndpoint::Receive(void* inbuf,
                         int inlen,
                         bool allow_less,
                         unsigned int timeout_ms) {
  int actual = BulkTransfer(inbuf, LIBUSB_ENDPOINT_IN, inlen, timeout_ms);
  DLOG(INFO) << "Received " << actual << "/" << inlen << " bytes";
  if ((actual != inlen) && !allow_less) {
    LOG(ERROR) << "Failed to receive the complete data.";
    return kError;
  }
  return actual;
}

int UsbEndpoint::FindInterface() {
  if (!devh_) {
    LOG(ERROR) << "devh_ is nullptr.";
    return kError;
  }

  libusb_device* dev = libusb_get_device(devh_);
  libusb_config_descriptor* conf = nullptr;
  int r = libusb_get_active_config_descriptor(dev, &conf);
  if (r < 0) {
    LogUSBError("libusb_get_active_config_descriptor", r);
    libusb_free_config_descriptor(conf);
    return kError;
  }

  int iface_num = kError;
  for (int i = 0; i < conf->bNumInterfaces; i++) {
    iface_num = FindEndpoint(&conf->interface[i]);
    if (iface_num != kError) {
      break;
    }
  }

  libusb_free_config_descriptor(conf);
  return iface_num;
}

int UsbEndpoint::FindEndpoint(const libusb_interface* iface) {
  const libusb_interface_descriptor* iface_desc = nullptr;
  for (int iface_idx = 0; iface_idx < iface->num_altsetting; iface_idx++) {
    iface_desc = &iface->altsetting[iface_idx];
    if (iface_desc->bInterfaceClass == 255 &&
        iface_desc->bInterfaceSubClass == kUsbSubclassGoogleUpdate &&
        iface_desc->bInterfaceProtocol == kUsbProtocolGoogleUpdate &&
        iface_desc->bNumEndpoints) {
      const libusb_endpoint_descriptor* ep = &iface_desc->endpoint[0];
      ep_num_ = ep->bEndpointAddress & 0x7f;
      chunk_len_ = ep->wMaxPacketSize;
      return iface_idx;
    }
  }
  return kError;
}

int UsbEndpoint::BulkTransfer(void* buf,
                              enum libusb_endpoint_direction direction_mask,
                              int len,
                              unsigned int timeout_ms) {
  if (timeout_ms == 0) {
    timeout_ms = kTimeoutMs;
  }

  int actual = 0;
  int r = libusb_bulk_transfer(devh_,
                               ep_num_ | direction_mask,
                               reinterpret_cast<uint8_t*>(buf),
                               len,
                               &actual,
                               timeout_ms);
  if (r < 0) {
    LogUSBError("libusb_bulk_transfer", r);
    return kError;
  }
  return actual;
}

}  // namespace hammerd
