# Control and registers

Owner: Huy Nguyen Huynh Quoc.

Chapter 4: register TLM target and activation/frame controller.

CMake target: `h264_control`. Public header: h264/control/control_regs.h; h264/control/encoder_controller.h.

Regression tests currently live in tests because they require host + DDR + top. Run from components/h264:
`ctest --test-dir build/vp --output-on-failure`. Relevant tests: registers, invalid_params, integration_*.

Read `interfaces/docs/interface_contracts.md` before
changing an interface. All headers use .h. Do not duplicate source files across tasks.

