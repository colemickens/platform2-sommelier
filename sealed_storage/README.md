#SealedStorage

## About

SealedStorage library allows to seal data so that it can only be unsealed on a
local machine (bound to a specific secure module) in a specific state
(typically, verified boot mode). Unlike unsealing, sealing can be done in any
state.

SealedStorage only supports Cr50 and uses trunks and tpm\_manager to work with
the secure module.

The library interface is defined in sealed_storage.h.
An example of using the interface is provided in sealed_storage_tool.cc.

## Underlying implementation

The data is sealed using a multi-staged process:
1. A TPM-bound sealing ECC key is generated. The key is generated as a primary
key in endorsement hierarchy, so using the same key template always results in
the same key. The key has an attached policy that allows performing authorized
operations only when PCRs are in the desired state. The policy is part of the
key template, so anyone who wants to generate the same key would have to
have it with that auth policy.
2. The sealing key is used in ECDH protocol to generate an ephemeral keypair
(ECDH_KeyGen command). ECDH_KeyGen doesn't require authorization and can be
performed in any state, regardless of the policy set for the sealing key.
The resulting private part of the generated keypair (Z point) is used as a
seed for creating an AES-256 key. The public part (pub point) becomes a part
of the sealed blob.
3. A random IV is generated.
4. The data to seal is encrypted using AES-256-CBC with the key and IV obtained
on the two previous steps.
5. The resulting blob contains {pub point, IV, encrypted data}. This
blob can be stored by the caller in any non-volatile storage.

To unseal, the steps are reversed:
1. The blob, containing {pub point, IV, encrypted data}, is parsed.
2. The same primary key in endorsement hierarchy is generated. The policy for
the key is passed by the caller and must match the policy used when sealing
data to get the same sealing key.
3. The sealing key is used in ECDH protocol to restore the private part of the
ephemeral key (Z point) given its public part (pub point, retreieved from the
blob). ECDH_ZGen command used for that required authorization and would only
succeed if the policy attached to the sealing key is satisfied. And, as it was
constructed when sealing, the policy is satisifed only if PCRs are in the right
state. Thus, for a wrong state, and attempt to unseal would fail at this step.
4. After restoring Z point, it is used as a seed for generating AES-256 key,
exactly as it was done when sealing.
5. The encrypted data is decrypted using the resulting AES key and IV obtained
from the data blob.
