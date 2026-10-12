# Clock, reset and synchronization

Owner: Huy Nguyen Huynh Quoc.

Chapter 13: reset generation and sticky completion utilities (header-only).

CMake target: `h264_sync`. Public header: h264/sync/reset_domain.h.

Regression tests currently live in tests because they require host + DDR + top. Run from components/h264:
`ctest --test-dir build/vp --output-on-failure`. Relevant tests: reset, registers.

Read `interfaces/docs/interface_contracts.md` before
changing an interface. All headers use .h. Do not duplicate source files across tasks.

