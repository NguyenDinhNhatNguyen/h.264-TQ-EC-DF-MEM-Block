# Inter VP validation — 2026-10-10

Compared against [SISLAB HAS v2.1.11](../SISLAB_H264_AVC_HAS_Spec_Detailed_v2.1.11.md)
sections 8 and 6.2. Scope: functional transaction VP with explicit search/rate/MVP
inputs. This pass extends tests and CMake within Inter-Prediction; SHA256 hashes of the nine
production headers/sources remain identical to the baseline. No other modules/parent files were changed.

## Execution results

| Configuration | Environment | Result |
|---|---|---|
| Full VP Release | WSL/Linux, GNU 15.2.0, C++17, SystemC 2.3.4 | 18/18 PASS |
| Full VP Debug + UBSan | -fsanitize=undefined; halt_on_error=1:print_stacktrace=1 | 18/18 PASS, no sanitizer diagnostics |
| Core Debug + ASan/UBSan | TLM/DMA OFF; -fsanitize=address,undefined; leak detection ON | 3/3 PASS, no sanitizer diagnostics |
| Full VP Debug + gcov | --coverage; 7 production .cpp files | 18/18 PASS |
| Core Release Windows | MSYS2 UCRT64 GNU 16.1.0; TLM/DMA OFF | 2 PASS, 1 NOT RUN: Application Control |

Final Linux/UBSan/coverage reruns were around 13:47-13:48 +07. Core ASan and native core
runs used the final core tests; later changes only added TLM rejection checks.
Sanitizers instrumented the module/tests and DMA headers; the SystemC library was not rebuilt.
Windows blocked `h264_inter_core_tests.exe` with the message
"An Application Control policy has blocked this file". Elevated CTest was also
blocked; the full native Windows suite remains incomplete. The interpolation
and limits executables passed. The blocked core test passed on Linux and under sanitizers.

Raw LastTest logs, CTest registration, configuration, coverage summary/script and SHA256 hashes:
[full_test_20261010.txt](tests/evidence/full_test_20261010.txt).
[validation_20261010.txt](tests/evidence/validation_20261010.txt) records the 13-test baseline
at 12:49, not evidence for the current expanded suite.
Evidence is retained after build cleanup.

| Test group | Assertions |
|---|---:|
| Core SAD/cache/search/MC/EEI | 103190 |
| Interpolation oracle, 3 seeds | 144002 |
| Limits / geometry / invalid inputs | 30836 |
| Actual SW DMA integration bus 32 / 64 / 128 | 33686 / 29078 / 27254 |
| Normal / zero-latency TLM | 11337 per group |
| Shared / local / disable cancellation | 601 per group |
| Retag / incoming-delay / stale-malformed-rearm | 594 / 605 / 51 |
| Partitions / partitions-zero | 107359 per group |
| Boundaries / boundary-zero | 19843 / 19793 |

Counts include harness/protocol assertions; they are not HAS coverage percentages.
There are no expected-failure tests. The bench requires driver completion and settled helper actions;
simulation starvation is not counted as PASS.

## Coverage

GCC 15.2.0 gcov JSON, with counters cleared before the final coverage run. Only seven
production .cpp files are counted, excluding headers, tests, the standard library, SystemC and DMA.
A covered line/branch outcome has an execution count greater than zero.

| Source | Lines | Branch outcomes taken |
|---|---:|---:|
| cache/reference_cache.cpp | 70/70 | 117/154 |
| eei/motion_syntax.cpp | 4/4 | 3/6 |
| fme/interpolation.cpp | 50/50 | 62/74 |
| fme/search.cpp | 74/74 | 120/160 |
| ime/sad_tree.cpp | 17/18 | 16/18 |
| ipc/inter_tlm.cpp | 150/150 | 232/321 |
| mc/motion_compensation.cpp | 47/48 | 77/98 |
| Total | **412/414 (99.52%)** | **627/831 (75.45%)** |

The two unexecuted lines are closing braces at sad_tree.cpp:24 and
motion_compensation.cpp:40. The gcov branch denominator includes compiler exception/unwind
paths; branch coverage is below 100%. These figures do not prove every HAS requirement.

## Verified behavior

IME checks 41 partition positions, maximum SAD 65280, wide rate, overflow, stable
ties and a randomized oracle. Interpolation checks 16 luma/64 chroma phases, negative/border coordinates,
constant/ramp/impulse/checkerboard/step/random patterns and signed diagonal intermediates.

