# Biod: Biometrics Daemon

## ABOUT

Biod (Biometrics Daemon) is a daemon for enrolling, authenticating and managing
biometrics data for faster unlocking. It manages all biometric sensors for all
users of the device.

## KEYWORDS

Enroll - the act of registering a specific piece of biometric data.

Enrollment - the biometric data that the users register along with its metadata.

Authenticate - the act of checking a new piece of biometric data against stored
enrollments.

Biometric - a piece of device/hardware/sensor for collecting a specific type of
biometric data. Each Biometric is in charge of storing the enrollments that it
generated.

## STORAGE

The enrollments are stored under the directory:
/home/root/[hash_of_user_id]/biod/[name_of_biometric]/
with the file name:
Enrollment[UUID].

UUID has the form of XXXXXXXX_XXXX_XXXX_XXXX_XXXXXXXXXXXX where X represents a
lowercase hex digit. UUID is a 128-bit random number generated with guid, so it
will highly unlikely repeat. '_' are used instead of '-' because this UUID is
used in biod D-bus object paths, which do not allow '-'.

Currently each enrollment file is stored in JSON format, and contains one
enrollment and its metadata.

## HARDWARE

### FakeBiometric

FakeBiometric is a fake biometric which simulates a real biometric sensor and
its interaction with biod. It is used to test biod.

### FpcBiometric

FpcBiometric is a real sensor and has its own algorithms library (called
libfp.so) for building, serializing, updating and matching enrollments. libfp.so
is provided by the hardware vendor and dynamically loaded at runtime.

## CHROME

Biod communicates with Chrome via D-bus messages. Chrome provides the graphical
interface for users to enroll new biometric data and manage their own
enrollments. Chrome is also responsible for the visual display during
authentication.

## LOGIN MANAGER

When a user logs in or biod starts, biod will ask login manager for a list of
currently logged-in users, and load from storage the enrollments of the newly
logged-in users.

When a user logs out, all users log out at the same time, biod receives a
signal from login manager and remove all enrollments in the memory.

## CRYPTOHOME

Because the enrollments are stored in per user stateful under /home/root/
[hash_of_user_id], they are naturally encrypted by cryptohome. Enrollments are
encrypted when the users log out and decrypted when the users log in. Cryptohome
provides a layer of security to biod.

## TESTING TOOLS

### fake_biometric_tool

fake_biometric_tool provides the interface for users to send D-bus messages as
FakeBiometric to fake the behavior of a biometric sensor. It can be used to test
the behavior of biod.

### biod_client_tool

biod_client_tool provides the interface to fake the behavior of a biometrics
client, for example, a lock screen or a biometric enrollment app. It can be used
to test biod and biometric sensors.
