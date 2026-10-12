# H.264 Intra Prediction

Author: VinhNT124

H.264 intra-frame prediction model with a C++17 core and a SystemC 2.3.4/TLM-2.0 interface. The module generates predictors, residuals and mode decisions for each block; reconstructed pixels are retained as references for subsequent blocks.

## Functions

- Luma 4x4: all 9 modes; luma 16x16: vertical, horizontal, DC and plane.
- Chroma U/V 8x8: DC, horizontal, vertical and plane.
- Looks up top, left, upper-left and upper-right samples from reconstructed context, with separate validity for each plane.
- Evaluates modes using SAD or 4x4 Hadamard SATD plus caller-supplied `mode_penalty`. Larger-block SATD sums 4x4 tiles; costs use 64-bit accumulators.
- Retains predictors, signed 16-bit residuals and reference snapshots by token until reconstruction feedback arrives.
- Manages frames, reset and transaction cancellation through shared epochs.

Legal modes are evaluated in ascending mode-number order. Equal costs retain the first evaluated mode. If upper-right is unavailable, samples E..H are replaced with D; scan positions 3/7/11/13/15 do not read invalid upper-right samples.

## Structure and CMake targets

Public namespace: `h264::intra`.

| Target | Role |
|---|---|
| `h264_intra_core` | Prediction algorithms, mode selection and reconstructed context; no SystemC dependency |
| `h264_intra_tlm` | TLM socket, timing, tokens and frame/reset lifecycle |

The corresponding aliases are `cdc::components::h264_intra_core` and `cdc::components::h264_intra_tlm`. Public headers are in `include/h264/intra/`, implementation in `src/`, and standalone tests in `tests/`. TLM uses shared contracts from `../interfaces` and `../synchronization`.

## Data flow and configuration

```text
CMB DMA / local RAM ── Evaluate(source) ──> IntraTlm
TQ adapter          <── Replay(predictor, residual)
Reconstruction      ── Reconstruct(decoded pixels) ──> neighbor/mode RAM
```

The CMB adapter packs source pixels per block in row-major order. After reading predictors/residuals, TQ/ITQ reconstructs and clips pixels to 8 bits, then feeds them back to the module. Reference context is updated with reconstructed pixels through reconstruction feedback or `ImportReconstructed`.

Example configuration and connections:

```cpp
h264::intra::Options opts;
opts.coded_width = 176;
opts.coded_height = 144;
h264::intra::IntraTlm intra("intra", opts);
initiator.bind(intra.target_socket);
intra.reset_n(reset_n);
intra.frame_enable(frame_enable);
```

Create modules and connect sockets/signals before `sc_start`. Call `b_transport` from `SC_THREAD`; keep payloads, buffers and extensions alive until the transaction returns.

Coded width/height are fixed per instance, ranging from 16..4096 in multiples of 16. `Block::x/y` coordinates are measured in plane samples; U/V dimensions are half those of Y. Luma 4x4, luma 16x16 and chroma 8x8 must be aligned to 4, 16 and 8 samples respectively. When connected to DMA, the shared maximum is 1920x1088.

The caller processes macroblocks in raster order and luma 4x4 blocks in this scan order:

```text
 0  1  4  5
 2  3  6  7
 8  9 12 13
10 11 14 15
```

## TLM interface

The following addresses identify simulation socket operations. Each transaction requires `h264::intra::Extension`, an NxN-byte buffer, streaming width at least equal to buffer length, and no byte enables. The socket does not provide DMI.

| Address / operation | Command | Data and result |
|---|---|---|
| `0` / Evaluate | WRITE | Source pixels, block, metric and penalty; returns decision/token |
| `1` / Replay | READ | Evaluate token; returns predictor and a decision containing residuals |
| `2` / Reconstruct | WRITE | Reconstructed pixels and token; updates neighbor/mode RAM and `block_done` |
| `3` / ImportReconstructed | WRITE | Reconstructed pixels and block; imports context when no block is pending |

The module holds at most one pending block. Replay may be read repeatedly with the same token and always returns the selected snapshot. Reconstruct requires a preceding Replay; Evaluate or ImportReconstructed while pending is rejected without disturbing the current block. Replay/Reconstruct obtain block information from the stored token.

`ready()` indicates that the predictor awaits feedback. `block_done()` retains completion until the next successful Evaluate or frame/reset. The `predictor_ready` and `block_completed` events wake waiters; callers must still check state and aggregate block completion into macroblock completion.

### Timing and reset

Incoming delay is consumed before the operation. Default reference, candidate, compare, replay and feedback latencies are 10/4/1/4/10 ns respectively; these are VP timing parameters. The socket handles one transaction at a time; concurrent transactions receive a generic error and may retry after the active transaction returns.

