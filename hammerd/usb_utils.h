// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains wrapper of libusb functions.

#ifndef HAMMERD_USB_UTILS_H_
#define HAMMERD_USB_UTILS_H_

#include <libusb.h>
#include <stdint.h>

#include <string>

#include <base/macros.h>

namespace hammerd {

// TODO(akahuang): Make them as argument or include usb_descriptor.h
constexpr uint16_t kUsbVidGoogle = 0x18d1;
constexpr uint16_t kUsbPidGoogle = 0x5022;
constexpr uint8_t kUsbSubclassGoogleUpdate = 0x53;
constexpr uint8_t kUsbProtocolGoogleUpdate = 0xff;

bool InitLibUSB();
void ExitLibUSB();
void LogUSBError(const char* func_name, int return_code);

class UsbEndpoint {
 public:
  UsbEndpoint();
  virtual ~UsbEndpoint();

  // Initializes the USB endpoint.
  virtual bool Connect();
  // Releases USB endpoint.
  virtual void Close();
  // Returns whether the USB endpoint is initialized.
  virtual bool IsConnected() const;

  // Sends the data to USB endpoint and then reads the result back.
  // Returns the byte number of the received data. -1 if the process fails.
  int Transfer(const void* outbuf,
               int outlen,
               void* inbuf,
               int inlen,
               bool allow_less,
               unsigned int timeout_ms = 0);
  // Sends the data to USB endpoint.
  // Returns the byte number of the received data.
  virtual int Send(const void* outbuf, int outlen, unsigned int timeout_ms = 0);
  // Receives the data from USB endpoint.
  // Returns the byte number of the received data. -1 if the amount of received
  // data is not as required and `allow_less` argument is false.
  virtual int Receive(void* inbuf,
                      int inlen,
                      bool allow_less = false,
                      unsigned int timeout_ms = 0);

  // Gets the chunk length of the USB endpoint.
  virtual size_t GetChunkLength() const { return chunk_len_; }

  // Gets the configuration string of the USB endpoint.
  virtual std::string GetConfigurationString() const {
    return configuration_string_;
  }

 private:
  // Returns the descriptor at the given index as an ASCII string.
  std::string GetStringDescriptorAscii(uint8_t index);
  // Finds the interface number. Returns -1 on error.
  int FindInterface();
  // Find the USB endpoint of the hammer EC.
  int FindEndpoint(const libusb_interface* iface);

  // Wrapper of libusb_bulk_transfer.
  // Returns the actual transfered data size. -1 if the transmission fails.
  // If the timeout is not assigned, then use default timeout value.
  int BulkTransfer(void* buf,
                   enum libusb_endpoint_direction direction_mask,
                   int len,
                   unsigned int timeout_ms = 0);

  libusb_device_handle* devh_;
  std::string configuration_string_;
  uint8_t ep_num_;
  size_t chunk_len_;
  DISALLOW_COPY_AND_ASSIGN(UsbEndpoint);
};

}  // namespace hammerd
#endif  // HAMMERD_USB_UTILS_H_
