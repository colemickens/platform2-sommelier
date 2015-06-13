// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <unistd.h>

#include <base/logging.h>

#include "trunks/trunks_ftdi_spi.h"

// Assorted TPM2 registers for interface type FIFO.
#define TPM_ACCESS_REG       0
#define TPM_STS_REG       0x18
#define TPM_DATA_FIFO_REG 0x24
#define TPM_DID_VID_REG  0xf00
#define TPM_RID_REG      0xf04

namespace trunks {

// Locality management bits (in TPM_ACCESS_REG)
enum TpmAccessBits {
  tpmRegValidSts = (1 << 7),
  activeLocality = (1 << 5),
  requestUse = (1 << 1),
  tpmEstablishment = (1 << 0),
};

enum TpmStsBits {
  tpmFamilyShift = 26,
  tpmFamilyMask = ((1 << 2) - 1),  // 2 bits wide
  tpmFamilyTPM2 = 1,
  resetEstablishmentBit = (1 << 25),
  commandCancel = (1 << 24),
  burstCountShift = 8,
  burstCountMask = ((1 << 16) -1),  // 16 bits wide
  stsValid = (1 << 7),
  commandReady = (1 << 6),
  tpmGo = (1 << 5),
  dataAvail = (1 << 4),
  Expect = (1 << 3),
  selfTestDone = (1 << 2),
  responseRetry = (1 << 1),
};