Active-low `reset_n` cancels work and old context. A rising `frame_enable` starts a new frame and clears context; holding enable high does not start another frame. After reset, drive enable low and then high to restart. Tokens contain generation and sequence and are invalidated by frame/reset.

An optional `h264::EpochExtension` connects the shared reset domain. An epoch stale at entry is rejected without disturbing the current valid block; cancellation during an active transaction invalidates the entire frame context. The caller coordinates reset/frame-enable even while the socket is idle, keeps the domain alive until the response, and waits for the old transaction to return (`transport_active()==false`) before reusing buffers.

## Standalone build and tests

Requires CMake 3.21 or later and a C++17 compiler. Run the following commands from the `CDC-VP` root in WSL/Linux.

### Core without SystemC

```sh
cmake -S components/h264/Intra-Prediction -B build/h264-intra-core \
  -DCMAKE_BUILD_TYPE=Debug -DH264_INTRA_BUILD_TLM=OFF -DBUILD_TESTING=ON
cmake --build build/h264-intra-core -j 4
ctest --test-dir build/h264-intra-core --output-on-failure
```

### Core and SystemC/TLM

SystemC 2.3.4 must match the project compiler and C++ ABI. Replace the paths below with your installation:

```sh
cmake -S components/h264/Intra-Prediction -B build/h264-intra-tlm \
  -DCMAKE_BUILD_TYPE=Debug -DH264_INTRA_BUILD_TLM=ON -DBUILD_TESTING=ON \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so
cmake --build build/h264-intra-tlm -j 4
ctest --test-dir build/h264-intra-tlm --output-on-failure
```

You may use `CMAKE_PREFIX_PATH` pointing to the `SystemCLanguage` 2.3.4 package, or `SYSTEMC_SOURCE_DIR`, instead of the include/library pair.

| Test group | Test count | Coverage |
|---|---:|---|
| Core | 2 | Prediction modes, neighbor context, SAD/SATD and regression |
| TLM contract | 1 | Evaluate/Replay/Reconstruct, tokens and completion |
| Shared epoch | 2 | Transaction cancellation during incoming delay and wait wake-up |
| Reset matrix | 1 | Reset during processing stages |
| TLM regression | 2 | Custom timing and zero latency |
| Reset boundaries | 2 | Reset around deadlines with normal and zero latency |

A core-only build registers **2 tests**; a TLM build registers **10 tests**. Verification results and logs are in [VALIDATION.md](VALIDATION.md).

## H.264 integration tests

[Prediction_Test](../tests/Prediction_Test/README.md) connects CMB DMA to Intra through `H264Arb` and `DmaTransport`, using the DMA subsystem arbiter/AXI bridge. The `cmb_intra` group checks one 16x16 picture with sixteen luma 4x4 blocks and two chroma 8x8 blocks on 32/64/128-bit buses. It uses identity reconstruction to test the prediction/feedback flow.

The [full codec test](../tests/Coding_Reconstruction_Test/FULL_PIPELINE_CODEC_REPORT.md) connects prediction with TQ/ITQ and reconstruction. Its adapter processes 32x32 YUV420 pictures, one I4x4 frame and three P frames; chroma U/V use DC mode. Luma passes through `TqTop` TLM, while chroma DC is processed by arithmetic classes in the adapter.

```sh
cmake -S components/h264 -B build/h264-vp \
  -DCMAKE_BUILD_TYPE=Debug -DH264_BUILD_TESTS=ON \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so
cmake --build build/h264-vp -j 4
ctest --test-dir build/h264-vp -R '^h264_prediction_cmb_intra_' --output-on-failure
ctest --test-dir build/h264-vp -L prediction --output-on-failure
ctest --test-dir build/h264-vp -R '^full_codec_' --output-on-failure
```

The `prediction` label includes **15 integration tests** for DMA, Intra, Inter and control. The H.264 component imports prediction libraries with their standalone suites disabled, so use the standalone build above for the complete Intra suite. The **36 `full_codec_*` tests** are registered only when CMake finds Python3 and FFmpeg; inspect them with `ctest --test-dir build/h264-vp -N -R '^full_codec_'`.

## Usage scope

The module supports progressive 8-bit YUV 4:2:0 with luma 4x4/16x16 and chroma 8x8. The caller selects partitions, organizes macroblocks, supplies slice/constrained-intra availability and connects TQ/ITQ, entropy and deblocking. `ImportReconstructed` requires the correct picture and context order; mode RAM stores the selected mode but does not yet emit MPM syntax.

Automatic partition RDO, High Profile luma 8x8, bit depths above 8 and MBAFF are not implemented. VP timing has not been compared cycle by cycle with RTL. The codec pipeline currently lives in a test adapter; Intra is not yet included in H.264 package install/export.
