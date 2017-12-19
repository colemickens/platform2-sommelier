// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MODEMFWD_COMPONENT_H_
#define MODEMFWD_COMPONENT_H_

#include <memory>

#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <dbus/bus.h>

namespace dbus {
class ObjectProxy;
}  // namespace dbus

namespace modemfwd {

class Component {
 public:
  static std::unique_ptr<Component> Load(scoped_refptr<dbus::Bus> bus);
  ~Component();

  base::FilePath GetPath() const;

 private:
  Component(scoped_refptr<dbus::Bus> bus,
            dbus::ObjectProxy* proxy,
            const base::FilePath& base_component_path);

  void Unload();

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* proxy_;  // weak, owned by |bus_|

  base::FilePath base_component_path_;

  DISALLOW_COPY_AND_ASSIGN(Component);
};

}  // namespace modemfwd

#endif  // MODEMFWD_COMPONENT_H_