  // SPI frame header for TPM transactions is 4 bytes in size, it is described
  // in section "6.4.6 Spi Bit Protocol" of the TCG issued "TPM Profile (PTP)
  // Specification Revision 00.43.
struct SpiFrameHeader {
  unsigned char body[4];
};

TrunksFtdiSpi::~TrunksFtdiSpi() {
  if (mpsse_)
    Close(mpsse_);

  mpsse_ = NULL;
}

bool TrunksFtdiSpi::ReadTpmSts(uint32_t *status) {
  return FtdiReadReg(TPM_STS_REG, sizeof(*status), status);
}

bool TrunksFtdiSpi::WriteTpmSts(uint32_t status) {
  return FtdiWriteReg(TPM_STS_REG, sizeof(status), &status);
}

void TrunksFtdiSpi::StartTransaction(bool read_write,
                                     size_t bytes, unsigned addr) {
  unsigned char *response;
  SpiFrameHeader header;

  // The first byte of the frame header encodes the transaction type (read or
  // write) and size (set to lenth - 1).
  header.body[0] = (read_write ? 0x80 : 0) | 0x40 | (bytes - 1);

  // The rest of the frame header is the internal address in the TPM
  for (int i = 0; i < 3; i++)
    header.body[i + 1] = (addr >> (8 * (2 - i))) & 0xff;

  Start(mpsse_);

  response = Transfer(mpsse_, header.body, sizeof(header.body));

  // The TCG TPM over SPI specification itroduces the notion of SPI flow
  // control (Section "6.4.5 Flow Control" of the TCG issued "TPM Profile
  // (PTP) Specification Revision 00.43).

  // The slave (TPM device) expects each transaction to start with a 4 byte
  // header trasmitted by master. If the slave needs to stall the transaction,
  // it sets the MOSI bit to 0 during the last clock of the 4 byte header. In
  // this case the master is supposed to start polling the line, byte at time,
  // until the last bit in the received byte (transferred during the last
  // clock of the byte) is set to 1.
  while (!(response[3] & 1)) {
    unsigned char *poll_state;

    poll_state = Read(mpsse_, 1);
    response[3] = *poll_state;
    free(poll_state);
  }
  free(response);
}

bool TrunksFtdiSpi::FtdiWriteReg(unsigned reg_number, size_t bytes,
                                 const void *buffer) {
  if (!mpsse_)
    return false;

  StartTransaction(false, bytes, reg_number + locality_ * 0x10000);
  Write(mpsse_, buffer, bytes);
  Stop(mpsse_);
  return true;
}

bool TrunksFtdiSpi::FtdiReadReg(unsigned reg_number, size_t bytes,
                                void *buffer) {
  unsigned char *value;

  if (!mpsse_)
    return false;

  StartTransaction(true, bytes, reg_number + locality_ * 0x10000);
  value = Read(mpsse_, bytes);
  if (buffer)
    memcpy(buffer, value, bytes);
  free(value);
  Stop(mpsse_);
  return true;
}

bool TrunksFtdiSpi::Init() {
  uint32_t did_vid, status;
  uint8_t cmd;

  if (mpsse_)
    return true;

  mpsse_ = MPSSE(SPI0, TWO_MHZ, MSB);
  if (!mpsse_)
    return false;

  // Reset the TPM using GPIOL0, issue a 100 ms long pulse.
  PinLow(mpsse_, GPIOL0);
  usleep(100000);
  PinHigh(mpsse_, GPIOL0);

  FtdiReadReg(TPM_DID_VID_REG, sizeof(did_vid), &did_vid);

  if ((did_vid & 0xffff) != 0x15d1) {
    LOG(ERROR) << "unknown vid: 0x" << std::hex << (did_vid & 0xffff);
    return false;
  }

  // Try claiming locality zero.
  FtdiReadReg(TPM_ACCESS_REG, sizeof(cmd), &cmd);
  if (cmd != (tpmRegValidSts | tpmEstablishment)) {
    LOG(ERROR) << "invalid reset status: 0x" << std::hex << (unsigned)cmd;
    return false;
  }
  cmd = requestUse;
  FtdiWriteReg(TPM_ACCESS_REG, sizeof(cmd), &cmd);
  FtdiReadReg(TPM_ACCESS_REG, sizeof(cmd), &cmd);
  if (cmd != (tpmRegValidSts | activeLocality | tpmEstablishment)) {
    LOG(ERROR) << "failed to claim locality, status: 0x" << std::hex
               << (unsigned)cmd;
    return false;
  }

  ReadTpmSts(&status);
  if (((status >> tpmFamilyShift) & tpmFamilyMask) != tpmFamilyTPM2) {
    LOG(ERROR) << "unexpected TPM family value, status: 0x" << std::hex
               << status;
    return false;
  }
  burst_count_ = (status >> burstCountShift) & burstCountMask;
  FtdiReadReg(TPM_RID_REG, sizeof(cmd), &cmd);
  printf("Connected to device vid:did:rid of %4.4x:%4.4x:%2.2x\n",
         did_vid & 0xffff, did_vid >> 16, cmd);

  return true;
}

void TrunksFtdiSpi::SendCommand(const std::string& command,
                                const ResponseCallback& callback) {
  printf("%s invoked\n", __func__);
}

bool TrunksFtdiSpi::WaitForStatus(uint32_t statusMask,
                                  uint32_t statusExpected, int timeout_ms) {
  uint32_t status;
  do {
    usleep(1000);
    if (!timeout_ms--) {
      LOG(ERROR) << "failed to get expected status " << std::hex
                 << statusExpected;
      return false;
    }
    ReadTpmSts(&status);
  } while ((status & statusMask) != statusExpected);
  return true;
}

std::string TrunksFtdiSpi::SendCommandAndWait(const std::string& command) {
  uint32_t status;
  uint32_t expected_status_bits;
  std::string rv("");

  if (command.length() > burst_count_) {
    LOG(ERROR) << "cannot (yet) transmit more than " << burst_count_
               << " bytes";
    return rv;
  }

  WriteTpmSts(commandReady);

  // No need to wait for the sts.Expect bit to be set, at least with the
  // 15d1:001b device, let's just write the command into FIFO.
  FtdiWriteReg(TPM_DATA_FIFO_REG, command.length(), command.c_str());

  // And tell the device it can start processing it.
  WriteTpmSts(tpmGo);

  expected_status_bits = stsValid | dataAvail;
  if (!WaitForStatus(expected_status_bits, expected_status_bits))
      return rv;

  // The response is ready, read it out byte by byte for now. Will be
  // optimized to read the length first an then the entire remaining data in
  // one shot.
  do {
    uint8_t c;

    FtdiReadReg(TPM_DATA_FIFO_REG, sizeof(c), &c);
    rv.push_back(c);
    ReadTpmSts(&status);
  } while ((status & expected_status_bits) == expected_status_bits);

  return rv;
}

}  // namespace trunks
