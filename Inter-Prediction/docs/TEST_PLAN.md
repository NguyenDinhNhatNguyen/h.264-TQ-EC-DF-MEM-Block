# Test coverage and remaining work

Based on [spec mapping](SPEC_MAPPING.md); evidence is in [VALIDATION](../VALIDATION.md).

## Implemented tests

| CTest name | Executed coverage |
|---|---|
| h264_inter_sad_cache_search | 41 positions × 2 seeds × 128 trials; max SAD/wide rate/overflow; cache isolation/immutability; all partitions × 12 sources, independent IME/FME/MC oracle; EEI/ties/stale modes |
| h264_inter_interpolation_oracle | 3 seeds x 6 patterns; 16 luma phases over coordinates -2..17; 64 chroma phases at interior/borders; direct 2D/table oracle and six-tap/diagonal anchors |
| h264_inter_limits | 40 geometry/MV cases: 16x16, QCIF, 720p, 1920x1088, 4096x4096; extreme signed MV and inward FME; 4096 integer/64 fractional candidates; invalid geometry/plane/MC/search, atomic refill and retag during search |
| h264_inter_sw_dma_32/64/128 | Actual SwDma/H264Arb/AxiMasterBridge/DdrModel; 48×32, P/B/P × 6 MBs; overlap, successful reads, RRESP fault/retry, stale outstanding reads, 4KiB split, MC vs DDR |
| h264_inter_contract_normal | 8+8 columns, unrelated L1/busy calls, delayed U, P/B/P, fractional winner/phase/EEI, independent transform order, repeat Peek/bad Accept/final stall |
| h264_inter_contract_zero | Same scenarios as normal, with zero service latency |
| h264_inter_contract_shared/local/disable/retag | 7 sites: absent SW, IME service, FME service, MC sample service, U support, Accept service, Accept incoming; wake on the correct cancellation event, no stale output/done |
| h264_inter_contract_incoming | Source snapshot before delay, benign notifications retain deadlines; shared cancellation during incoming search/Peek/Accept |
| h264_inter_contract_stale | Stale-at-entry retains the owner; invalid token/address/command/byte-enable/pointer/streaming/missing-extension; Peek/Accept before Commit; duplicate Commit; reset rearm and optional EpochExtension/local-only waits |
| h264_inter_contract_partitions/partitions-zero | Per profile: 3 seeds x P/B x 41 positions = 246 cases; fractional winner, EEI/MVD, luma/chroma oracle and independent transform order; stalls/bad Accept/final handshake |
| h264_inter_contract_boundaries/boundary-zero | Per profile: 4 cancellation kinds x 12 sites x {-1,0,+1} ps = 144 cases; response deadlines, no stale output/done and actor completion; normal and zero service timing |

Core-only has 3 tests; TLM without DMA has 15; the full suite has 18. Contract/integration
timeouts are 30 seconds; limits timeout is 60 seconds. The SystemC driver must actually complete;
starvation is not counted as PASS. Assertion counts are not coverage percentages for the entire HAS.

Geometry tests use sparse corner fixtures to check clamping and arithmetic;
they do not encode all these resolutions in full. Cancellation exactly at a deadline
allows valid SystemC sequential orderings and checks result consistency,
without imposing RTL priorities absent from the specification. Gcov for seven production .cpp files:
412/414 lines and 627/831 branch outcomes executed; see VALIDATION and raw evidence.

## Open tests

- SISLAB golden data for exact candidate priority/rate, autonomous partition/mode selection,
  MVP, P-skip/B-direct and bi/weighted prediction.
- Actual TQ/entropy/DF adapters and multi-frame reconstructed feedback.
- QCIF byte comparison, PSNR and 601-frame qualification.
- Full-frame/resolution and search-window workloads following the actual encoder policy;
  the current geometry/MV regression covers boundary cases, not the entire input domain.
- Every simultaneous-event ordering and per-cycle RTL reset priority; the deadline
  regression currently checks the sites/profiles described above.
- RTL retiming/cycle/filter-internal assertions; current checks cover stage/candidate/
  sample metadata at transaction level.
- Complete performance reports for resolution, I/P/B, search, AXI, DF, clocks, cycles/stalls.
- Unexecuted branch outcomes, including compiler exception/unwind paths;
  line coverage does not imply 100% specification coverage.

Full encoder signoff requires separate evidence for the open items.
