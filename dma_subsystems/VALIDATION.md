# DMA validation — 2026-10-10

Environment: Ubuntu WSL, GCC 15.2.0, CMake 4.2.3, Accellera SystemC 2.3.4
at `/opt/systemc-2.3.4`.

| Check | Result |
|---|---|
| Core: 78 original + 194 review/external-memory assertions | 272 pass, 0 fail |
| Shared-bus integration, DMA 32-bit | 284 pass, 0 fail |
| Shared-bus integration, DMA 64-bit | 284 pass, 0 fail |
| Shared-bus integration, DMA 128-bit | 284 pass, 0 fail |
| Bounded stalled-channel activation | 12 pass, 0 fail |
| Total per full CTest run | **1,136 assertions, 5/5 suites pass** |
| CMake standalone and CDC-VP root build | Both build/test runs pass |
| Standalone, `-Wall -Wextra -Wpedantic -Werror` | Compile/link pass |
| Core ASan + UBSan, including borrowed-memory lifetime | 272 pass; no diagnostics |

[Standalone log](tlm/tests/evidence/standalone_tlm_20261010.log),
[root build log](tlm/tests/evidence/root_tlm_20261010.log),
[core sanitizer log](tlm/tests/evidence/core_sanitized_20261010.log).
These logs are retained through `.gitignore`. No ASan run is claimed for the SystemC
wrapper; the sanitizer evidence above applies to the core.

## Historical core evidence

The logs below preserve earlier reviews before external-memory/SystemC integration
was added; they do not replace the 272-assertion results above:

- [Core fixes](tb/evidence/dma_tests_fixed_20261010.log).
- [Core sanitizer snapshot](tb/evidence/dma_tests_sanitized_20261010.log).
- [Spec review](tb/evidence/spec_review_tests_20261010.log).

Build instructions, contracts and model limitations are in the [README](README.md).
The folder cleanup only reorganized documentation/logs and ignored generated files; it did not change
implementation/tests or rerun the regression. Functional DMA VP results do not establish full encoder
or RTL signoff.
