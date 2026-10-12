# H.264 Inter Prediction

Author: VinhNT124

H.264 inter-frame prediction model with a C++17 core and a SystemC 2.3.4/TLM-2.0 interface. The module performs integer/fractional motion estimation, motion compensation and motion-syntax metadata generation from the current macroblock and reconstructed reference data.

## Functions

- IME (Integer Motion Estimation): computes SAD over sixteen 4x4 blocks for each candidate, aggregating 41 positions across 7 partition shapes.
- FME (Fractional Motion Estimation): starts from the integer winner and evaluates caller-supplied fractional offsets; luma interpolation uses a six-tap filter and quarter-sample averaging.
- MC (Motion Compensation): outputs Y/U/V predictors in transform-block order with plane/block/phase/last metadata.
- EEI: generates reference, partition, MV and MVD information from the caller-supplied MVP.
- Separate List0 and List1 reference caches manage picture tags, loaded pixel regions and picture-edge extension.
- Holds samples/metadata stable during downstream stalls; manages tokens, reset and shared epochs through TLM.

Costs use 64-bit accumulators and reject overflow. The caller supplies candidate order, rate cost and MVP; equal total costs retain the first evaluated candidate. Interpolation preserves signed intermediates before clipping results.

## Structure and CMake targets

Public namespace: `h264::inter`.

| Target | Role |
|---|---|
| `h264_inter_core` | Reference cache, SAD, search, interpolation, MC and motion syntax; no SystemC dependency |
| `h264_inter_tlm` | TLM socket, timing and Evaluate/Commit/Peek/Accept control |

The corresponding aliases are `cdc::components::h264_inter_core` and `cdc::components::h264_inter_tlm`. Public headers are in `include/h264/inter/`, implementation in `src/`, and standalone tests in `tests/`. TLM uses shared contracts from `../interfaces` and `../synchronization`; SW DMA integration tests use the sibling `../dma_subsystems` module.

## Data flow and core API

```text
Reconstructed reference ── SW DMA / refill ──> Reference cache
Current MB + candidates ── Evaluate ────────> IME / FME
Selected decision       ── Commit ─────────> MC
Downstream              <── Peek / Accept ── Predictor + metadata
```

[inter_core.h](include/h264/inter/inter_core.h) provides a SystemC-independent API. Requests supply a current Y16x16 luma macroblock, absolute macroblock coordinates and a partition. P-pictures use List0; B-pictures require integer candidates and valid search data in both List0/List1. The result selects uni-prediction on one list. Fractional offsets are relative to the integer winner and range from [-3,3] quarter samples.

Reference tags contain picture identity and REFM slot 0..2. Each `retag` creates a new incarnation, even if the tag is unchanged. `fill` validates the entire region before updating loaded state; loaded pixels remain immutable until the next retag, and overlapping regions are accepted when their data matches.

Supports progressive 8-bit planar YUV 4:2:0. Coded width/height range from 16..4096 in multiples of 16; when connected to DMA, the shared maximum is 1920x1088. Motion vectors are limited to +/-65536 quarter samples.

## TLM interface

[inter_tlm.h](include/h264/inter/inter_tlm.h) defines the simulation socket operations:

| Address / operation | Command / payload | Data and result |
|---|---|---|
| `0` / Evaluate | WRITE, 256 Y bytes and request extension | Returns decision, token and EEI; does not commit |
| `1` / Commit | WRITE, 0 bytes and token | Locks the winner for MC predictor generation |
| `2` / Peek | READ, 1 byte and token | Returns current sample/metadata without advancing the cursor |
| `3` / Accept | WRITE, previously peeked sample and token/accept_sequence | Acknowledges the sample and advances the cursor; final sample sets done |

Repeated Peek returns the same sample. `output()` retains data/metadata while downstream stalls; only Accept advances to the next sample. Chroma for small partitions may cover only part of a 4x4 transform block; the TQ adapter must assemble all parts before transforming.

The SW DMA adapter calls `retag` before search and `refill` after successful reads. A matching tag with missing required reference regions still makes search wait for data. Retagging during search, or retagging the selected reference, invalidates the corresponding decision. Inter receives data through the adapter; the SW module issues DMA transactions.

Create modules/sockets before `sc_start` and call `b_transport` from `SC_THREAD`. Keep payloads, buffers and extensions alive until the response. The module handles one transport and one pending mode at a time; requests/source data are copied before the incoming-delay wait. `progress()` identifies the active stage, candidate and sample.

### Timing and reset

