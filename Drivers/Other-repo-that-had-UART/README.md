# UART Command + Feedback + Debug Porting Plan

## Goal

Implement **functional bidirectional UART control** in this repository (`/workspace/hoverboard-UGV-firmware`) using the reference files in `Drivers/Other-repo-that-had-UART` as the template:

- Binary command input on **USART2** (left sensor cable)
- Binary feedback output on **USART2**
- Optional CSV/ASCII debug stream on **USART3**

The intent is to keep compatibility with the proven host tooling pattern from the reference (`Scripts/UART-control-test.py`) while fitting the simpler architecture of this current firmware.

---

## 1) Repository Inspection Summary

## Current project architecture (`Inc/`, `Src/`)

The active firmware in this repo is the classic split:

- **Hardware init and peripheral wiring**: `Src/setup.c`, `Inc/setup.h`
- **Application loop and input/mixer logic**: `Src/main.c`
- **Motor ISR and low-level actuation**: `Src/bldc.c`
- **Input drivers (PPM/Nunchuck)**: `Src/control.c`
- **Debug output helper**: `Src/comms.c`
- **Build-time feature gating**: `Inc/config.h`
- **IRQ vectors**: `Src/stm32f1xx_it.c`

Important finding: this repo already has *partial UART control* (`CONTROL_SERIAL_USART2` path in `Src/main.c`) and debug print infrastructure (`Src/comms.c`), but it does **not** yet implement the robust frame parser + continuous structured feedback architecture seen in the reference repo.

## Reference architecture in `Drivers/Other-repo-that-had-UART`

The reference folder shows a more complete communication stack:

- `Inc/util.h` / `Src/util.c`:
  - command/feedback-related structures
  - RX DMA ring-buffer processing helpers (`usart*_rx_check` style)
  - protocol processing entry points
- `Inc/comms.h` / `Src/comms.c`:
  - optional debug command protocol / runtime parameter shell
- `Inc/config.h`:
  - rich feature flags (`CONTROL_SERIAL_*`, `FEEDBACK_SERIAL_*`, `DEBUG_SERIAL_*`)
- `Scripts/UART-control-test.py`:
  - explicit binary command + binary feedback + debug CSV host model

## Gap analysis (what is missing in the current repo)

1. **No formal `SerialFeedback` frame in active code path** (feedback to host is absent in `Src/main.c`).
2. **No DMA ring-buffer parser/state machine** for robust desynchronization recovery (current code relies on fixed-size DMA struct receive).
3. **No unified communication owner module** for command RX + feedback TX + debug TX scheduling.
4. **No explicit feedback timing contract** (e.g., 100 Hz feedback, 20 Hz debug).
5. **No host-side test script integrated at root/docs level** for repeatable verification.

---

## 2) Target UART Architecture for This Repo

We will keep the current project’s style (minimal files, macro-gated features) but add the proven concepts.

### 2.1 Protocol contract (recommended)

Use one binary protocol family with explicit frame headers and XOR checksums.

- **Start frame**: keep one constant (`START_FRAME`), migrate to `0xABCD` if matching host script is desired.
- **Command frame** (host → board):
  - `start`, `steer`, `speed`, `checksum`
- **Feedback frame** (board → host):
  - `start`, `cmd1`, `cmd2`, `speedR_meas`, `speedL_meas`, `batVoltage`, `boardTemp`, `cmdLed`, `checksum`

This aligns with the existing script in `Drivers/Other-repo-that-had-UART/Scripts/UART-control-test.py`.

### 2.2 Runtime data flow

1. USART2 RX DMA runs continuously into a byte buffer.
2. Main loop calls `uart_control_rx_check()` each iteration.
3. Parser extracts valid `SerialCommand` frames, updates `cmd1/cmd2`, timestamps last valid packet.
4. Main loop continues normal mixer/safety logic.
5. At fixed interval (e.g., every 10 ms), firmware packs and transmits `SerialFeedback` on USART2 TX (DMA or non-blocking IT).
6. At lower interval (e.g., every 50 ms), firmware emits debug CSV on USART3 (optional feature macro).

### 2.3 Module boundaries

- **`Inc/util.h` + `Src/util.c` (new in active repo)**
  - protocol structs (`SerialCommand`, `SerialFeedback`)
  - checksum helpers
  - byte-stream frame parser
- **`Src/comms.c` (extend existing)**
  - owns buffers, TX scheduling, debug formatting
  - exports:
    - `uart_comms_init()`
    - `uart_control_rx_check()`
    - `uart_feedback_periodic()`
    - `uart_debug_periodic()`
- **`Src/main.c`**
  - removes ad-hoc struct-DMA assumptions
  - calls comms APIs per loop
- **`Src/setup.c`**
  - confirms USART2 full duplex + RX DMA + TX DMA init
  - keeps USART3 TX for debug
- **`Inc/config.h`**
  - add/normalize feature flags for control/feedback/debug

---

## 3) Concrete Implementation Plan (File-by-File)

## Phase A — Protocol foundation

1. **Add `Inc/util.h` and `Src/util.c` to active firmware tree** (port minimal subset).
2. Define packed structs for command/feedback.
3. Implement checksum helpers:
   - `uint16_t serial_cmd_checksum(...)`
   - `uint16_t serial_feedback_checksum(...)`
4. Implement parser that scans a byte stream for start frame and validates checksum.

