// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_QMI_CONSTANTS_H_
#define HERMES_QMI_CONSTANTS_H_

#include <array>

namespace hermes {

// This is also defined in qmi_uim.h, but cannot be removed since C does not
// have constexpr and the macro must be present in qmi_uim.c. This mimics that
// value.
constexpr size_t kBufferDataSize = 256;

// Constants for opening a libqrtr socket, these are QMI/QRTR specific.
constexpr uint8_t kQrtrPort = 0;
constexpr uint8_t kQrtrUimService = 11;

// TODO(jruthe): Currently, this is a "tag" that's not defined in SGP.22 to
// communicate that another packet with a payload must be sent to the eSIM.
constexpr uint16_t kGetMoreResponseTag = 0xFFFF;

// QMI UIM Info1 tag as specified in SGP.22 ES10b.GetEuiccInfo
constexpr uint16_t kEsimInfo1Tag = 0xBF20;

// QMI UIM Challenge tag as specified in SGP.22 ES10b.GetEuiccChallenge
constexpr uint16_t kEsimChallengeTag = 0xBF2E;

// TODO(jruthe): this is currently the slot on Cheza, but Esim should be able to
// support different slots in the future.
constexpr uint8_t kEsimSlot = 0x01;

constexpr uint8_t kInvalidChannel = -1;

// QMI UIM command codes as specified by QMI UIM service.
enum class QmiUimCommand : uint16_t {
  kReset = 0x0000,
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
