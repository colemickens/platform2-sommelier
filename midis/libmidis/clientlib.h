// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_LIBMIDIS_CLIENTLIB_H_
#define MIDIS_LIBMIDIS_CLIENTLIB_H_

#include <stdint.h>
#include "midis/messages.h"

#ifdef __cplusplus
extern "C" {
#endif

// Connect client to the MIDI Server.
//
// This function opens a socket on the supplied file path associated with MIDIS,
// and then establishes a connection to it.
//
// The fd returned is used to listen to and send control messages to MIDIS, like
// requesting a list of devices, receiving device added/removed notifications
// and requesting access to a subdevice.
//
// Args:
//   socket_path: String pointer to the file path for the MIDIS socket.
//
// Returns:
//   fd corresponding to UNIX Domain Socket connected to MIDIS on success.
//   -errno otherwise.
int MidisConnectToServer(const char* socket_path);

// Parse the header when there is a pending message from MIDIS.
//
// This function reads a message header from the supplied fd and returns
// the message type.
//
// NOTE: The caller should supply a pointer to a uint32_t variable which will
// be filled with the size of the message payload.
//
// Args:
//   fd: client fd to communicate with MIDIS.
//   payload_size: pointer to a variable which will store the payload size.
//   type: pointer to a variable which will store the message type.
//
// Returns:
//   0 on success.
//   -errno otherwise.
int MidisProcessMsgHeader(int fd, uint32_t* payload_size, uint32_t* type);

// Request a list of currently connected MIDI devices.
//
// If successful, the service will send a message back to the client, containing
// the list of currently connected MIDI devices.
//
// Args:
//   fd: client fd to communicate with MIDIS.
//
// Returns:
//   0 on success.
//   -errno otherwise.
int MidisListDevices(int fd);

// Process the list of devices sent by MIDIS
//
// This function copies a buffer containing several `struct MidisDeviceInfo`
// corresponding to the list of the devices currently connected, into a buffer.
// The buffer should be allocated by the caller, and the buffer size should be
// at least as big as the payload size.
//
// The buffer is filled in the following format:
//
// |   1 byte    |  sizeof(MidisDeviceInfo) |
// | num_devices |      device_info_1       | device_info_2 | .. | device_info_n
//
// Args:
//   fd: client fd to communicate with MIDIS.
//   buf: A buffer to store the device information.
//   buf_len: Size of the allocated buffer.
//   payload_size: Size of the payload we will be receiving.
// Returns:
//   number of bytes read on sucess.
//   -errno otherwise.
int MidisProcessListDevices(int fd, uint8_t* buf, size_t buf_len,
                            uint32_t payload_size);

// Request a fd to listen to a port of a MIDI device.
//
// If successful, the service will send a message back to the client, containing
// the FD for the requested port.
//
// Args:
//   fd: client fd to communicate with MIDIS.
//   port_msg: pointer to a struct containing card, device_num and
//     subdevice_num information of the requested port.
//
// Returns:
//   0 on success.
//   -errno otherwise.
int MidisRequestPort(int fd, const struct MidisRequestPort* port_msg);

// Receive a message containing an fd for a requested port.
//
// On success, the struct pointed to by port_msg will be filled up with
// information regarding the port for which the fd is being returned.
//
// NOTE: The port_msg pointer should be allocated by the caller.
// Args:
//   fd: client fd to communicate with MIDIS.
//   port_msg: pointer to a struct containing card, device_num and
//     subdevice_num information of the requested port.
//
// Returns:
//   fd to listen/write MIDI messages to on success
//   -errno otherwise.
int MidisProcessRequestPortResponse(int fd, struct MidisRequestPort* port_msg);

// Get information about a device which was added or removed.
//
// NOTE: The caller must allocate and pass in a pointer to a MidisDeviceInfo
// struct, which will be filled with information pertaining to the device
// which was added or removed.
//
// Args:
//   fd: client FD to communicate with MIDIS
//   dev_info: pointer to a `struct MidisDeviceInfo` in which the device info
//     will be filled.
//
// Returns:
//   0 on success.
//   -errno otherwise.
int MidisProcessDeviceAddedRemoved(int fd, struct MidisDeviceInfo* dev_info);

#ifdef __cplusplus
}
#endif

#endif  // MIDIS_LIBMIDIS_CLIENTLIB_H_
