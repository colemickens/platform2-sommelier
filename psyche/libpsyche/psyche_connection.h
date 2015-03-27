// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_LIBPSYCHE_PSYCHE_CONNECTION_H_
#define PSYCHE_LIBPSYCHE_PSYCHE_CONNECTION_H_

#include <memory>
#include <string>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/scoped_ptr.h>

#define PSYCHE_EXPORT __attribute__((visibility("default")))

namespace protobinder {
class BinderHost;
class BinderProxy;
}  // namespace protobinder

namespace psyche {

// Interface encapsulating a client's communication to psyched.
class PSYCHE_EXPORT PsycheConnectionInterface {
 public:
  // TODO(derat): Switch this to std::unique_ptr once base::Bind() supports it.
  using GetServiceCallback =
      base::Callback<void(scoped_ptr<protobinder::BinderProxy>)>;

  // Registers |service|, identified by |service_name|, with psyched. Ownership
  // of |service| remains with the caller. Blocks until registration is
  // complete, returning true on success.
  virtual bool RegisterService(const std::string& service_name,
                               protobinder::BinderHost* service) = 0;

  // Fetches the service identified by |service_name| from psyched.
  // |callback| will be invoked when the service is available.
  virtual void GetService(
      const std::string& service_name,
      const GetServiceCallback& callback) = 0;

 protected:
  virtual ~PsycheConnectionInterface() = default;
};

// Real implementation of PsycheConnectionInterface.
class PSYCHE_EXPORT PsycheConnection : public PsycheConnectionInterface {
 public:
  PsycheConnection();
  ~PsycheConnection() override;

  // Initializes the connection, returning true on success.
  bool Init();

  // PsycheConnectionInterface:
  bool RegisterService(const std::string& service_name,
                       protobinder::BinderHost* service) override;
  void GetService(const std::string& service_name,
                  const GetServiceCallback& callback) override;

 private:
  // Implements IPsycheClientHostInterface. Defined in .cc file so this header
  // doesn't need to include generated proto files.
  class Impl;
  std::unique_ptr<Impl> impl_;

  DISALLOW_COPY_AND_ASSIGN(PsycheConnection);
};

}  // namespace psyche

#endif  // PSYCHE_LIBPSYCHE_PSYCHE_CONNECTION_H_
