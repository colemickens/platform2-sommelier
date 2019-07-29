// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_EC_EVENT_SERVICE_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_EC_EVENT_SERVICE_H_

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/message_loop/message_loop.h>
#include <base/sequence_checker_impl.h>
#include <base/threading/simple_thread.h>

#include "diagnostics/wilco_dtc_supportd/ec_constants.h"
#include "mojo/wilco_dtc_supportd.mojom.h"

namespace diagnostics {

namespace internal {
class EcEventMonitoringThreadDelegate;
}  // namespace internal

// Subscribes on EC events and redirects EC events to wilco_dtc.
class WilcoDtcSupportdEcEventService final {
 public:
  // A packet of data sent by the EC when it notices certain events have
  // occured, such as the battery, AC adapter, or USB-C state changing.
  // The format of this packet is a variable length sequence of 16-bit words.
  // Word 0 is the |size| word, representing the number of following
  // words in the struct. Word 1 is the |type| word. The following |size|-1
  // words are the |payload|. Depending on the value of |type|, the |payload|
  // is interpreted in different ways. There are other possible values of |type|
  // and other interpretations of |payload| than those listed here. There will
  // be, at most, 6 words in the |payload|. See section 2.3 "ACPI EC Event
  // notification" of the Wilco EC specification at go/wilco-ec-spec for more
  // information.
  struct alignas(2) EcEvent {
   public:
    // The |type| member will be one of these.
    enum Type : uint16_t {
      // Interpret |payload| as SystemNotifyPayload.
      SYSTEM_NOTIFY = 0x0012,
    };

    // Sub-types applicable for SystemNotifyPayload.
    enum SystemNotifySubType : uint16_t {
      AC_ADAPTER = 0x0000,
      BATTERY = 0x0003,
      USB_C = 0x0008,
    };

    // Flags used within |SystemNotifyPayload|.
    struct alignas(2) AcAdapterFlags {
      enum Cause : uint16_t {
        // Barrel charger is incompatible and performance will be restricted.
        NON_WILCO_CHARGER = 1 << 0,
      };
      uint16_t reserved0;
      Cause cause;
      uint16_t reserved2;
      uint16_t reserved3;
      uint16_t reserved4;
    };

    // Flags used within |SystemNotifyPayload|.
    struct alignas(2) BatteryFlags {
      enum Cause : uint16_t {
        // An incompatible battery is connected and battery will not charge.
        BATTERY_AUTH = 1 << 0,
      };
      uint16_t reserved0;
      Cause cause;
      uint16_t reserved2;
      uint16_t reserved3;
      uint16_t reserved4;
    };

    // Flags used within |SystemNotifyPayload|
    struct alignas(2) UsbCFlags {
      // "Billboard" is the name taken directly from the EC spec. It's a weird
      // name, but these can represent a variety of miscellaneous events.
      enum Billboard : uint16_t {
        // HDMI and USB Type-C ports on the dock cannot be used for
        // displays at the same time. Only the first one connected will work.
        HDMI_USBC_CONFLICT = 1 << 9,
      };
      enum Dock : uint16_t {
        // Thunderbolt is not supported on Chromebooks, so the dock
        // will fall back on using USB Type-C.
        THUNDERBOLT_UNSUPPORTED_USING_USBC = 1 << 8,
        // Attached dock is incompatible.
        INCOMPATIBLE_DOCK = 1 << 12,
        // Attached dock has overheated.
        OVERTEMP_ERROR = 1 << 15,
      };
      Billboard billboard;
      uint16_t reserved1;
      Dock dock;
    };

    // Interpretation of |payload| applicable when |type|==Type::SYSTEM_NOTIFY.
    struct alignas(2) SystemNotifyPayload {
      SystemNotifySubType sub_type;
      // Depending on |sub_type| we interpret the following data in different
      // ways. Note that these flags aren't all the same size.
      union SystemNotifyFlags {
        AcAdapterFlags ac_adapter;
        BatteryFlags battery;
        UsbCFlags usb_c;
      } flags;
    };

    EcEvent() = default;

    EcEvent(uint16_t num_words_in_payload, Type type, const uint16_t payload[6])
        : size(num_words_in_payload + 1), type(type), payload{} {
      memcpy(&this->payload, payload,
             std::min(sizeof(this->payload),
                      num_words_in_payload * sizeof(payload[0])));
    }

    bool operator==(const EcEvent& other) const {
      return memcmp(this, &other, sizeof(*this)) == 0;
    }

    // Translate the |size| member into how many bytes of |payload| are used.
    size_t PayloadSizeInBytes() const;

    // |size| is the number of following 16-bit words in the event.
    // Default is 1 to account for |type| word and empty |payload|.
    uint16_t size = 1;
    Type type = static_cast<Type>(0);
    // Depending on |type| we interpret the following data in different ways.
    union {
      SystemNotifyPayload system_notify;
    } payload = {};
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when event from EC was received.
    //
    // Calls wilco_dtc |HandleEcNotification| gRPC function with |payload| in
    // request.
    virtual void SendGrpcEcEventToWilcoDtc(const EcEvent& ec_event) = 0;
    // Forwards Mojo event to browser's HandleEvent Mojo function in order
    // to display relevant system notifications.
    virtual void HandleMojoEvent(
        const chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdEvent&
            mojo_event) = 0;
  };

  explicit WilcoDtcSupportdEcEventService(Delegate* delegate);
  ~WilcoDtcSupportdEcEventService();

  // Starts service.
  bool Start();

  // Shutdowns service.
  void Shutdown(base::Closure on_shutdown_callback);

  // Overrides the file system root directory for file operations in tests.
  void set_root_dir_for_testing(const base::FilePath& root_dir) {
    root_dir_ = root_dir;
  }

  // Overrides the |event_fd_events_| in tests.
  void set_event_fd_events_for_testing(int16_t events) {
    event_fd_events_ = events;
  }

 private:
  // Signal via writing to the |shutdown_fd_| that the monitoring thread should
  // shut down. Once the monitoring thread handles this event and gets ready
  // for shutting down, it will reply by scheduling an invocation of
  // OnShutdown() on the foreground thread.
  void ShutdownMonitoringThread();

  // This is called on the |message_loop_->task_runner()| when new EC event
  // was received by background monitoring thread.
  void OnEventAvailable(const EcEvent& ec_event);

  // This is called on the |message_loop_->task_runner()| when the background
  // monitoring thread is shutting down.
  void OnShutdown();

  base::MessageLoop* const message_loop_;

  // This callback will be invoked after current service shutdown.
  base::Closure on_shutdown_callback_;

  // Unowned. The delegate should outlive this instance.
  Delegate* const delegate_;

  // The file system root directory. Can be overridden in tests.
  base::FilePath root_dir_{"/"};

  // EC event |event_fd_| and |event_fd_events_| are using for |poll()|
  // function in |monitoring_thread_|. Both can be overridden in tests.
  base::ScopedFD event_fd_;
  int16_t event_fd_events_ = kEcEventFilePollEvents;

  // Shutdown event fd. It is used to stop |poll()| immediately and shutdown
  // |monitoring_thread_|.
  base::ScopedFD shutdown_fd_;

  // The delegate which will be executed on the |monitoring_thread_|.
  std::unique_ptr<internal::EcEventMonitoringThreadDelegate>
      monitoring_thread_delegate_;
  // The backgroung thread monitoring the EC sysfs file for upcoming events.
  std::unique_ptr<base::SimpleThread> monitoring_thread_;

  base::SequenceCheckerImpl sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(WilcoDtcSupportdEcEventService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_EC_EVENT_SERVICE_H_
