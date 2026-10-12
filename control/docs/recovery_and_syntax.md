# Error drain and FN/POC preflight

## Required adapter contract

FrameExecutorIf adds two pure virtual methods; existing adapters must be updated before building.

`syntax_requirements(config)` must not wait, issue DMA or run producers. It returns the required
frame_num/POC widths for the actual schedule and the configured widths decoded by the adapter.
The controller checks configured >= required and a maximum of 32 bits before begin_activation.
Do not use one frame_count formula for both FN/POC or assume a minus-4 encoding.
If the release cannot determine the requirements, the adapter must throw runtime_error rather than return zero.

The checksum processing and contract fixtures return {0,0,0,0} because they do not emit H.264 syntax. This
is an intentional fixture-only exemption; it does not validate FN/POC configuration for actual H.264 output.
A real adapter must derive requirements from frame_num, POC, reset/wrap and coding-order rules.

`abort_and_drain(generation)` stops new producers/requests, then waits for responses from ALL clients with
submitted work, including transactions queued in the arbiter. Payloads must remain alive until return.
Return true only when no writers remain pending and producers cannot issue further requests.
No arbiter changes are required: producers stop in the adapter while the arbiter drains its accepted queue.

## Controller state

- Preflight failure before begin: no work has been issued; report ERROR normally.
- Failure in begin/execute/finalize: keep BUSY and call abort_and_drain. Finish with ERROR/IRQ only after true.
- Drain returns false or throws: keep BUSY, do not signal a completion IRQ, and wait for reset.
  A host timeout retains the buffers; expiration of the grace period alone does not permit reuse.
- Reset during drain: the adapter wakes on reset.changed without modifying the new generation state;
  the controller discards stale completion. Old DMA requests use EpochExtension to prevent commits after reset.
- STM_LEN retains the accepted-word count; errors do not turn partial output into a successful bitstream.

The current stub uses blocking transport in a single thread, so no background requests remain after unwinding;
its abort_and_drain returns true. Do not copy this implementation into a concurrent adapter.

## Test

recovery_syntax_tb has a DMA worker with a 200 ns delayed write; the control thread reports an error after 10 ns.
It checks BUSY/IRQ/base-write permissions during drain, data completion, reset during drain,
false/throwing drains retaining BUSY, and sufficient versus one-bit-short FN/POC widths before DMA starts.
The literal 4/5-bit policy in the test checks the mechanism, not the released register mapping.
