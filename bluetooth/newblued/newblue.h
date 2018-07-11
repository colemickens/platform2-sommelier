// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_NEWBLUE_H_
#define BLUETOOTH_NEWBLUED_NEWBLUE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <base/single_thread_task_runner.h>

#include "bluetooth/newblued/libnewblue.h"

namespace bluetooth {

enum class EirType : uint8_t {
  FLAGS = 0x01,
  UUID16_INCOMPLETE = 0x02,
  UUID16_COMPLETE = 0x03,
  UUID32_INCOMPLETE = 0x04,
  UUID32_COMPLETE = 0x05,
  UUID128_INCOMPLETE = 0x06,
  UUID128_COMPLETE = 0x07,
  NAME_SHORT = 0x08,
  NAME_COMPLETE = 0x09,
  TX_POWER = 0x0a,
  CLASS_OF_DEV = 0x0d,
  SSP_HASH = 0x0e,
  SSP_RANDOMIZER = 0x0f,
  DEVICE_ID = 0x10,
  SOLICIT16 = 0x14,
  SOLICIT128 = 0x15,
  SVC_DATA16 = 0x16,
  PUB_TRGT_ADDR = 0x17,
  RND_TRGT_ADDR = 0x18,
  GAP_APPEARANCE = 0x19,
  SOLICIT32 = 0x1f,
  SVC_DATA32 = 0x20,
  SVC_DATA128 = 0x21,
  MANUFACTURER_DATA = 0xff,
};

// Structure representing a discovered device.
struct Device {
  // MAC address (in format XX:XX:XX:XX:XX:XX).
  std::string address;
  // Whether the MAC address is a random address.
  bool is_random_addr;
  // RSSI of last received inquiry response.
  int8_t rssi;

  // Data parsed from Extended Inquiry Response (EIR).
  std::string name;
  uint32_t eir_class;
  uint16_t appearance;
};

// A higher-level API wrapper of the low-level libnewblue C interface.
// This class abstracts away the C interface details of libnewblue and provides
// event handling model that is compatible with libchrome's main loop.
class Newblue {
 public:
  using DeviceDiscoveredCallback = base::Callback<void(const Device&)>;

  explicit Newblue(std::unique_ptr<LibNewblue> libnewblue);
  virtual ~Newblue() = default;

  base::WeakPtr<Newblue> GetWeakPtr();

  // Initializes the LE stack (blocking call).
  // Returns true if initialization succeeds, false otherwise.
  virtual bool Init();

  // Listens to reset complete event from the chip. This is useful to detect
  // when NewBlue is ready to bring up the stack.
  // Returns true on success and false otherwise.
  virtual bool ListenReadyForUp(base::Closure callback);

  // Brings up the NewBlue stack. This should be called when the adapter has
  // just been turned on, detected when there is reset complete event from the
  // chip. ListenReadyForUp() should be used to detect this event.
  // Returns true if success, false otherwise.
  virtual bool BringUp();

  // Starts LE scanning, |callback| will be called every time inquiry response
  // is received.
  virtual bool StartDiscovery(DeviceDiscoveredCallback callback);
  // Stops LE scanning.
  virtual bool StopDiscovery();

  // Updates EIR data of |device|.
  static void UpdateEir(Device* device, const std::vector<uint8_t>& eir);

 private:
  // Posts task to the thread which created this Newblue object.
  // libnewblue callbacks should always post task using this method rather than
  // doing any processing in the callback's thread.
  bool PostTask(const tracked_objects::Location& from_here,
                const base::Closure& task);

  // Called by NewBlue when it's ready to bring up the stack. This is called on
  // one of NewBlue's threads, so we shouldn't do anything on this thread other
  // than posting the task to our mainloop thread.
  static void OnStackReadyForUpThunk(void* data);
  // Triggers the callback registered via ListenReadyForUp().
  void OnStackReadyForUp();

  static void DiscoveryCallbackThunk(void* data,
                                     const struct bt_addr* addr,
                                     int8_t rssi,
                                     uint8_t reply_type,
                                     const void* eir,
                                     uint8_t eir_len);
  // Called when inquiry response is received as a result of StartDiscovery().
  void DiscoveryCallback(const std::string& address,
                         uint8_t address_type,
                         int8_t rssi,
                         uint8_t reply_type,
                         const std::vector<uint8_t>& eir);

  std::unique_ptr<LibNewblue> libnewblue_;

  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;

  base::Closure ready_for_up_callback_;

  DeviceDiscoveredCallback device_discovered_callback_;
  uniq_t discovery_handle_ = 0;
  std::map<std::string, std::unique_ptr<Device>> discovered_devices_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<Newblue> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Newblue);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_NEWBLUE_H_
