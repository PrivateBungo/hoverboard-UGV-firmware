## Objective and Strategy

### Objective

The goal of this effort is to equip the hoverboard-UGV-firmware with a robust, well-documented **UART-based remote control and feedback system**, closely mirroring the proven architecture and communication protocol from the hoverboard-firmware-hack-FOC project.

By implementing this UART interface, the firmware will support:
- **Remote Command Input:** Receiving speed and steering commands over a serial interface (USART2, 3.3V logic).
- **Real-Time Feedback:** Transmitting system status, measurements, and states back to the host continuously via the same interface.
- **Optional CSV Debug Output:** Providing detailed, structured diagnostic output on a separate serial port (USART3) for logging and analysis.

This feature dramatically improves flexibility and integration with PC-side tools or microcontroller hosts, enabling enhanced motion control, testing, and diagnostics for UGV and robotics applications.

---

### Approach and Strategy

This project uses an incremental, code-driven porting strategy, leveraging mature building blocks from a well-tested firmware base. The plan is:

1. **Direct Import of Key Modules:**  
   Copy all protocol definition, parsing logic, and communication glue code (see File Summary below) from the FOC firmware into the new project. This ensures exact protocol compatibility and saves time versus reimplementation.

2. **Modular & Maintainable Design:**  
   Keep protocol/data structure definitions in headers (`util.h`), processing helpers in dedicated source files (`util.c`), and high-level serial state/multiplexing/control in separate communication modules (`comms.c`). Maintain clear boundaries between hardware config, control logic, and communication code.

3. **Preserved Testability:**  
   Import and use the known-good Python test tool (`uart_control_test.py`) for rapid feedback during development. This allows for systematic validation at every step, ensuring commands are received and system feedback is output as expected.

4. **Configuration via Macros:**  
   Use `config.h` to select the active communication mode (UART or other) at compile time, making it easy to expand or disable features as the project grows.

5. **Stepwise Integration:**  
   - First, stand up UART packet parsing and decoding from USART2.
   - Next, enable regular feedback transmissions.
   - Finally, activate CSV/debug output on USART3, if desired.
   - All integration is test-driven, verifying with the Python test script and live serial port tools.

6. **AI-Assisted Refactoring:**  
   With all relevant code in place and context visible to coding assistants (e.g., Copilot, GPT-4), architectural adaptation, variable renaming, and application-specific glue are refactored and “stitched” into the new control system with high fidelity and traceability.

---

### Scope

While this brings UART/serial control and feedback to the hoverboard-UGV-firmware project, it is designed to be adaptable to similar STM32-based systems, and intends to provide a solid, forward-compatible foundation for remote control, feedback, telemetry, and logging.

---

*(For file-level details, see below.)*

---


## UART Control & Feedback File Overview

This firmware implements UART-based remote control, feedback, and CSV debug output modeled after the hoverboard-firmware-hack-FOC architecture.  
Below is a summary of each imported file/module, its purpose, and its relevance for the functionality:

---

### File Summary

| File/Filename                  | Main Purpose                                           | Key Relevance |
|--------------------------------|-------------------------------------------------------|---------------|
| `Inc/util.h`                   | Protocol structs (`SerialCommand`, `SerialFeedback`), function prototypes for UART parsing/processing. | Defines packet formats and key handler APIs. |
| `Src/util.c`                   | Implements helper functions for UART protocol (frame parsing, checksum, etc.) | Low-level protocol logic (encode/decode, CRC/XOR/checksum, etc.) |
| `Src/comms.c` <br> `Inc/comms.h` | UART communication logic. Parses serial bytes, dispatches packet processing, manages timeouts. | Main code for serial parsing, command/feedback handling. |
| `Inc/config.h`                 | Feature and variant macros (#defines) for build-time config (e.g., enabling serial control/feedback on USART2/3) | Ensures correct setup of UART/USART and debug features during build. |
| `Src/main.c`                   | Main firmware entry point and control loop. Integrates UART packet handling, control logic, and feedback/CSV debug output. | Shows how decoded UART commands connect to drive logic, and how outgoing feedback is sent. |
| `Inc/stm32f1xx_hal_conf.h`     | STM32 HAL driver configuration (which periperhals' modules are compiled in) | Required only if not previously set in your project—needed for HAL-based UART code. |
| `scripts/uart_control_test.py` | Python script for host-side UART testing (packet send/receive, feedback display, CSV debug capture). | Great for end-to-end validation and serves as reference for binary protocol format. |

---

### Notes

- All files together form a clear, modular separation between low-level protocol parsing (util), serial comms glue code (comms), build-time setup (config), and main application logic (main.c).
- `uart_control_test.py` allows test-driven development—use it to verify command reception, feedback replies, and debug data output.
- STM32 HAL driver files (e.g., `stm32f1xx_hal_uart.c`) are usually provided by ST and do not need to be modified or copied unless there were project-specific changes.
- The architecture is designed for straightforward integration and porting to new STM32-based projects.

---
