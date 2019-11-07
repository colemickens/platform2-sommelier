# Running Fingerprint Study

## Install

```bash
BOARD=nocturne
DUT=dut1

emerge-$BOARD fingerprint_study
cros deploy $DUT fingerprint_study
```

## Launch

```bash
# On device
# Disable biod upstart
stop biod
start fingerprint_study
# Navigate to http://localhost:9000
```
