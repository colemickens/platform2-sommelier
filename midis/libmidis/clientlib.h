// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_LIBMIDIS_CLIENTLIB_H_
#define MIDIS_LIBMIDIS_CLIENTLIB_H_

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Miscellaneous constants used by the server.
static const size_t kMidisStringSize = 256;
static const uint8_t kMidisMaxDevices = 7;

// Enum listing the types of messages a client can send.
enum ClientMsgType {
  REQUEST_LIST_DEVICES = 0,
  REQUEST_PORT = 1,
};

// Enum listing the types of messages the server can send.
enum ServerMsgType {
  LIST_DEVICES_RESPONSE = 0,
  DEVICE_ADDED = 1,
  DEVICE_REMOVED = 2,
  REQUEST_PORT_RESPONSE = 3,
  INVALID_RESPONSE = UINT_MAX,
};

// Struct used at the start of every buffer sent between
// client and server. It is used as a header to denote the message
// type being sent, as well as the size of the subsequent payload.
// A typical usage pattern for the client would be as follows:
// - Poll on server fd.
// - Read buffer.
// - Call MidisProcessMsgHeader() to determine |type| and |payload_size|.
// - Call relevant MidisProcess* function based on |type|.
struct MidisMessageHeader {
  uint32_t type;
  uint32_t payload_size;
} __attribute__((packed));

// Struct used by server to send device info about MIDI H/W devices
// that have been connected / disconnected from the system.
// It is used with the following server messages types:
// - LIST_DEVICES_RESPONSE
// - DEVICE_ADDED
// - DEVICE_REMOVED
// For information on how to use this struct, please see the documentation
// for the following relevant library functions:
// - MidisProcessListDevices
// - MidisProcessDeviceAddedRemoved
struct MidisDeviceInfo {
  uint32_t card;
  uint32_t device_num;
  uint32_t num_subdevices;
  uint32_t flags;
  uint8_t name[kMidisStringSize];
  uint8_t manufacturer[kMidisStringSize];
} __attribute__((packed));

// Struct used by client to request an fd for a particular MidiPort (the term
// "port" and "subdevice" are used interchangeably here). To be used with
// client messages of type REQUEST_PORT, and with server messages of type
// REQUEST_PORT_RESPONSE. For usage, please see the documentation of the
// following library functions:
// - MidisRequestPort
// - MidisProcessRequestPortResponse
struct MidisRequestPort {
  uint32_t card;
  uint32_t device_num;
  uint32_t subdevice_num;
} __attribute__((packed));

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
