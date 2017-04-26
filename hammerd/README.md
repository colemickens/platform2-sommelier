# hammerd: A daemon to update hammer

## Summary

hammer is the base of detachable, connected via USB over pogo pins. hammer runs
its own upgradable firmware (base EC, running EC codebase on STM32F072), is
attached to a trackpad (with its own upgradable FW), and is able to pair with
the detachable.

We need a userspace daemon, running on the AP, that does the following things
related to hammer:

- Waits for a base to be attached on detachable's pogo pins port, and then
  performs the following tasks as required.
  - Base EC FW update
  - Base trackpad FW update
  - Base pairing
  - Tell base to increment its rollback counter (if necessary)
  - Interaction with Chrome:
    - Shows notification during update (EC+trackpad)
    - Shows notification that a new base is connected (pairing)
