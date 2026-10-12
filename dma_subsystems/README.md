# H.264 DMA Subsystem

Author: VinhNT124

H.264 DMA model with a C++17 core and a SystemC 2.3.4/TLM-2.0 wrapper. The module transfers data between CMB, search windows, reconstructed frames and the NAL buffer through an arbiter and AXI memory bridge. The wrapper provides MMIO, shared DDR, IRQ and reset for CDC-VP platform integration.

## Functions

| Component | Role |
|---|---|
| CMB | Reads the current macroblock from the source frame and outputs Y16x16/U8x8/V8x8 |
| SW | Loads List0/List1 reference search windows and reuses overlapping regions |
| DF/REFM | Writes reconstructed or filtered pixels to reference frame memory |
| NAL | Writes output words and manages buffer capacity and stream length |
| Arbiter | Schedules DMA client requests and retains the grant until the transaction completes |
| AXI bridge | Plans bursts and handles byte lanes/strobes and responses on 32/64/128-bit buses |

## Structure and CMake targets

| Target | Role |
|---|---|
| `h264_dma` | Header-only core with no SystemC dependency |
| `h264_dma_tlm` | SystemC/TLM and MMIO wrapper |

The corresponding aliases are `cdc::components::h264_dma` and `cdc::components::h264_dma_tlm`. The core lives in `common/`, `dma/` and `mem/`; the wrapper in `tlm/`; standalone tests in `tb/` and `tlm/tests/`. The H.264 component registers and installs/exports these DMA targets.

## Transactions and memory

`DmaRequest.beats` counts internal transfers, each of `size` bytes; total payload is `beats * size`, independent of AXI data width. The arbiter stores copies of the request and write payload. Read completion returns the requested bytes in `DmaResponse.data`.

The bridge aligns addresses to bus beats, selects lanes/WSTRB and splits bursts at 4 KiB boundaries or the burst limit. Each write segment receives one B response; only strobed bytes are written. Successful segments remain committed if a later segment fails.

`service_one()` returns the client holding the grant, even while the transaction is pending; `NONE` means idle. `run(max_steps)` continues through stalls and reports an error if the limit is reached while work remains. `error()` retains the failure state until the subsystem is idle and the host calls `clear_error()`. `AxiBridgeConfig` delays are measured in service steps; the read FIFO holds up to 16 internal words, each up to 4 bytes.

Coded width/height must be positive multiples of 16, up to 1920x1088. A planar YUV420 frame needs `F = 3 * width * height / 2` bytes; REFM has three slots and needs `3F` bytes. Layout checks cover the physical window, bus-width alignment, capacity and overlap. `nal_capacity=0` derives capacity from the next region or the end of modeled DDR.

`DmaSubsystem` can use private DDR or accept `MemoryIf&`, `memory_base` and `memory_bytes` from the platform. The platform retains ownership of external memory. `memory()` returns the common interface; `ddr()` is available only when the subsystem owns private DDR.

## Client API

### CMB: current macroblock

`update_enable(true)` starts a frame on a low-to-high enable edge; holding enable high does not start another frame. `fetch_macroblock()` accepts at most one pending tile. `pixels()` becomes ready after all reads succeed and returns planar Y256/U64/V64 samples.

In `WorkingSet` mode, call `select_working_frame(slot)` before enabling. The controller supplies source slots in coding order; `RegisterBases.cmb_frames` and source capacity determine the allocated frame count.

### SW: reference search window

`set_ref_slot(list, slot, picture_tag)` selects and retags references separately for each list. `fill_window()` becomes ready only after the entire Y/U/V region is loaded; successfully read overlapping regions are reused when moving to another macroblock or row.

The window starts at the macroblock coordinates and is clipped to the coded frame; `SwPlane` provides its actual origin/extent. Consumers handle the search origin and boundary padding. `invalidate()` clears both residency and reference selection, so slots must be selected again. Use references only after the DF producer confirms frame completion.

### DF: reconstructed frame output

The producer supplies reconstructed `MacroblockPixels`. Example of writing one macroblock in bypass mode, where `vp` is a `DmaSubsystem` and `reconstructed` contains the input pixels:

```cpp
vp.df().latch_df_enable(false);
vp.df().set_ref_slot(1);
vp.df().on_sofm();
vp.df().schedule_macroblock(mb_x, mb_y, reconstructed, false);
vp.df().dma_fmdone();  // Call after submitting all work for the frame.
vp.df().consume_done();
vp.run();
// The reference is ready when vp.df().frame_complete() returns true.
```

