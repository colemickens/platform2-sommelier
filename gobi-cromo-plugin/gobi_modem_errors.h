// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Errors used for GobiModem and subclasses.  This file is included
// multiple times: once in the .h, and once in the .cc, hence no
// include guard

DEFINE_ERROR(Activation)
DEFINE_ERROR(Registration)
DEFINE_ERROR(Connect)
DEFINE_ERROR(Disconnect)
DEFINE_ERROR(Pin)
DEFINE_ERROR(FirmwareLoad)
DEFINE_ERROR(Sdk)
DEFINE_ERROR(Mode)
DEFINE_ERROR(OperationInitiated)
DEFINE_ERROR(InvalidArgument)
DEFINE_MM_ERROR(NoNetwork)
DEFINE_MM_ERROR(OperationNotAllowed)
