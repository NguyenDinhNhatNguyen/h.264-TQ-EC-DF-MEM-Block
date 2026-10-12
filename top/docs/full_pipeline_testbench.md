# Full-pipeline testbench and replacing stubs with released modules

## Run immediately

Run the following commands from components/h264.

```powershell
cmake --build build/vp -j 8
ctest --test-dir build/vp --output-on-failure
./build/vp/bin/full_pipeline_tb.exe stub tests/vectors/full_pipeline/stub.cfg build/full_pipeline_stub
```

This test runs host -> registers/controller -> pipeline -> DMA -> DDR -> IRQ -> output,
with two activations, two frames per activation, a 128-bit bus and 50 ns DDR latency. Stub output is only a
checksum, not an H.264 bitstream. pipeline_contract_tb uses a different test adapter:
variable word count, buffering until final drain and no EOS, demonstrating that the controller does not
depend on the stub length formula. The test adapter is not selected in the real executable.

## When Vinh and Nguyen release their modules

1. Both contributors export CMake libraries and public .h headers, with passing unit tests.
2. Huy creates an assembly adapter implementing FrameExecutorIf and the make_released_pipeline factory.
   The assembly owns prediction/DMA/coding/memory modules and connects each DMA initiator
   to the factory-provided DmaTransport::clients. Do not reuse FramePipelineStub for the real backend.
3. Adapter CMake exports `h264_released_pipeline`, linking the team targets and h264_top.
   The sample adapter has been removed from this package. When real IP is available, supply a separate adapter directory through H264_RELEASED_PIPELINE_DIR; its contract is described below.
4. Supply planar YUV input and independent golden data matching the configuration; create case.cfg.
5. Configure:

```powershell
cmake -S . -B build/vp -DH264_RELEASED_PIPELINE_DIR="C:/path/to/team_adapter" -DH264_FULL_PIPELINE_CASE="C:/path/to/case.cfg"
cmake --build build/vp -j 8
ctest --test-dir build/vp -R full_pipeline_real --output-on-failure
./build/vp/bin/full_pipeline_tb.exe real C:/path/to/case.cfg build/real_output
```

Without an adapter, the real backend returns an error; it does not fall back to the stub. With an adapter but
no case, the real executable builds, but CTest does not register real qualification.
Successful linking does not qualify the real modules. The bundled vectors are for fixtures only.

## Adapter contract

- `reset()`: called from SC_METHOD; must not wait; clears/invalidates state and preserves the epoch.
- `begin_activation(config, generation)`: configures modules from the snapshot; resets counters/output base.
- `execute(config, generation, frame)`: processes a frame and returns newly committed words; may return 0
  if entropy output is buffered until flush. Huy updates the frame counter after each return.
- `picture_task`/`execute_picture`: transfers coding/display indices, source slots and I/P/B types.
  Adapters with B-pictures must supply the released schedule; the fallback supports only N=1.
- The NAL DMA callback must call `nal_words_accepted(generation,count)` when it accepts packed words,
  rather than waiting for finalization to update STM_LEN. Finalization must still wait for memory responses.
- `end_activation(config, generation, committed_words)`: flushes the formatter, waits for the **final NAL and
  reference responses**, and returns total activation words including padding/EOS according to the release.
- Do not exceed nal_capacity_bytes; failures throw std::runtime_error. Old reset epochs must not
  commit or update new state. Check epochs after waits.
- Config carries raw and decoded DFCON/SPARA0-2 fields according to PDF Table 20-3. The host encodes
  QP/frame count into registers; the VP constructor no longer determines these values.
  Cases with raw spara0/spara1 overrides replace configuration; golden data must match.

## Case format

Text `key=value`, one key per line, comments starting with `#`, and no spaces around `=`.
Vector paths are relative to the case directory. `.hex` contains whitespace-separated hex bytes;
all other extensions are read as raw binary. Missing files/golden data or unknown keys cause errors.

```text
width=176
height=144
frames=1
qp=26
bus_width=32
latency_ns=40
timeout_ns=10000000
nal_capacity=65536
repeat=1
source=input.yuv
expected=golden.bin
```

source must contain at least frames * coded_width * coded_height * 3/2 bytes. expected contains exactly
STM_LEN*4 bytes, including final word padding/EOS according to the contract. Do not update golden data using DUT
output. Repeat uses the same vectors by default; `source.1`, `expected.1`, etc. override activations
indexed from 0 (for example, to continue GOP state). P/B cases require correctly ordered coding inputs.

Optional: dfcon, spara0, spara1, spara2 (decimal/0xhex integers), expected_ref and reference_offset
(byte offset in REFM). expected_ref compares a specific region rather than guessing the useful slot.

## Checks and limitations

The harness checks normal/error/BUSY/frame count, timeout, IRQ acknowledgment, STM_LEN, exact bytes,
two 64-byte guards around NAL, valid write regions, pending responses at IRQ and DMA activity
within 100 ns after drain. Actual output is saved as activation_N.bin before golden comparison if an
output-directory is supplied. The first mismatched byte is reported. Optional reference golden data checks reconstruction.

The DDR fixture is 64 MiB: CMB=0x1000, REFM=0x01000000, NAL=0x03000000; NAL is limited to 8 MiB;
source/reference regions must not overlap. This is a test layout, not a production address requirement.
Change bus_width/latency_ns in the case to run the 32/64/128 matrix and release-required latencies.
The harness does not automatically run a decoder/PSNR or verify internal backpressure for every block;
Vinh/Nguyen unit tests and decoder validation remain separate gates. Real modules are not yet available,
so only the harness/adapter contract is currently qualified, not the full H.264 pipeline.
