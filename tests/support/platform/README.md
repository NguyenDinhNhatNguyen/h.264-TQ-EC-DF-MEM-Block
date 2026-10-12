# Platform models

`host/`: HostDriver issues register/memory transactions over TLM. The caller runs in an SC_THREAD.
`memory/`: DdrMemory stores bytes, with configurable latency, fault regions and transaction tracing.

This models the environment outside the encoder IP, rather than an algorithm task. CMake target: h264_platform.
Host and DMA bind two initiators to the DDR multi-target socket. Memory serializes accesses and commits after latency.
The in_flight/active_address diagnostics are for test observation only, not the IP software interface.

Owner: Huy Nguyen Huynh Quoc (test infrastructure). Chapter 12 Memory Architecture belongs to
Nguyen in the team-provided Memory Architecture module: local memory, layout/stride,
reference slots and read latency/collision contracts. This DDR model only performs external
memory transactions; it does not choose slot rotation or replace Nguyen's internal RAMs.
