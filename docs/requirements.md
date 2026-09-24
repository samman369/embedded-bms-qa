# Software Requirements Specification — BMS Firmware v1.0

Scope: firmware for a 4-cell (4S) lithium-ion Battery Management System.
The firmware runs a 100 ms measurement cycle, protects the pack by opening the
charge / discharge MOSFETs, and exposes a CRC-protected UART command protocol.

Every requirement below is verified by at least one automated unit test,
automated functional test, or manual test case. See the traceability matrix
in the test dashboard (`reports/dashboard.html`).

## Protection & state machine

| ID | Requirement |
|----|-------------|
| REQ-BMS-001 | On power-up the BMS shall be in state `INIT` with both FETs OFF until the first measurement cycle completes; with nominal inputs it shall then enter `IDLE` with both FETs ON. |
| REQ-BMS-010 | If any valid cell voltage is ≥ 4200 mV for 3 consecutive cycles (300 ms), the BMS shall set the OV fault. |
| REQ-BMS-011 | An active OV condition shall release only when every valid cell is ≤ 4100 mV (100 mV hysteresis). |
| REQ-BMS-020 | If any valid cell voltage is ≤ 3000 mV for 3 consecutive cycles, the BMS shall set the UV fault. |
| REQ-BMS-021 | An active UV condition shall release only when every valid cell is ≥ 3100 mV. |
| REQ-BMS-030 | If pack temperature is ≥ 60.0 °C for 3 consecutive cycles, the BMS shall set the OT fault. |
| REQ-BMS-031 | An active OT condition shall release only when temperature is ≤ 55.0 °C. |
| REQ-BMS-040 | If discharge current is ≥ 30 A (I ≤ −30000 mA) or charge current is ≥ 10 A (I ≥ +10000 mA) for 3 consecutive cycles, the BMS shall set the OC fault. The condition releases as soon as current is inside the limits. |
| REQ-BMS-045 | A cell reading below 500 mV or above 5000 mV is implausible: the BMS shall set the SENSOR fault within 1 cycle and exclude that reading from min/max voltage evaluation. |
| REQ-BMS-050 | While in `FAULT`, both charge and discharge FETs shall be OFF, switched in the same cycle the fault is set. |
| REQ-BMS-051 | Faults shall be latched. The BMS shall leave `FAULT` only on a `CLEAR_FAULTS` command, which shall be rejected while any fault condition is still active. |
| REQ-BMS-060 | Without faults, the state shall be `CHARGING` if I ≥ +100 mA, `DISCHARGING` if I ≤ −100 mA, otherwise `IDLE`. |

## Communication protocol (UART)

| ID | Requirement |
|----|-------------|
| REQ-COM-001 | Frames shall have the form `$<payload>*<CC>\n`, where `CC` is the CRC-8 (poly 0x07, init 0x00, CRC-8/SMBUS) of the payload as two hex digits. |
| REQ-COM-002 | A frame with a wrong CRC shall be answered with `ERR,CRC` and not executed. |
| REQ-COM-003 | A malformed frame (missing `$` or `*`, bad hex, empty payload, illegal characters) shall be answered with `ERR,FORMAT`. |
| REQ-COM-004 | Frames longer than 64 characters shall be answered with `ERR,LENGTH`; the receiver shall then process the next frame normally. |
| REQ-COM-005 | An unknown command shall be answered with `ERR,UNKNOWN_CMD`. |
| REQ-COM-010 | `PING` shall return `OK,PONG`. |
| REQ-COM-011 | `GET_VERSION` shall return `OK,VERSION=<x.y.z>`. |
| REQ-COM-012 | `GET_STATUS` shall return `OK,STATE=<s>,FAULTS=0x<hh>,VMIN=<mV>,VMAX=<mV>,TEMP=<0.1°C>,CUR=<mA>`. |
| REQ-COM-013 | `GET_CELL,<n>` shall return `OK,CELL<n>=<mV>` for n = 0..3, and `ERR,ARG` for a missing, non-numeric or out-of-range index. |
| REQ-COM-014 | `CLEAR_FAULTS` shall return `OK,CLEARED` on success or `ERR,FAULT_ACTIVE` when rejected. |

## Drivers

| ID | Requirement |
|----|-------------|
| REQ-DRV-001 | The UART RX ring buffer shall be FIFO, hold `size − 1` bytes, and on overflow drop the new byte and increment an overflow counter. |

## Fault bit map

| Bit | Mask | Fault |
|-----|------|-------|
| 0 | 0x01 | OV — over-voltage |
| 1 | 0x02 | UV — under-voltage |
| 2 | 0x04 | OT — over-temperature |
| 3 | 0x08 | OC — over-current |
| 4 | 0x10 | SENSOR — implausible measurement |
