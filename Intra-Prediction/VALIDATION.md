# Intra-Prediction validation

Compared against [HAS v2.1.11](../SISLAB_H264_AVC_HAS_Spec_Detailed_v2.1.11.md)
sections 7.1-7.5, 13.2, 15.3-15.4 and 18.3, together with the
[DMA contract](../dma_subsystems/DMA_SUBSYSTEM_SPEC.md).
This validates the prediction block and VP/TLM protocol.

## Final check

On 2026-10-10 at 11:59 +07: rebuilt and ran standalone **10/10 PASS**,
UBSan **10/10 PASS**, with no diagnostics. Reviewed production sources against
the HAS/DMA specification; no new functional issues were found within the current vectors
and VP contract. Source/test hashes still match the regression evidence.
This pass wrote only inside the Intra folder; the H264 integration 55/55 result below is
evidence from 11:47 and was not rerun during the final check.

## Results

Regression run on 2026-10-10, 11:46-11:47 +07: WSL, GNU C++ 15.2.0,
C++17, SystemC 2.3.4. Release for regression, Debug for UBSan.

| Suite | Result |
|---|---|
| Standalone Intra | **10/10 PASS** |
| H264 component | **55/55 PASS**, including 22 intra/prediction-related tests |
| Standalone UBSan | **10/10 PASS**, `halt_on_error=1`, no diagnostics |
| Core algorithms / independent regression | 2,316 / 79,382 checks |
| TLM contract / reset matrix | 62 / 590 checks; reset matrix has 44 scenarios |
| Extended TLM / zero latency | 7,420 / 7,388 checks |
| Shared-reset boundary / zero service latency | 710 / 228 checks; 42 / 14 cases |
| Shared-DMA integration 32/64/128 | 3/3 PASS, 18 checks per bus |
| CMB full picture 32/64/128 | 3/3 PASS, 22,626 checks per bus |
| CMB mixed partition 32/64/128 | 3/3 PASS, 22,630 checks per bus |
| CMB TQ wiring 32/64/128 | 3/3 PASS, 13,881 checks per bus |

[Regression evidence](tests/evidence/regression_20261010.txt) contains commands,
source/test hashes and raw logs for all three suites. Intra tests do not use
`WILL_FAIL`. The 55 H264 tests include codec stubs and expected-failure cases;
PASS in those tests validates contracts/error rejection, not codec correctness.

## Requirements and evidence

| HAS requirement | Test / observable | Scope |
|---|---|---|
| Section 7.1 sample generators and legal modes | `core_tests.cpp`, `review/core_review.cpp`, `prediction_oracle.h`: 9 luma4 modes, 4 luma16/chroma modes; 512 reference sets x 16 availability masks | Independent oracle using coefficient tables, plane gradients and chroma DC quadrants; random/boundary inputs, seed 20261010 |
| Section 7.1 reference availability | All 16 scan positions; guards at 3/7/11/13/15 even after future pixels are imported; full coded width and entire right edge | Core and independent decoded/valid map in the CMB bench |
| Section 7.2 reconstructed ownership | Source 255 / feedback 17 / next predictor 17; separate Y/U/V storage/validity | CMB source does not automatically become neighbor data; decoded feedback fixture is explicitly identified |
| Section 7.2 RAM latency and metadata | `tlm_tests.cpp`, `tlm_regression.cpp`: token/block/reference snapshot; caller metadata changes do not switch blocks | Checks VP abstraction timing and alignment, not RTL RAM pipeline qualification |
| Section 7.3 cost accumulation / best candidate | SAD/SATD matrix oracle, every candidate/global minimum in picture vectors; signed residual; 64-bit penalty | Numeric candidate order and SAD/SATD are VP choices documented in README |
| Section 7.3 strict ties | Y4/Y16/U8/V8 x SAD/SATD x 3 scenarios: cost 0, nonzero distortion, penalties for modes 1/2 | 24 vectors with both custom and zero latency; retain the first legal minimum across repeated Replay and commit |
| Section 7.4 reset / repeated activation | `reset_tests.cpp`, `review/epoch_review.cpp`: local/shared/frame reset, benign notifications, stale-entry isolation, old-token rejection, recovery | 44-case matrix across incoming/reference/candidate/compare/replay/feedback/import; held-high enable does not restart the frame |
| Section 7.4 winning replay / right edge | Force modes 3/7 to win at x44, width48; compare every predictor/residual byte, mode/cost/reference snapshot and committed mode | Evaluate/Replay use identical upper-right/right-edge rules |
| Section 7.4 stall / completion | Repeated Replay after stalls; premature feedback rejection; observer during feedback wait; completion event after memory/mode commit | TLM blocking acceptance; caller aggregates block done into MB done |
| Section 7.5 variable latency | Measure 1/2/3/4/6/9 legal candidates; custom 9/6/3/8/11ns, incoming7ns, zero service latency | 6-candidate Evaluate = 70ns; benign notifications preserve deadlines; HAS does not require a fixed MB-cycle count |
| Shared-reset boundary | `reset_boundary_tests.cpp`: 7 sites x offsets -1/0/+1ps x 2 timer orders; additional zero-service cases | 42 + 14 cases; status/state/events follow reset before/after acceptance, with no obsolete commit; no local/frame exact-deadline sweep yet |
| Section 7.1 caller scheduling / CMB handoff | `../integration_tests/prediction/cmb_intra_tb.cpp`: bus32/64/128, Y/U/V, 4 pictures48x32, 24MB/252blocks | Homogeneous and checkerboard luma4/luma16; independent reference map; all pixels commit before picture completion |
| DMA ownership | CMB readiness after all 32 row responses; planar strides/lanes, 4KiB boundaries and error path | Actual CmbDma/H264Arb/AxiMasterBridge/DdrModel; failed responses do not expose a tile |
| Section 15.3 HAS-V-INTRA-01 | Vectors cover reset, upper-right/right-edge, ties and predictor/replay | Observable bitstream and RTL/codec comparisons remain open |

