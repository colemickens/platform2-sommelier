// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_QMI_CONSTANTS_H_
#define HERMES_QMI_CONSTANTS_H_

namespace hermes {

// Constants for opening a libqrtr socket, these are QMI/QRTR specific.
constexpr uint8_t kQrtrPort = 0;
constexpr uint8_t kQrtrUimService = 11;

// QMI UIM Info1 tag as specified in SGP.22 ES10b.GetEuiccInfo
constexpr uint16_t kEsimInfo1 = 0xBF20;

// TODO(jruthe): this is currently the slot on Cheza, but Esim should be able to
// support different slots in the future.
constexpr uint8_t kEsimSlot = 0x01;

// QMI UIM command codes as specified by QMI UIM service.
enum class QmiCommand {
  kSendApdu = 0x003B,
  kLogicalChannel = 0x003F,
  kOpenLogicalChannel = 0x0042,
};

// QMI result codes as specified in SGP.22 2.3.1
enum class EsimQmiResult {
  kQmiResultSuccess,
  kQmiResultFailure,
};

// This list currently only contains QMI error codes specified for the functions
// necessary for LOGICAL_CHANNEL and SEND_APDU QMI commands, and will be
// expanded as more QMI integration is added.
enum class EsimQmiError {
  kQmiErrorNone,
  kQmiErrorInternal,
  kQmiMalformedMsg,
  kQmiNoMemory,
  kQmiInvalidArg,
  kQmiArgTooLong,
  kQmiMissingArg,
  kQmiInsufficientResources,
  kQmiSimFileNotFound,
  kQmiAccessDenied,
  kQmiIncompatibleState,
};

}  // namespace hermes

#endif  // HERMES_QMI_CONSTANTS_H_
