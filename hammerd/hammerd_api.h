// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The external API for firmware testing. Because it is called by Python ctypes
// modules, we wrap it as C header file.
// When we expose a C++ class, the wrapper function of its constructor should be
// named as "<class_name>_New", and return a pointer to the instance.
// The wrapper function of a public method should be named as
// "<class_name>_<method_name>".

#ifndef HAMMERD_HAMMERD_API_H_
#define HAMMERD_HAMMERD_API_H_

#include <brillo/brillo_export.h>

#include "hammerd/update_fw.h"

extern "C" {

using hammerd::FirmwareUpdater;
using hammerd::SectionName;
using hammerd::UpdateExtraCommand;
using hammerd::UsbConnectStatus;
using hammerd::FirstResponsePdu;

// The intermediary type for converting Python string to C++ std::string.
// It is used to store either a normal string ending with '\0' or binary data.
struct ByteString {
  char* ptr;
  size_t size;
};

// Expose the global constant.
BRILLO_EXPORT int kEntropySize = hammerd::kEntropySize;

// Expose FirmwareUpdater class.
BRILLO_EXPORT FirmwareUpdater* FirmwareUpdater_New(
    uint16_t vendor_id, uint16_t product_id, int bus, int port);
BRILLO_EXPORT bool FirmwareUpdater_LoadEcImage(
    FirmwareUpdater* updater, const ByteString* ec_image);
BRILLO_EXPORT bool FirmwareUpdater_LoadTouchpadImage(
    FirmwareUpdater* updater, const ByteString* touchpad_image);
BRILLO_EXPORT UsbConnectStatus FirmwareUpdater_TryConnectUsb(
    FirmwareUpdater* updater);
BRILLO_EXPORT void FirmwareUpdater_CloseUsb(
    FirmwareUpdater* updater);
BRILLO_EXPORT bool FirmwareUpdater_SendFirstPdu(
    FirmwareUpdater* updater);
BRILLO_EXPORT void FirmwareUpdater_SendDone(
    FirmwareUpdater* updater);
BRILLO_EXPORT bool FirmwareUpdater_InjectEntropy(
    FirmwareUpdater* updater);
BRILLO_EXPORT bool FirmwareUpdater_InjectEntropyWithPayload(
    FirmwareUpdater* updater, const ByteString* payload);
BRILLO_EXPORT bool FirmwareUpdater_SendSubcommand(
    FirmwareUpdater* updater, UpdateExtraCommand subcommand);
BRILLO_EXPORT bool FirmwareUpdater_SendSubcommandWithPayload(
    FirmwareUpdater* updater,
    UpdateExtraCommand subcommand, const ByteString* cmd_body);
BRILLO_EXPORT bool FirmwareUpdater_SendSubcommandReceiveResponse(
    FirmwareUpdater* updater,
    UpdateExtraCommand subcommand, const ByteString* cmd_body,
    void* resp, size_t resp_size);
BRILLO_EXPORT bool FirmwareUpdater_TransferImage(
    FirmwareUpdater* updater, SectionName section_name);
BRILLO_EXPORT bool FirmwareUpdater_TransferTouchpadFirmware(
    FirmwareUpdater* updater, uint32_t section_addr, size_t data_len);
BRILLO_EXPORT SectionName FirmwareUpdater_CurrentSection(
    FirmwareUpdater* updater);
BRILLO_EXPORT bool FirmwareUpdater_ValidKey(
    FirmwareUpdater* updater);
BRILLO_EXPORT bool FirmwareUpdater_ValidRollback(
    FirmwareUpdater* updater);
BRILLO_EXPORT bool FirmwareUpdater_VersionMismatch(
    FirmwareUpdater* updater, SectionName section_name);
BRILLO_EXPORT bool FirmwareUpdater_IsSectionLocked(
    FirmwareUpdater* updater, SectionName section_name);
BRILLO_EXPORT bool FirmwareUpdater_UnlockSection(
    FirmwareUpdater* updater, SectionName section_name);
BRILLO_EXPORT bool FirmwareUpdater_IsRollbackLocked(
    FirmwareUpdater* updater);
BRILLO_EXPORT bool FirmwareUpdater_UnlockRollback(
    FirmwareUpdater* updater);
BRILLO_EXPORT const FirstResponsePdu* FirmwareUpdater_GetFirstResponsePdu(
    FirmwareUpdater* updater);
BRILLO_EXPORT const char* FirmwareUpdater_GetSectionVersion(
    FirmwareUpdater* updater, SectionName section_name);

}  // extern "C"
#endif  // HAMMERD_HAMMERD_API_H_