Option latencies are VP timing parameters. Active-low `reset_n` cancels work; falling enable and the rising edge starting a new frame invalidate both reference lists and the mode. After reset while enable is high, drive enable low and then high to restart.

An optional `h264::EpochExtension` connects the shared reset domain. An epoch already stale at entry is rejected without disturbing valid work; cancellation during an active transaction invalidates that frame. The caller keeps the domain alive until the response and coordinates enable/reset to restart, including shared resets while the module is idle.

## Standalone build and tests

Requires CMake 3.21 or later and a C++17 compiler. Run the following commands from the `CDC-VP` root in WSL/Linux.

### Core without SystemC

```sh
cmake -S components/h264/Inter-Prediction -B build/h264-inter-core \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DH264_INTER_BUILD_TLM=OFF -DH264_INTER_BUILD_DMA_TESTS=OFF
cmake --build build/h264-inter-core -j 4
ctest --test-dir build/h264-inter-core --output-on-failure
```

### Core, SystemC/TLM and SW DMA tests

SystemC 2.3.4 must match the project compiler and C++ ABI. Replace the paths below with your installation:

```sh
cmake -S components/h264/Inter-Prediction -B build/h264-inter-tlm \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DH264_INTER_BUILD_TLM=ON -DH264_INTER_BUILD_DMA_TESTS=ON \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so
cmake --build build/h264-inter-tlm -j 4
ctest --test-dir build/h264-inter-tlm --output-on-failure
```

You may use `CMAKE_PREFIX_PATH` pointing to the `SystemCLanguage` 2.3.4 package, or `SYSTEMC_SOURCE_DIR`, instead of the include/library pair. Disable `H264_INTER_BUILD_DMA_TESTS` for core and TLM without SW DMA tests.

| Test group | Test count | Coverage |
|---|---:|---|
| Core | 3 | SAD, cache, search, MC/EEI, interpolation and geometry/MV limits |
| SW DMA integration | 3 | Reference Y/U/V, errors, retry, retag and overlap on 32/64/128-bit buses |
| TLM contract | 12 | Normal/zero-latency timing, reset/disable, tokens, retag, partitions and deadline boundaries |

A full build registers **18 tests**; disabling SW DMA tests leaves **15 tests**; disabling both TLM and SW DMA tests leaves **3 tests**. Verification results and logs are in [VALIDATION.md](VALIDATION.md); scenarios are described in [TEST_PLAN.md](docs/TEST_PLAN.md).

## H.264 integration tests

[Prediction_Test](../tests/Prediction_Test/README.md) connects SW DMA to Inter through `H264Arb` and `DmaTransport`, using the DMA subsystem arbiter/AXI bridge. The `sw_inter` group checks references, predictors, metadata, Peek and 384 Y/U/V samples per macroblock on 32/64/128-bit buses. The control group checks Intra/Inter coordination within a workload.

The [full codec test](../tests/Coding_Reconstruction_Test/FULL_PIPELINE_CODEC_REPORT.md) connects Inter with TQ/ITQ, CAVLC/NAL, reconstruction and reference DMA. The adapter processes 32x32 YUV420 pictures, four I-P-P-P frames, one List0 reference and P16x16 with zero MV. Codec verification in this group is limited to that configuration.

```sh
cmake -S components/h264 -B build/h264-vp \
  -DCMAKE_BUILD_TYPE=Debug -DH264_BUILD_TESTS=ON \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so
cmake --build build/h264-vp -j 4
ctest --test-dir build/h264-vp -R '^h264_prediction_sw_inter_' --output-on-failure
ctest --test-dir build/h264-vp -L prediction --output-on-failure
ctest --test-dir build/h264-vp -R '^full_codec_' --output-on-failure
```

The `prediction` label includes **15 integration tests** for DMA, Intra, Inter and control. The H.264 component imports prediction libraries with their standalone suites disabled, so use the standalone build above for the complete Inter suite. The **36 `full_codec_*` tests** are registered only when CMake finds Python3 and FFmpeg; inspect them with `ctest --test-dir build/h264-vp -N -R '^full_codec_'`.

## Usage scope

The caller selects partitions, candidates, rate costs and MVP, and supplies reconstructed references through SW DMA. Other modules perform TQ, reconstruction, entropy and deblocking. Automatic partition/mode selection, skip/direct and bi/weighted prediction are not implemented.

VP timing has not been compared cycle by cycle with RTL. The codec pipeline currently lives in a test adapter; Inter is not yet included in H.264 package install/export.