Deliverable: parser can recover from byte misalignment and return valid decoded commands.

## Phase B — UART RX reliability

1. Replace one-shot struct receive assumption in `Src/main.c`.
2. Introduce circular RX buffer for USART2 DMA.
3. Add `uart_control_rx_check()` call each loop.
4. Maintain `last_valid_command_ms` to support timeout logic.

Deliverable: command input remains stable under line noise and framing loss.

## Phase C — Feedback transmission

1. Define `SerialFeedback` population logic from existing runtime signals:
   - commanded setpoints (`cmd1`, `cmd2`)
   - measured motor speed (`speedR`, `speedL` or actual measured vars if available)
   - battery and board temperature
   - status/LED flags
2. Send feedback at fixed rate (recommended 100 Hz).
3. Guard TX with busy flags to avoid overlap.

Deliverable: host receives continuous valid feedback frames.

## Phase D — Debug output channel

1. Keep current `consoleScope()` support as optional legacy mode.
2. Add CSV debug mode compatible with the reference test script.
3. Send CSV at 20 Hz on USART3 when `DEBUG_SERIAL_USART3` is enabled.
4. If USART3 disabled, allow fallback debug on USART2 only when feedback is disabled.

Deliverable: stable logging stream for tuning and diagnostics.

## Phase E — Host tooling and verification assets

1. Copy/rename `Scripts/UART-control-test.py` into active repo script area (or document exact usage from this folder).
2. Validate with two adapters (control + debug) and one-adapter mode (control+feedback only).
3. Document wiring, voltage-level warnings (3.3 V only), and expected terminal output.

Deliverable: repeatable end-to-end bench test process.

---

## 4) Integration Points in Current Files

## `Inc/config.h` updates

Add explicit macro set (example):

- `CONTROL_SERIAL_USART2`
- `FEEDBACK_SERIAL_USART2`
- `DEBUG_SERIAL_USART3`
- `CONTROL_BAUD`, `FEEDBACK_BAUD`, `DEBUG_BAUD`
- optional `SERIAL_FEEDBACK_INTERVAL_MS`, `SERIAL_DEBUG_INTERVAL_MS`

Also enforce conflicts with `#error` blocks (same style as existing config checks).

## `Src/setup.c` updates

- Ensure USART2 RX DMA is circular and started at boot.
- Ensure USART2 TX DMA is initialized for non-blocking feedback.
- Keep USART3 TX DMA path for debug.
- Avoid local shadow UART handles (use shared globals).

## `Src/main.c` updates

- Remove direct trust of raw `command` struct memory image.
- Call parser/check function every loop.
- Use parsed command cache as input source.
- Schedule feedback/debug in deterministic loop sections.

## `Src/comms.c` updates

- Expand from scope-only debug helper into communication coordinator:
  - RX buffer processing
  - frame pack/send helpers
  - telemetry formatting

## `Src/stm32f1xx_it.c` updates

- Keep DMA IRQ handlers; ensure all enabled DMA channels used by RX/TX are routed to HAL handlers.

---

## 5) Timing, Safety, and Failure Behavior

- **Command timeout**: if no valid command for `TIMEOUT` window, force neutral command and/or disable torque request.
- **Checksum fail**: discard frame; keep last valid command until timeout.
- **RX desync**: parser scans forward to next valid start marker.
- **TX backpressure**: skip one feedback period if prior TX still busy (never block control loop).
- **Safety precedence**: UART command path must not bypass battery/temp/overcurrent shutdown paths already present.

---

## 6) Recommended Milestones and Acceptance Criteria

### Milestone 1: Command RX stable

- Can send repeated command frames from host.
- Board applies steer/speed correctly.
- Invalid frames do not cause random actuation.

### Milestone 2: Feedback live

- Host decodes continuous feedback frames at target rate.
- Battery/temp fields are plausible and stable.

### Milestone 3: Debug stream live

- CSV logs are parseable and timestamped.
- No control-loop instability when debug is enabled.

### Milestone 4: Regression checks

- ADC/PPM/Nunchuck builds still compile when UART features are disabled.
- Macro conflict checks catch invalid feature combinations.

---

## 7) Practical Bring-Up Checklist

1. Build with UART control + feedback + debug macros enabled.
2. Flash firmware.
3. Connect control adapter to USART2 (TX/RX/GND, 3.3 V logic).
4. Connect debug adapter to USART3 TX/GND.
5. Run test script with control and debug ports.
6. Verify:
   - command echo in feedback
   - motor response to speed/steer ramps
   - periodic CSV lines in debug log
7. Pull USB/host link and confirm timeout safety behavior.

---

## 8) Notes on Porting Strategy

- Do **not** copy the entire reference codebase blindly; port only UART-related layers needed for this repo’s simpler architecture.
- Keep original control/motor logic intact where possible.
- Prefer small, reviewable commits:
  1) protocol structs/parser,
  2) RX integration,
  3) feedback TX,
  4) debug CSV,
  5) docs/tests.

This minimizes regression risk and makes hardware bench debugging much easier.

---

## 9) Expected End State

After this plan is implemented, this repository will have:

- Reliable UART command ingestion on USART2
- Structured binary feedback to host on USART2
- Optional dedicated debug telemetry on USART3
- Clear compile-time feature gates and conflict checks
- A repeatable host-side verification workflow

That gives you the same practical operator experience as the reference UART-enabled firmware, but adapted cleanly to the current codebase.
