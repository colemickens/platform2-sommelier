// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DLCSERVICE_SYSTEM_STATE_H_
#define DLCSERVICE_SYSTEM_STATE_H_

#include <memory>

#include <base/files/file_path.h>
#include <base/macros.h>
#include <dlcservice/proto_bindings/dlcservice.pb.h>
#include <imageloader/dbus-proxies.h>
#include <update_engine/proto_bindings/update_engine.pb.h>
#include <update_engine/dbus-proxies.h>

#include "dlcservice/boot/boot_slot.h"
#include "dlcservice/dlc_manager.h"

namespace dlcservice {

class SystemState {
 public:
  virtual ~SystemState() = default;

  // Creates a singleton |SystemState| that can be accessible globally by
  // calling |SystemState::Get()|. Subsequent calls will fail and is an error.
  // But if |for_test| is true, repeated calls can be made during testing to
  // reset |SystemState|. Note: Should only be used during tests.
  static void Initialize(
      std::unique_ptr<org::chromium::ImageLoaderInterfaceProxyInterface>
          image_loader_proxy,
      std::unique_ptr<org::chromium::UpdateEngineInterfaceProxyInterface>
          update_engine_proxy,
      std::unique_ptr<BootSlot> boot_slot,
      const base::FilePath& manifest_dir,
      const base::FilePath& preloaded_content_dir,
      const base::FilePath& content_dir,
      const base::FilePath& metadata_dir,
      bool for_test = false);

  // Gets the pointer to the current |SystemState|.
  static SystemState* Get();

  // Getters for states that |SystemState| holds.
  org::chromium::ImageLoaderInterfaceProxyInterface* image_loader() const;
  org::chromium::UpdateEngineInterfaceProxyInterface* update_engine() const;
  const BootSlot& boot_slot() const;
  const base::FilePath& manifest_dir() const;
  const base::FilePath& preloaded_content_dir() const;
  const base::FilePath& content_dir() const;
  const base::FilePath& metadata_dir() const;

 protected:
  SystemState(
      std::unique_ptr<org::chromium::ImageLoaderInterfaceProxyInterface>
          image_loader_proxy,
      std::unique_ptr<org::chromium::UpdateEngineInterfaceProxyInterface>
          update_engine_proxy,
      std::unique_ptr<BootSlot> boot_slot,
      const base::FilePath& manifest_dir,
      const base::FilePath& preloaded_content_dir,
      const base::FilePath& content_dir,
      const base::FilePath& metadata_dir);

 private:
  std::unique_ptr<org::chromium::ImageLoaderInterfaceProxyInterface>
      image_loader_proxy_;
  std::unique_ptr<org::chromium::UpdateEngineInterfaceProxyInterface>
      update_engine_proxy_;
  std::unique_ptr<BootSlot> boot_slot_;
  base::FilePath manifest_dir_;
  base::FilePath preloaded_content_dir_;
  base::FilePath content_dir_;
  base::FilePath metadata_dir_;

  static std::unique_ptr<SystemState> g_instance_;
  DISALLOW_COPY_AND_ASSIGN(SystemState);
};

}  // namespace dlcservice

#endif  // DLCSERVICE_SYSTEM_STATE_H_
