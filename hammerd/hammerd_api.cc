// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hammerd/hammerd_api.h"

#include <memory>

#include "hammerd/usb_utils.h"

extern "C" {

using hammerd::FirmwareUpdater;
using hammerd::SectionName;
using hammerd::UpdateExtraCommand;
using hammerd::UsbEndpoint;

BRILLO_EXPORT FirmwareUpdater* FirmwareUpdater_New(
    uint16_t vendor_id, uint16_t product_id, int bus, int port) {
  return new FirmwareUpdater(
      std::make_unique<UsbEndpoint>(vendor_id, product_id, bus, port));
}
BRILLO_EXPORT UsbConnectStatus FirmwareUpdater_TryConnectUsb(
    FirmwareUpdater* updater) {
  return updater->TryConnectUsb();
}
BRILLO_EXPORT void FirmwareUpdater_CloseUsb(
    FirmwareUpdater* updater) {
  updater->CloseUsb();
}
BRILLO_EXPORT bool FirmwareUpdater_LoadEcImage(
    FirmwareUpdater* updater, std::string image) {
  return updater->LoadEcImage(image);
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
BRILLO_EXPORT bool FirmwareUpdater_SendFirstPdu(
    FirmwareUpdater* updater) {
  return updater->SendFirstPdu();
}
BRILLO_EXPORT void FirmwareUpdater_SendDone(
    FirmwareUpdater* updater) {
  return updater->SendDone();
}

}  // extern "C"