When filtering is enabled, pass a filtered tile with `already_filtered=true`, or register `set_filter(callback)` for unfiltered tiles. The callback runs vertically before horizontally, and DMA writes Y/U/V only after it completes. Missing pixel input or a required filter producer is rejected.

`dma_fmdone(frame_id)` and `frame_complete(frame_id)` track completion per frame, preventing a delayed done from completing a new frame incorrectly. The legacy `latch_dis_idc(0/1)` API corresponds to DF enable and differs from the syntax field `disable_deblocking_filter_idc`.

### NAL: output buffer

Each flush snapshots accepted words; multiple flushes append within one activation. Pointers/counts update only after successful responses, and capacity is checked before enqueueing.

`on_disable()` requires no pending output, reloads the pointer and preserves `STM_LEN` for host reads. The next activation follows disable and then `begin_activation()`. `WordByteOrder` configures byte order, defaulting to little-endian. `accept_eos()` writes bytes `00 00 01 0B`.

## SystemC/TLM connections

The wrapper provides these ports:

| Port | Role |
|---|---|
| `target_socket` | MMIO target accepting local offsets after address decoding |
| `master_socket` | DMA initiator issuing physical addresses |
| `reset_n` | Reset active-low |
| `irq` | Level interrupt connected to the interrupt controller |

Map MMIO with `bus_router.add_target()`, connect host and DMA master to the same router, and share one DDR mapping. `H264DmaOptions.memory_base/memory_bytes` must match the physical RAM window.

The [platform test](tlm/tests/h264_dma_platform.cpp) uses this example configuration:

| Region | Address / size |
|---|---|
| MMIO | `0x10080000`, 4 KiB |
| Shared DDR | `0x80000000`, 1 MiB |
| CMB | `0x80001000` |
| REFM | `0x80010000` |
| NAL | `0x80030000`, capacity 4 KiB |

These addresses are for testing; the integrating platform configures its own memory map. The host loads source data and reads REFM/NAL through `b_transport` on the shared bus.

## MMIO registers

| Offset | Register | Contents |
|---|---|---|
| `0x00` | SCON | ECE bit 0, GIE bit 1; ECE 0-to-1 latches configuration and starts processing |
| `0x04` | STAT | NORMAL bit 18, ERROR bit 17, BUSY bit 16, FMCNT bits 15:0 |
| `0x08` | FMSIZE | Coded width bits 26:16, height bits 10:0 |
| `0x0C` | DFCON | Enable bit 0; signed alpha/beta offsets for the producer |
| `0x10` | SPARA0 | QP, CMB mode and syntax/GOP fields; reset `0x12C` |
| `0x14` | SPARA1 | Working-set allocation bits 31:25, syntax widths, FMENC bits 15:0 |
| `0x18` | SPARA2 | Crop/delta fields |
| `0x1C` | REFM | Physical base of three reference slots |
| `0x20` | NAL | Physical output base |
| `0x24` | CMB | Physical source base |
| `0x28` | STM_LEN | Number of successfully written 32-bit words |

Configuration writes require ECE=0 and BUSY=0. A new start resets stream count/pointer; disabling after completion preserves `STM_LEN`. GIE controls IRQ. Reading STAT or STM_LEN acknowledges IRQ while retaining status/count fields; `transport_dbg` is read-only and does not acknowledge.

MMIO uses aligned 32-bit transactions, little-endian register bytes and `00/FF` byte enables. Invalid size/alignment returns BURST_ERROR; writes to read-only registers return COMMAND_ERROR; invalid reserved bits or configuration phase returns GENERIC_ERROR. Undefined offsets default to ADDRESS_ERROR; the `ReadZero` option permits zero reads.

Valid QP is 0..51; FMENC and working-set allocation must be nonzero. Cropping must retain a valid image region. Invalid configuration terminates the activation with ERROR.

## Workload, completion and reset

Install `set_workload_provider(callback)` before activation, or enqueue `H264ActivationWorkload` while ECE=0/BUSY=0. Queued workloads take priority; each start consumes one workload, even if configuration is invalid. Reset clears the queue and retains installed callbacks/filters.

A workload supplies all macroblocks of each coded picture, each coordinate exactly once, source/reference slots, List0/List1 selections, picture tags, reconstructed/filtered pixels and NAL words. Frame count must not exceed FMENC. CMB mode 1 processes one picture per activation; mode 0 selects source slots in the working set. FMCNT counts completed pictures in the activation; the codec producer determines coding order and reference validity.

