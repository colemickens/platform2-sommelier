// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <unistd.h>

#include <base/logging.h>

#include "trunks/trunks_ftdi_spi.h"

// Assorted TPM2 registers for interface type FIFO.
#define TPM_ACCESS_REG      0
#define TPM_STS_REG      0x18
#define TPM_DID_VID_REG 0xf00
#define TPM_RID_REG     0xf04

namespace trunks {

// Locality management bits (in TPM_ACCESS_REG)
enum TpmAccessBits {
  tpmRegValidSts = (1 << 7),
  activeLocality = (1 << 5),
  requestUse = (1 << 1),
  tpmEstablishment = (1 << 0),
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
                                 void *buffer, int locality) {
  if (!mpsse_)
    return false;

  StartTransaction(false, bytes, reg_number + locality * 0x10000);
  Write(mpsse_, buffer, bytes);
  Stop(mpsse_);
  return true;
}

bool TrunksFtdiSpi::FtdiReadReg(unsigned reg_number, size_t bytes,
                                void *buffer, int locality) {
  unsigned char *value;

  if (!mpsse_)
    return false;

  StartTransaction(true, bytes, reg_number + locality * 0x10000);
  value = Read(mpsse_, bytes);
  if (buffer)
    memcpy(buffer, value, bytes);
  free(value);
  Stop(mpsse_);
  return true;
}

bool TrunksFtdiSpi::Init() {
  uint32_t did_vid;
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

  FtdiReadReg(TPM_RID_REG, sizeof(cmd), &cmd);
  printf("Connected to device vid:did:rid of %4.4x:%4.4x:%2.2x\n",
         did_vid & 0xffff, did_vid >> 16, cmd);

  return true;
}

void TrunksFtdiSpi::SendCommand(const std::string& command,
                                const ResponseCallback& callback) {
  printf("%s invoked\n", __func__);
}

std::string TrunksFtdiSpi::SendCommandAndWait(const std::string& command) {
  return std::string("");
}

}  // namespace trunks
