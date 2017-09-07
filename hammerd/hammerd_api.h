// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The external API for firmware testing. Because it is called by Python ctypes
// modules, we wrap it as C header file.

#ifndef HAMMERD_HAMMERD_API_H_
#define HAMMERD_HAMMERD_API_H_

#include <string>

#include <brillo/brillo_export.h>

#include "hammerd/update_fw.h"

extern "C" {

using hammerd::FirmwareUpdater;
using hammerd::SectionName;
using hammerd::UpdateExtraCommand;

// Expose FirmwareUpdater class.
BRILLO_EXPORT FirmwareUpdater* FirmwareUpdater_New(
    uint16_t vendor_id, uint16_t product_id, int bus, int port);
BRILLO_EXPORT bool FirmwareUpdater_TryConnectUSB(
    FirmwareUpdater* updater);
BRILLO_EXPORT void FirmwareUpdater_CloseUSB(
    FirmwareUpdater* updater);
BRILLO_EXPORT bool FirmwareUpdater_LoadImage(
    FirmwareUpdater* updater, std::string image);
BRILLO_EXPORT SectionName FirmwareUpdater_CurrentSection(
    FirmwareUpdater* updater);
BRILLO_EXPORT bool FirmwareUpdater_UpdatePossible(
    FirmwareUpdater* updater, SectionName section_name);
BRILLO_EXPORT bool FirmwareUpdater_VersionMismatch(
    FirmwareUpdater* updater, SectionName section_name);
BRILLO_EXPORT bool FirmwareUpdater_TransferImage(
    FirmwareUpdater* updater, SectionName section_name);
BRILLO_EXPORT bool FirmwareUpdater_InjectEntropy(
    FirmwareUpdater* updater);
BRILLO_EXPORT bool FirmwareUpdater_SendSubcommand(
    FirmwareUpdater* updater, UpdateExtraCommand subcommand);
BRILLO_EXPORT bool FirmwareUpdater_SendFirstPDU(
    FirmwareUpdater* updater);
BRILLO_EXPORT void FirmwareUpdater_SendDone(
    FirmwareUpdater* updater);

}  // extern "C"
#endif  // HAMMERD_HAMMERD_API_H_
