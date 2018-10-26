# Biod: Biometrics Daemon

## ABOUT

Biod (Biometrics Daemon) is a daemon for enrolling, authenticating and managing
biometrics data for faster unlocking. It manages all biometric sensors for all
users of the device.

## KEYWORDS

Record - a specific piece of biometric data that a user registers along with its
metadata.

Enroll - the act of registering a record.

EnrollSession - the session during which enrolling of a record happens.

Authenticate - the act of checking a new piece of biometric data against stored
records.

AuthSession - the session during which authenticating a new piece of biometric
data against stored records happens.

BiometricsManager - manager for a piece of device/hardware/sensor for collecting
a specific type of biometric data. Each BiometricsManager is in charge of
storing the records that it enrolled.

## STORAGE

The records are stored under the directory:
/home/root/[hash_of_user_id]/biod/[name_of_biometrics_manager]/
with the file name:
Record[UUID].

UUID has the form of XXXXXXXX_XXXX_XXXX_XXXX_XXXXXXXXXXXX where X represents a
lowercase hex digit. UUID is a 128-bit random number generated with guid, so it
will highly unlikely repeat. '_' are used instead of '-' because this UUID is
used in biod D-bus object paths, which do not allow '-'.

Each record file is stored in JSON format, and contains one record and its
record id and label.

## HARDWARE

### CrosFpBiometric

TODO

## CHROME

Biod communicates with Chrome via D-bus messages. Chrome provides the graphical
interface for users to enroll new biometric data and manage their own
records. Chrome is also responsible for the visual display during
authentication.

## LOGIN MANAGER

When a user logs in or biod starts, biod will ask login manager for a list of
currently logged-in users, and load from storage the records of the newly
logged-in users.

When a user logs out, all users log out at the same time, biod receives a
signal from login manager and remove all records in the memory.

## CRYPTOHOME

Because the records are stored in per user stateful under /home/root/
[hash_of_user_id], they are naturally encrypted by cryptohome. Records are
encrypted when the users log out and decrypted when the users log in. Cryptohome
provides a layer of security to biod.

## TESTING TOOLS

### biod_client_tool

biod_client_tool provides the interface to fake the behavior of a biometrics
client, for example, a lock screen or a biometric enrollment app. It can be used
to test biod and biometric sensors.
