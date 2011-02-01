// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_MODEM_H_
#define CROMO_MODEM_H_

namespace org {
namespace freedesktop {
namespace ModemManager {
class Modem_adaptor;
namespace Modem {
class Cdma_adaptor;
class Simple_adaptor;
}}}}

class Modem {
  public:
    typedef org::freedesktop::ModemManager::Modem_adaptor ModemAdaptor;
    typedef org::freedesktop::ModemManager::Modem::Cdma_adaptor CdmaAdaptor;
    typedef org::freedesktop::ModemManager::Modem::Simple_adaptor SimpleAdaptor;
    virtual ModemAdaptor *modem_adaptor() = 0;
    virtual CdmaAdaptor *cdma_adaptor() = 0;
    virtual SimpleAdaptor *simple_adaptor() = 0;
};

#endif /* !CROMO_MODEM_H_ */
