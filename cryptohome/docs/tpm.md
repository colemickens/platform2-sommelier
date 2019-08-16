# TPM

[TOC]

## Overview

Cryptohomed is responsible for initialization of the TPM once it has been
enabled and activated. Ownership creates a Storage Root Key (SRK) on the TPM,
which is necessary to seal user keys (the system-wide cryptohome key and keys
used by opencryptoki+tpm are ultimately sealed by the SRK; see
http://trousers.sourceforge.net/pkcs11.html for more details on how opencryptoki
uses the SRK). The ownership process is in the tpm_init library, separate from
cryptohome, but the basic process is as follows:

1.  If `/sys/class/{misc|tpm}/tpm0/device/owned` is 0 and
    `/sys/class/{misc|tpm}/tpm0/device/enabled` is 1 then the initialize thread
    will attempt to initialize the TPM.
2.  An Endorsement Key (`EK`) will be created if it doesn't exist (some vendors
    do not create one at the factory, as it is not required by the spec).
3.  Tspi_TPM_TakeOwnership will be called with the Trousers default well-known
    ownership password, and the Chromium OS well-known `SRK` password. This may
    take between 10s and 150s.
4.  If successful, the `SRK`'s password is reset to a NULL string, and its use
    is unrestricted (in other words, code using the SRK does not need to know
    the owner password).
5.  Finally, the owner password is changed to a randomly-generated string that
    is available to the user for the duration of that boot.

Cryptohome uses the TPM merely for secure key storage to help protect the user
from data loss should their device be lost or compromised. Keys sealed by the
TPM can only be used on the TPM itself, meaning that offline or brute-force
attacks are difficult.
