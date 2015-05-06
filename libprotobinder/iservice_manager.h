// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_ISERVICE_MANAGER_H_
#define LIBPROTOBINDER_ISERVICE_MANAGER_H_

#include "binder_export.h"  // NOLINT(build/include)
#include "iinterface.h"  // NOLINT(build/include)

namespace protobinder {

class BinderHost;
class BinderProxy;
class Status;

class BINDER_EXPORT IServiceManager : public IInterface {
 public:
  DECLARE_META_INTERFACE(ServiceManager)

  virtual Status AddService(const char* name, BinderHost* binder) = 0;
  virtual BinderProxy* GetService(const char* name) = 0;

  enum {
    GET_SERVICE_TRANSACTION = 1,
    CHECK_SERVICE_TRANSACTION,
    ADD_SERVICE_TRANSACTION,
    LIST_SERVICES_TRANSACTION,
  };
};

BINDER_EXPORT IServiceManager* GetServiceManager();

}  // namespace protobinder

#endif  // LIBPROTOBINDER_ISERVICE_MANAGER_H_
