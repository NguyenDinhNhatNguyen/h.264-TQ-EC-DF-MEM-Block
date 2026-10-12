# Integration status

| Component | Connection | Status |
|---|---|---|
| ControlRegs / EncoderController | TLM register target + blocking frame executor | VP implemented/tested |
| H264Arb / AxiMasterBridge via DmaTransport | Multi-client target -> one memory initiator | VP implemented/tested |
| Reset | Epoch + delay release | VP implemented/tested |
| DDR / Host | Platform models | Implemented/tested |
| CMB / Reference / NAL clients | FramePipelineStub | Huy's fixture; awaiting Vinh task 05 |
| Intra / inter prediction | ProcessingStub | Awaiting Vinh task 06/07 |
| TQ / reconstruction / entropy | ProcessingStub | Awaiting Nguyen task 08/09/10 |
| Search-window / P/B list coordination | Not connected | Awaiting Vinh task 05/07 |
| Memory architecture / local RAM | Not connected | Awaiting Nguyen task 11; platform DDR is only a test environment |

Handoff points: `ProcessingIf` for block fixtures; `FrameExecutorIf` for a complete functional
pipeline; `DmaTransport::clients` for new DMA initiators. A functional model requires new golden data
and separate I/P/B, variable-length entropy and reference-completion tests.

Pipeline assembly details: [full_pipeline_testbench.md](full_pipeline_testbench.md). The pipeline is injected through a factory;
the controller receives the length from the adapter and does not append EOS itself. The full-pipeline harness exists;
the real backend is enabled only when a team adapter target and independent golden data are available.
