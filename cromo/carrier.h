// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_CARRIER_H_
#define CROMO_CARRIER_H_

#include "base/basictypes.h"

#include <dbus-c++/dbus.h>
#include <map>

#include "utilities.h"

class Carrier {
 public:
  enum ActivationMethod {
    kOmadm,
    kOtasp,
    kNone
  };

  Carrier(const char* name,
          const char* firmware_directory,
          unsigned long carrier_id,
          int   carrier_type,
          ActivationMethod activation_method,
          const char* activation_code)
      :  name_(name),
         firmware_directory_(firmware_directory),
         carrier_id_(carrier_id),
         carrier_type_(carrier_type),
         activation_method_(activation_method),
         activation_code_(activation_code) {
  }
  virtual ~Carrier(){};

  // Called after Modem.Simple.Status has filled the property map, but
  // before the property map has been returned.
  virtual void ModifyModemStatusReturn(
      utilities::DBusPropertyMap *properties) const {}

  const char *name() const {return name_;}
  const char *firmware_directory() const {return firmware_directory_;}
  unsigned long carrier_id() const {return carrier_id_;}
  int carrier_type() const {return carrier_type_;}
  ActivationMethod activation_method() const {return activation_method_;}
  const char *activation_code() const {return activation_code_;}

 protected:
  const char* name_;
  const char* firmware_directory_;
  unsigned long carrier_id_;
  int carrier_type_;
  ActivationMethod activation_method_;
  const char* activation_code_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Carrier);
};

class CromoServer;
void AddBaselineCarriers(CromoServer *);

#endif  /* CROMO_CARRIER_H_ */