Limits cover 40 cases: 5 dimensions (16x16, 176x144, 1280x720, 1920x1088,
4096x4096) x 4 signed MV extremes at +/-65536 x 2 MB positions. Sparse corner fixtures
check clamping, inward fractional refinement and all 384 MC samples;
they do not encode full frames. Checks include up to 4096 integer/64 fractional candidates,
invalid dimensions/plane/search/MC, atomic refill and retag during core evaluation.

The TLM partition regression runs 246 cases per timing profile: 3 seeds x P/B x 41
positions. Independent source/oracle data creates a fractional winner and checks list/tag/MV/MVD,
residuals, every Y/U/V sample, independent transform-block order, phase/last, repeated
Peek, bad Accept, consumer stalls and final done. Total across both profiles: 492 cases.

The deadline regression runs 144 cases per profile: shared/local reset, disable or
retag x 12 sites x {-1,0,+1} ps. Sites include incoming delays, IME/FME service/compare,
Commit, Peek/sample, Accept/final Accept and repeated held Peek. Normal: 46 accepted,
98 cancelled; zero service: 96 accepted, 48 cancelled. At the exact deadline timestamp,
valid SystemC sequential orderings are accepted; no RTL priority is asserted. Checks cover
completion/cancellation timestamps, owner invalidation and absence of stale output/done.

Existing TLM tests also check row-start 8+8 columns, unrelated-list refill,
delayed U, B startup, P/B/P, current-MB snapshots before incoming delay, benign
notifications retaining deadlines, stale-at-entry retaining the owner, reset/enable-low rearm,
malformed transport, Peek/Accept before Commit, duplicate Commit and local-only
waits without EpochExtension.

Actual SwDma/H264Arb/AxiMasterBridge/DdrModel integration uses 48x32, three P/B/P pictures
and 18 MBs per 32/64/128-bit bus width. It checks overlap reuse, successful-read
residency, RRESP fault/retry, stale outstanding responses after retag, 4 KiB splitting
and predictors from reconstructed DDR references.

## Reproduction

Run from Inter-Prediction; see the [README](README.md) for full/core builds. The environment
used SystemC include/library paths under /opt/systemc-2.3.4 with matching compiler/C++17 ABI.

```sh
cmake -S . -B build/full -DCMAKE_BUILD_TYPE=Release \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so
cmake --build build/full -j 4
ctest --test-dir build/full --output-on-failure

cmake -S . -B build/ubsan -DCMAKE_BUILD_TYPE=Debug \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so \
  -DCMAKE_CXX_FLAGS=-fsanitize=undefined \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=undefined
cmake --build build/ubsan -j 4
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build/ubsan --output-on-failure

cmake -S . -B build/asan-core -DCMAKE_BUILD_TYPE=Debug \
  -DH264_INTER_BUILD_TLM=OFF -DH264_INTER_BUILD_DMA_TESTS=OFF \
  -DCMAKE_CXX_FLAGS=-fsanitize=address,undefined \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined
cmake --build build/asan-core -j 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build/asan-core --output-on-failure
```

Coverage: configure full Debug with CXX/link flags `--coverage`, build, then run
CTest sequentially with a fresh build/counters. From `build/coverage`:

```sh
find CMakeFiles/h264_inter_core.dir CMakeFiles/h264_inter_tlm.dir \
  -name '*.gcno' -exec gcov -b -j {} +
python3 ../../tests/evidence/coverage_report.py .
```

## Acceptance limits

See [spec mapping](docs/SPEC_MAPPING.md) and [open tests](docs/TEST_PLAN.md).
This suite runs all current VP tests; it is not full encoder signoff.
SISLAB search/rate/MVP/comparator policy has not been confirmed against the Algorithm Spec/RTL;
skip/direct, bi/weighted prediction, autonomous partition selection,
actual TQ/entropy/DF feedback, QCIF byte/PSNR/601-frame qualification and RTL cycle
equivalence remain unverified. Simulation time does not establish fixed latency/throughput.

Parent CMake/export has not yet connected the module. When pushing this folder separately, the
destination repository needs the shared contracts and DMA dependencies; disable DMA tests if the sibling
DMA module is absent. The specification is read from the sibling path, not copied into the module.
