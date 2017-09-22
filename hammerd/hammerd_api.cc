// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hammerd/hammerd_api.h"

#include <base/memory/ptr_util.h>

#include "hammerd/usb_utils.h"

extern "C" {

using hammerd::FirmwareUpdater;
using hammerd::SectionName;
using hammerd::UpdateExtraCommand;
using hammerd::UsbEndpoint;

BRILLO_EXPORT FirmwareUpdater* FirmwareUpdater_New(
    uint16_t vendor_id, uint16_t product_id, int bus, int port) {
  return new FirmwareUpdater(
      base::MakeUnique<UsbEndpoint>(vendor_id, product_id, bus, port));
}
BRILLO_EXPORT UsbConnectStatus FirmwareUpdater_TryConnectUSB(
    FirmwareUpdater* updater) {
  return updater->TryConnectUSB();
}
BRILLO_EXPORT void FirmwareUpdater_CloseUSB(
    FirmwareUpdater* updater) {
  updater->CloseUSB();
}
BRILLO_EXPORT bool FirmwareUpdater_LoadECImage(
    FirmwareUpdater* updater, std::string image) {
  return updater->LoadECImage(image);
}
BRILLO_EXPORT SectionName FirmwareUpdater_CurrentSection(
    FirmwareUpdater* updater) {
  return updater->CurrentSection();
}
BRILLO_EXPORT bool FirmwareUpdater_UpdatePossible(
    FirmwareUpdater* updater, SectionName section_name) {
  return updater->UpdatePossible(section_name);
}
BRILLO_EXPORT bool FirmwareUpdater_VersionMismatch(
    FirmwareUpdater* updater, SectionName section_name) {
  return updater->VersionMismatch(section_name);
}
BRILLO_EXPORT bool FirmwareUpdater_TransferImage(
    FirmwareUpdater* updater, SectionName section_name) {
  return updater->TransferImage(section_name);
}
BRILLO_EXPORT bool FirmwareUpdater_InjectEntropy(
    FirmwareUpdater* updater) {
  return updater->InjectEntropy();
}
BRILLO_EXPORT bool FirmwareUpdater_SendSubcommand(
    FirmwareUpdater* updater, UpdateExtraCommand subcommand) {
  return updater->SendSubcommand(subcommand);
}
BRILLO_EXPORT bool FirmwareUpdater_SendFirstPDU(
    FirmwareUpdater* updater) {
  return updater->SendFirstPDU();
}
BRILLO_EXPORT void FirmwareUpdater_SendDone(
    FirmwareUpdater* updater) {
  return updater->SendDone();
}

}  // extern "C"
