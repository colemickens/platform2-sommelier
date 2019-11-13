// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_ARC_SERVICE_H_
#define ARC_NETWORK_ARC_SERVICE_H_

#include <memory>
#include <string>

#include <base/memory/weak_ptr.h>
#include <shill/net/rtnl_handler.h>
#include <shill/net/rtnl_listener.h>

#include "arc/network/datapath.h"
#include "arc/network/guest_service.h"

namespace arc_networkd {

class ArcService : public GuestService {
 public:
  class Context : public Device::Context {
   public:
    Context();
    ~Context() = default;

    // Tracks the lifetime of the ARC++ container.
    void Start();
    void Stop();
    bool IsStarted() const;

    bool IsLinkUp() const override;
    // Returns true if the internal state changed.
    bool SetLinkUp(bool link_up);

    bool HasIPv6() const;
    // Returns false if |routing_tid| is invalid.
    bool SetHasIPv6(int routing_tid);
    // Resets the IPv6 attributes.
    void ClearIPv6();

    int RoutingTableID() const;
    // Returns the current value and increments the counter.
    int RoutingTableAttempts();

    // For ARCVM only.
    const std::string& TAP() const;
    void SetTAP(const std::string& tap);

   private:
    // Indicates the device was started.
    bool started_;
    // Indicates Android has brought up the interface.
    bool link_up_;
    // The routing table ID found for the interface.
    int routing_table_id_;
    // The number of times table ID lookup was attempted.
    int routing_table_attempts_;
    // For ARCVM, the name of the bound TAP device.
    std::string tap_;
  };

  class Impl {
   public:
    virtual ~Impl() = default;

    virtual GuestMessage::GuestType guest() const = 0;

    virtual bool OnStart(int32_t id) = 0;
    virtual void OnStop() = 0;
    virtual bool IsStarted() const = 0;
    virtual bool OnStartDevice(Device* device) = 0;
    virtual void OnStopDevice(Device* device) = 0;
    virtual void OnDefaultInterfaceChanged(const std::string& ifname) = 0;

   protected:
    Impl() = default;
  };

  // Encapsulates all ARC++ container-specific logic.
  class ContainerImpl : public Impl {
   public:
    ContainerImpl(DeviceManagerBase* dev_mgr,
                  Datapath* datapath,
                  GuestMessage::GuestType guest);
    ~ContainerImpl() = default;

    GuestMessage::GuestType guest() const override;

    bool OnStart(int32_t pid) override;
    void OnStop() override;
    bool IsStarted() const override;
    bool OnStartDevice(Device* device) override;
    void OnStopDevice(Device* device) override;
    void OnDefaultInterfaceChanged(const std::string& ifname) override;

   private:
    // Handles RT netlink messages in the container net namespace and if it
    // determines the link status has changed, toggles the device services
    // accordingly.
    void LinkMsgHandler(const shill::RTNLMessage& msg);

    void SetupIPv6(Device* device);
    void TeardownIPv6(Device* device);

    pid_t pid_;
    DeviceManagerBase* dev_mgr_;
    Datapath* datapath_;
    GuestMessage::GuestType guest_;

    std::unique_ptr<shill::RTNLHandler> rtnl_handler_;
    std::unique_ptr<shill::RTNLListener> link_listener_;

    base::WeakPtrFactory<ContainerImpl> weak_factory_{this};
    DISALLOW_COPY_AND_ASSIGN(ContainerImpl);
  };

  // Encapsulates all ARC VM-specific logic.
  class VmImpl : public Impl {
   public:
    VmImpl(DeviceManagerBase* dev_mgr, Datapath* datapath);
    ~VmImpl() = default;

    GuestMessage::GuestType guest() const override;

    bool OnStart(int32_t cid) override;
    void OnStop() override;
    bool IsStarted() const override;
    bool OnStartDevice(Device* device) override;
    void OnStopDevice(Device* device) override;
    void OnDefaultInterfaceChanged(const std::string& ifname) override;

   private:
    int32_t cid_;
    DeviceManagerBase* dev_mgr_;
    Datapath* datapath_;

    DISALLOW_COPY_AND_ASSIGN(VmImpl);
  };

  // |dev_mgr| and |datapath| cannot be null.
  // |is_legacy| is temporary and intended to be used for testing only.
  ArcService(DeviceManagerBase* dev_mgr,
             Datapath* datapath,
             bool* is_legacy = nullptr);
  ~ArcService() = default;

  void OnStart() override;
  void OnStop() override;

  void OnDeviceAdded(Device* device) override;
  void OnDeviceRemoved(Device* device) override;
  void OnDefaultInterfaceChanged(const std::string& ifname) override;

 private:
  void StartDevice(Device* device);
  void StopDevice(Device* device);

  Datapath* datapath_;
  std::unique_ptr<Impl> impl_;

  base::WeakPtrFactory<ArcService> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ArcService);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_ARC_SERVICE_H_
