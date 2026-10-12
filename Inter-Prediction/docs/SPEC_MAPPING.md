# Inter Prediction specification mapping

Source: [SISLAB HAS v2.1.11](../../SISLAB_H264_AVC_HAS_Spec_Detailed_v2.1.11.md)
§8.1–8.6, §6.2; [DMA spec](../../dma_subsystems/DMA_SUBSYSTEM_SPEC.md) §2;
[H.264 interpolation](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-H.264-200503-S%21%21PDF-E&lang=e&type=items) §8.4.2.2.

INTER-* are internal tracking identifiers. Verified VP refers to the implementation/tests within
the VP scope below; it does not imply full RTL/codec signoff. Evidence is in
[VALIDATION](../VALIDATION.md).

| ID | Requirement / source | Implementation | Verification / status |
|---|---|---|---|
| INTER-ARCH-01 | IME/FME/MC/EEI/IPC/CMB/dual-list, section 8.1 | src/*, current-MB snapshot IPC | Core + normal/zero; VP pipeline, not an RTL pipeline |
| INTER-IME-01 | 16 primitive, 7 shapes, §8.2 | ime/sad_tree.cpp | Core 41 positions/direct pixel oracle |
| INTER-IME-02 | Cost/candidate/list/coordinate alignment, §8.2 | CandidateResult, progress | Randomized core search + TLM B fractional winner |
| INTER-IME-03 | Worst SAD + rate width, §8.2 | uint64_t, checked_cost | 65280 SAD, wide rate, overflow reject; RTL width TBD |
| INTER-FME-01 | IME seed, §8.3 | fme/search.cpp | Core seed/candidates/winner + TLM FME cancel |
| INTER-FME-02 | Six-tap/quarter, signed intermediates, §8.3 | fme/interpolation.cpp | Oracle 16 phases, 3 seeds, borders/patterns |
| INTER-FME-03 | Chroma bilinear/scaled MV, §8.3 | chroma_eighth, 4:2:0 MC | Oracle 64 phases/negative coordinates |
| INTER-FME-04 | Compare order/metadata, §8.3 | Caller order, first minimum | Intentional ties + fractional list/MVD/phase; RTL priority TBD |
| INTER-STALL-01 | Hold phase/address/metadata/valid, sections 6.2/8.3 | wait_support/Progress/held output | Row-start, delayed U, unrelated list; stage-level VP |
| INTER-MC-01 | Committed list/slot/partition/MV, §8.4 | Mode/incarnation checks | Core partitions, TLM Commit/Peek/Accept/retag |
| INTER-MC-02 | Transform order/plane/block/phase/last, §8.4 | mc/motion_compensation.cpp | Core 7 shapes; TLM 41 positions × P/B × 3 seeds, normal/zero; independent order/oracle |
| INTER-MC-03 | Done after the final handshake, section 8.4 | Peek holds, Accept advances | Normal/zero final-sample stalls/bad/repeated acceptance |
| INTER-CACHE-01 | Independent tag/support residency, §6.2/8.5 | cache/reference_cache.cpp | Sparse/tag/plane/list isolation, dual-list gating |
| INTER-CACHE-02 | Row-start/two columns, §6.2/8.5 | Sparse gate + external SW adapter | TLM 8+8 columns, actual SW raster/overlap |
| INTER-CACHE-03 | B startup/PB/list/picture switch, §8.5 | B preflight/retag | TLM P/B/P + actual SW 3 pictures; uni-pred B |
| INTER-DMA-01 | Successful reads before ready, DMA §2 | refill adapter contract | SW/arb/bridge/DDR 32/64/128, fault/retry/stale reads |
| INTER-EEI-01 | Motion/partition metadata, section 8.1 | eei/motion_syntax.cpp | MV/MVD/ref checked; caller MVP, no entropy encoding yet |
| INTER-RESET-01 | Reset/frame/stale work, §13 + VP contract | IPC generation/shared epoch | Shared/local/disable/retag/incoming/stale; 288 deadline ±1ps cases across normal/zero profiles |
| INTER-PERF-01 | No fixed latency, complete report, section 8.6 | Configurable timings | Simulation time only; benchmark remains open |
| INTER-POLICY-01 | Released generics, Appendix C | No policy engine yet | Open: Algorithm Spec/RTL + quality qualification |

## Explicit limitations

HAS section 1.3 does not replace the Algorithm Specification/RTL. The VP accepts explicit candidates,
rate and MVP, selecting single-partition uni-prediction. It does not infer the search pattern,
partition selection, SISLAB priority/rate formula or skip/direct policy from the architecture.

Appendix C baseline: FORCED_P_SKIP_G=1, B_RATE_COST_G=1, B_ABS_MVD_COST_G=0,
B_DIRECT_MATCH_G=1, FORCED_B_DIRECT_G=0. There is no policy/configuration engine for these
settings yet. Bi/weighted prediction, neighbor-derived MVP and complete entropy packaging
remain open.

The filter has an AVC oracle; SISLAB Algorithm Spec/RTL golden vectors are still needed to
confirm this specific encoder. Port names and per-cycle FSM/cache layout remain TBD.
HAS-V-SW-01 is partially covered by dual-list and actual SW integration;
48x32/3 pictures do not replace 601-frame qualification. TQ/DF/full encoder have not been run.