At the same timestamp, boundary tests record whether reset actually occurs before or after
the response, then check status, flags and event counts. Reset before
entry only rejects stale requests; reset cancelling a live request invalidates the old frame.
Zero service latency provides no yield point inside the operation; incoming-delay
cases in that suite still use 20ns. Shared reset while idle requires caller coordination of
local reset/frame enable according to the [socket contract](README.md#tlm-interface).

## Reset fixes protected by regression

- **F1: incoming-delay cancellation retained old tokens/context.** Validate epoch,
  token, block and payload before incoming wait; a valid live request owns
  the frame before waits. Cancellation invalidates the old frame even before a
  winner exists; stale/invalid requests do not clear the current owner. A new generation
  is not cleared by an old transaction catch handler.
- **F2: shared reset did not wake latency waits.** Wait on both the local
  generation event and `ResetDomain::changed`; recheck the epoch after wake-up.
  Notifications that do not change the epoch continue waiting until the original deadline.

Both reproducers reset at 12ns and return cancellation at 12ns; obsolete Replay
and feedback commits are rejected. Fixes are in `src/intra_tlm.cpp`.

## Coverage and evidence before regression expansion

The [fix evidence snapshot](tests/evidence/fix_20261010.txt) records core
ASan+UBSan with 79,382 checks and production gcov before expansion to 10 tests.
Both production files still match the hashes in that snapshot.

| Production file | Executable lines | Branches executed | Branch outcomes taken ≥1 |
|---|---|---|---|
| `src/intra_core.cpp` | 100% / 244 | 100% / 426 | 86.15% |
| `src/intra_tlm.cpp` | 100% / 114 | 98.17% / 219 | 71.69% |

Branch metrics include compiler/exception paths and defensive guards. 100% line
coverage is not 100% input/branch/spec coverage. Some test hashes differ from the old
snapshot because the regression was expanded; use the new regression manifest
to compare current tests.

Core-only 2/2, isolated review 3/3 and installed consumer 1/1 passed in an
earlier run. Coverage/ASan and those suites were not rerun at
11:46-11:47; production code was unchanged when those tests were added.

## Rerun

Configure/build from source using the [README](README.md#standalone-build-and-tests). For
already configured builds, run from the H264 folder:

```sh
cmake --build Intra-Prediction/build-tlm-linux -j 8
ctest --test-dir Intra-Prediction/build-tlm-linux --output-on-failure
cmake --build build/intra-linux -j 4
ctest --test-dir build/intra-linux -R 'intra|prediction' --output-on-failure
ctest --test-dir build/intra-linux --output-on-failure
```

To create a new UBSan build, run from the Intra-Prediction folder:

```sh
cmake -S . -B build-ubsan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS=-fsanitize=undefined \
  -DCMAKE_PREFIX_PATH=<SystemC-prefix>
cmake --build build-ubsan -j 4
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build-ubsan --output-on-failure
```

Raw CTest logs are in `Testing/Temporary/LastTest.log` for each build.
Build trees/caches and working reports are not part of source delivery; retained
evidence in `tests/evidence` is text versioned with the module.

## Scope when pushing this folder separately

- The 10 standalone tests and module sources belong to this folder. TLM uses
  `../interfaces` and `../synchronization`, which are already versioned in the repository;
  core-only does not need SystemC.
- Parent `../CMakeLists.txt` must add the module; `../cmake/install.cmake` must
  export both libraries. CMB/TQ suites are in `../integration_tests/prediction`
  and require DMA headers in `../dma_subsystems`. Changes outside this folder
  must be integrated separately by their owner to reproduce H264 55/55 and installed targets.
- At review time, the HAS Markdown and `../dma_subsystems` were still untracked.
  An Intra-only commit does not add these specification/dependency sources to Git;
  the owner must version them for links and integrated tests to work in a fresh checkout.
- For DMA integration, use shared coded dimensions up to 1920x1088 per the DMA
  specification. Standalone Intra core allows up to 4096 per dimension at VP level.

Build/cache/archive files are ignored. This delivery covers the VP module and
the evidence stated above, not full HAS/encoder signoff.

## Qualification limits

- TQ wiring uses actual TqTop for 192 Y4 blocks per bus at QP26. FTQ currently
  bypasses special DC Hadamard and lacks quantization scaling; ITQ lacks QP-dependent
  inverse scaling. Chroma/luma16 feedback remains a fixture. PASS verifies wiring
  and ownership, not H.264 transform correctness.
- No released codec adapter or independent SISLAB RTL golden data exists for selected
  modes, reconstructed pixels and bitstream. Sections 15.4/18.3 still require QCIF byte
  equality with matching source/configuration, independent decode, PSNR and repeatability.
  Release-vector byte counts in HAS are not expected lengths for every
  QCIF video.
- The caller/pipeline still owns partition selection, production MB scheduling,
  slice/constrained-intra availability and MPM syntax. The module supports
  8-bit 4:2:0 as documented in README.
- Local/frame reset exactly at deadlines, multiple random seeds and
  QCIF/HD picture stress have not been swept. Full-picture integration currently uses 48x32.
- SystemC tests do not replace cocotb on `h264_axi_wrapper`, pin/cycle RTL
  qualification or HAS FPGA timing/DRC/long-run hardware evidence.