`H264MacroblockJob.reconstructed` contains Y256/U64/V64 samples. DF bypass still requires reconstructed pixels; `replay_source_for_bypass=true` is only for DMA-only tests with DF disabled. SW cache is invalidated between pictures; B-pictures require both reference lists to be resident. NAL words append across frames, with optional EOS.

NORMAL/IRQ occur only after REFM and NAL responses succeed. A downstream non-OK response or core error sets ERROR. `max_service_steps` bounds a stalled core; the target/harness needs its own watchdog if `b_transport` does not return. Default register latency is 10 ns and service latency is 8 ns.

Clearing ECE while BUSY completes the active transaction, stops queued work, then sets ERROR. Active-low reset clears registers/status/IRQ/count and invalidates stale completion. Downstream-accepted bytes may already have affected DDR; stale completion after reset does not issue an IRQ for a new activation.

## Standalone build and tests

Requires CMake 3.16 or later and a C++17 compiler. Run the following commands from the `CDC-VP` root in WSL/Linux.

### Core without SystemC

```sh
cmake -S components/h264/dma_subsystems -B build/h264-dma-core \
  -DCMAKE_BUILD_TYPE=Debug -DH264_BUILD_TLM=OFF -DBUILD_TESTING=ON
cmake --build build/h264-dma-core -j 4
ctest --test-dir build/h264-dma-core --output-on-failure
```

### Core and SystemC/TLM

SystemC 2.3.4 must match the project compiler and C++ ABI. Replace the prefix below with your installation path:

```sh
cmake -S components/h264/dma_subsystems -B build/h264-dma-tlm \
  -DCMAKE_BUILD_TYPE=Debug -DH264_BUILD_TLM=ON -DBUILD_TESTING=ON \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4
cmake --build build/h264-dma-tlm -j 4
ctest --test-dir build/h264-dma-tlm --output-on-failure
```

| Test | Coverage |
|---|---|
| `h264_dma_contracts` | CMB/SW/DF/NAL, arbiter, AXI bridge, capacity, errors and memory lifetime |
| `h264_dma_shared_bus` | MMIO, DDR, IRQ and reset on a 32-bit bus |
| `h264_dma_shared_bus_64` | The same contract on a 64-bit bus |
| `h264_dma_shared_bus_128` | The same contract on a 128-bit bus |
| `h264_dma_response_timeout` | Service-step limit for a stalled channel |

A core-only build registers **1 test** with **272 assertions**; a TLM build registers **5 tests**. Verification results, sanitizer evidence and logs are recorded in [VALIDATION.md](VALIDATION.md).

## H.264 integration tests

[Prediction_Test](../tests/Prediction_Test/README.md) contains 15 tests connecting CMB/SW/DF/NAL with Intra/Inter and control/reset on 32/64/128-bit buses. This group uses `H264Arb` and [DmaTransport](../top/include/h264/top/dma_transport.h) with the core arbiter/AXI bridge. MMIO wrapper tests run in the separate DMA suite above.

The [full codec test](../tests/Coding_Reconstruction_Test/FULL_PIPELINE_CODEC_REPORT.md) connects DMA/prediction with TQ/ITQ, CAVLC/NAL and reconstruction. Its adapter processes 32x32 YUV420 pictures, four I-P-P-P frames, one List0 reference and P16x16 with zero MV.

```sh
cmake -S components/h264 -B build/h264-vp \
  -DCMAKE_BUILD_TYPE=Debug -DH264_BUILD_TESTS=ON \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so
cmake --build build/h264-vp -j 4
ctest --test-dir build/h264-vp \
  -R '^h264_dma_(contracts|shared_bus|response_timeout)' --output-on-failure
ctest --test-dir build/h264-vp -L prediction --output-on-failure
ctest --test-dir build/h264-vp -R '^full_codec_' --output-on-failure
```

The **36 `full_codec_*` tests** are registered only when CMake finds Python3 and FFmpeg; inspect the list with `ctest --test-dir build/h264-vp -N -R '^full_codec_'`.

## Usage scope

This module supports functional DMA and driver integration with shared DDR, MMIO and IRQ/reset. The codec producer handles scheduling, prediction, transform, entropy/NAL generation and deblocking arithmetic. Service-step/TLM timing has not been compared with RTL cycle/pin timing; the platform test uses a SystemC host driver.

Layout, configuration and client contracts are detailed in [DMA_SUBSYSTEM_SPEC.md](DMA_SUBSYSTEM_SPEC.md).
