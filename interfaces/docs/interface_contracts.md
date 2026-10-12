# Interface contracts v0.1

Coordinating owner: Huy Nguyen Huynh Quoc. Vinh Nguyen Thanh and Nguyen
review interfaces involving their producers/consumers. Pipeline assembly workflow: [full_pipeline_testbench.md](../../top/docs/full_pipeline_testbench.md).
The APIs below describe the running VP; real prediction/TQ/entropy APIs have not yet been finalized.

## Timing and concurrency

The model is loosely timed; blocking calls must run in `SC_THREAD`.
Targets consume input delay and return with zero delay; the bridge also consumes all
output annotations from replacement targets before reporting completion. The register target has
zero latency (individual AXI-Lite handshakes are not modeled).

The caller owns payloads and data buffers and keeps them alive until `b_transport` returns. Do not retain
payload pointers after return. DMI is not provided. Addresses are byte-based and little-endian; streaming_width
must be >= data_length; streaming wrap is unsupported. Byte enables repeat according to TLM rules,
with each element 00 or FF; byte-enable length must be >0 when a pointer is supplied.

## Register port

`EncoderVp::registers.socket`: 32-bit aligned accesses of exactly 4 bytes.
Invalid offset/alignment: ADDRESS_ERROR; writes to RO/live configuration: COMMAND_ERROR.
These are explicit VP policies for behavior not fully specified in the PDF.
Read-clear side effects occur once per accepted access with at least one enabled byte.
IRQ is driven by one SC_METHOD only, avoiding multiple writers when register accesses and the controller
execute in different processes.

## DMA / memory

Production uses DmaTransport::clients and DmaTransport::memory. H264Arb and
AxiMasterBridge are Vinh's cores. DmaClientExtension selects CMB/SW/NAL/DF;
untagged fixtures default to CMB. Priority is a configurable four-client
permutation, FIFO within a client; no legacy aging guarantee is added.
The grant covers the entire TLM request, including byte-enable runs.
The adapter enforces 32-bit addresses, non-wrapping streaming width and
00/FF repeating byte enables, then forwards enabled byte intervals to memory.
Vinh's bridge owns 32/64/128-bit lane/burst/4-KiB planning.
Incoming and downstream delays are consumed. Epoch is checked before and
after memory access; wait_idle is the fence after producers stop. Reset
cannot roll back writes already accepted by an external memory.
ControlRegs owns architectural LEN; the pipeline reports accepted NAL words
before waiting for DMA response. NalDma committed words serve drain validation.

## Functional adapter

`ProcessingIf::process(Macroblock)` is the current blocking interface. A macroblock contains
256 Y bytes + 64 U + 64 V, each plane in row order; x/y are macroblock coordinates and frame is the index
within the activation. `ProcessedBlock` contains reconstruction and output bytes.

The pipeline stub currently requires exactly 4 output bytes/MB. A complete encoder needs a new frame-executor
adapter with variable entropy length; do not force real entropy output into the checksum interface.
`FrameExecutorIf` lives in `interfaces/include/h264/frame_executor_if.h`. The top accepts a
`PipelineFactory` to create the pipeline at elaboration; the factory binds DMA clients to the arbiter.
`reset()` must not wait; `begin_activation()` initializes state; `execute()` returns newly
committed words (possibly 0 while buffering). `end_activation()` waits for all output/reference DMA
and returns the **total** word count for the activation. The controller neither appends EOS nor derives length from MB count.
Only the stub formatter adds its own EOS. Adapters must check the generation after every wait and
must not issue requests after finalization returns.

The mapping is provided in PDF Table 20-3 (pages 53-55). ControlRegs decodes SPARA0/1, DFCON and
SPARA2 from software-written values, retaining raw fields in FrameConfig. SequenceParameters
in HostDriver only helps encode registers; in EncoderVp, only nal_capacity_bytes is an allocation
configuration outside the registers. Constructor QP/frame count values do not override registers.
`nal_capacity_bytes` is a VP allocation contract, not a new register;
DMA clients must check capacity before writing. A controller length check after completion
cannot roll back an out-of-bounds write that has already occurred.

## Progress and picture scheduling

Adapters must implement syntax_requirements and abort_and_drain; see
`control/docs/recovery_and_syntax.md` from the root. FN/POC preflight runs before
begin_activation. Errors after begin clear BUSY only when the adapter confirms that producers have stopped and
all transactions have drained. A failed drain retains BUSY until reset.

The pipeline calls nal_words_accepted(generation,count) exactly once when NAL DMA accepts words.
STM_LEN increments immediately, even if responses are pending or later fail; normal/IRQ still waits for drain.
end_activation returns the total accepted words, which must match STM_LEN. Errors retain the accepted count.

FrameConfig contains completed_frames and activation_frames. Preload activations use at most
CMB_ON_MEM frames; frame-address mode uses one picture/base selected by the host. Validate the CMB region
against the working set, not total FMENC. Source slot and coding/display indices belong to
PictureTask. The controller retains sequence progress across CMB/NAL/batch updates, detects duplicate
display indices and calls execute_picture. N=1 uses an I/P schedule based on M; N>1 requires the adapter to override
picture_task with the released coding order. A missing schedule causes ERROR rather than an incorrect raster traversal.
