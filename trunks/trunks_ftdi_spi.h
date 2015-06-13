// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TRUNKS_FTDI_SPI_H_
#define TRUNKS_TRUNKS_FTDI_SPI_H_

#include <string>

#include <base/macros.h>

#include "trunks/command_transceiver.h"
#include "trunks/trunks_export.h"

#if defined SPI_OVER_FTDI

#include "trunks/ftdi/mpsse.h"

namespace trunks {

// TrunksFtdiSpi is a CommandTransceiver implementation that forwards all
// commands to the SPI over FTDI interface directly to a TPM chip.
class TRUNKS_EXPORT TrunksFtdiSpi: public CommandTransceiver {
 public:
  TrunksFtdiSpi() : mpsse_(NULL) {}
  ~TrunksFtdiSpi() override;

  // CommandTransceiver methods.
  bool Init() override;
  void SendCommand(const std::string& command,
                   const ResponseCallback& callback) override;
  std::string SendCommandAndWait(const std::string& command) override;

 private:
  struct mpsse_context* mpsse_;
  size_t burst_count_;  // As reported by the TPM_STS register

  // Read a TPM register into the passed in buffer, where 'bytes' the width of
  // the register. Return true on success, false on failure.
  bool FtdiReadReg(unsigned reg_number, size_t bytes,
                   void *buffer, int locality = 0);
  // Write a TPM register from the passed in buffer, where 'bytes' the width of
  // the register. Return true on success, false on failure.
  bool FtdiWriteReg(unsigned reg_number, size_t bytes,
                    void *buffer, int locality = 0);
  // Generate a proper SPI frame for read/write transaction, read_write set to
  // true for read transactions, the size of the transaction is passed as
  // 'bytes', addr is the internal TPM address space address (accounting for
  // locality).
  //
  // Note that this function is expected to be called when the SPI bus is idle
  // (CS deasserted), and will assert the CS before transmitting.
  void StartTransaction(bool read_write, size_t bytes, unsigned addr);
  // TPM Status Register is going to be accessed a lot, let's have dedicated
  // accessors for it,
  bool ReadTpmSts(uint32_t *status);
  bool WriteTpmSts(uint32_t status);

  DISALLOW_COPY_AND_ASSIGN(TrunksFtdiSpi);
};

}  // namespace trunks

#else  // SPI_OVER_FTDI ^^^^ defined  vvvvv NOT defined

namespace trunks {

// A plug to support compilations on platforms where FTDI SPI interface is not
// available.
class TRUNKS_EXPORT TrunksFtdiSpi: public CommandTransceiver {
 public:
  TrunksFtdiSpi() {}
  ~TrunksFtdiSpi() {}

  bool Init() { return false; }
  void SendCommand(const std::string& command,
                   const ResponseCallback& callback) {}
  std::string SendCommandAndWait(const std::string& command) {
    return std::string(""); }
};

}  // namespace trunks

#endif  // SPI_OVER_FTDI ^^^^ NOT defined

#endif  // TRUNKS_TRUNKS_FTDI_SPI_H_
