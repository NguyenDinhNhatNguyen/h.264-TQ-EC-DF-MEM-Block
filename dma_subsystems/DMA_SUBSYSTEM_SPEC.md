# H.264 DMA Subsystem — Functional VP Spec

Summary of the C++17/SystemC/TLM implementation contract, referencing
[SISLAB HAS v2.1.11](../SISLAB_H264_AVC_HAS_Spec_Detailed_v2.1.11.md),
sections 3, 5-6, 11.4, 12 and Appendix B.

## 1. Functions and interfaces

| Block | Function |
|---|---|
| CMB DMA | Reads Y16x16/U8x8/V8x8 macroblocks from the source frame |
| SW DMA | Reads search windows with separate List0/List1 caches and readiness |
| DF DMA | Writes reconstructed/filtered Y/U/V to the selected REFM slot |
| NAL DMA | Writes consecutive NAL words; updates length after successful responses |
| Arbiter/bridge | Retains request/payload/owner until completion; handles lanes, bursts and errors |

The core uses `MemoryIf`; the `h264_dma_tlm` wrapper provides MMIO `target_socket`, DMA
`master_socket`, input `reset_n` and output `irq`. CPU and DMA share DDR.

## 2. Memory contract

- Physical addresses are 32-bit; DMA data width is configurable as 32/64/128 bits.
- Coded dimensions are positive multiples of 16, up to 1920x1088; format is planar YUV 4:2:0.
  With `F = 3*Wc*Hc/2`: Y offset 0, U offset `Wc*Hc`, V offset `5*Wc*Hc/4`.
- CMB allocation contains 1..127 frames; REFM contains 3 slots at `base + slot*F`.
  NAL has a separate capacity. Bases must be nonzero and aligned to the bus width;
  regions must fit within the shared-memory window without overlap.
- `DmaRequest.beats` is an internal count; byte count = `beats * size`.
  The bridge splits external bursts at 4 KiB boundaries and writes only addressed bytes.
- CMB/SW become ready only after all required reads succeed; DF/NAL complete only after all
  required write responses succeed. B-pictures require both reference lists to be resident.

## 3. MMIO

Offsets local, aligned 32-bit access; register bytes little-endian.

| Offset | Register | Meaning |
|---|---|---|
| `0x00` | SCON | ECE bit 0: enable/start; GIE bit 1: gate IRQ |
| `0x04` | STAT | NORMAL 18, ERROR 17, BUSY 16, FMCNT 15:0; read clears IRQ |
| `0x08` | FMSIZE | Width 26:16, coded height 10:0 |
| `0x0C` | DFCON | Filter enable bit 0, signed alpha/beta offsets |
| `0x10` | SPARA0 | QP 0..51, CMB mode, syntax/GOP fields; reset `0x12C` |
| `0x14` | SPARA1 | CMB frame allocation 31:25, syntax fields, FMENC 15:0 nonzero |
| `0x18` | SPARA2 | Bottom crop 16:6, signed QP delta 5:0 |
| `0x1C` | REFM | Physical reference base |
| `0x20` | NAL | Physical output base |
| `0x24` | CMB | Physical source base |
| `0x28` | STM_LEN | Committed 32-bit word count; read clears IRQ |

Configuration writes require ECE=0/BUSY=0; STAT/STM_LEN are read-only. Reading these
registers preserves status/count and only acknowledges IRQ; debug reads do not acknowledge.
Invalid accesses return TLM errors according to VP policy; undefined offsets default to
ADDRESS_ERROR, with a ReadZero option. These responses do not establish RTL behavior.

## 4. Activation and completion

1. Disable, program registers and supply a workload/provider.
2. The ECE 0-to-1 edge latches configuration, clears old status/count/IRQ and sets BUSY.
   Holding ECE=1 does not restart processing.
3. The workload supplies all macroblocks, each coordinate once, source/reference
   slots, reference selections, reconstructed pixels and NAL words.
   Frame count must not exceed FMENC; CMB mode 1 processes one picture per activation.
4. NORMAL is set only after all REFM/NAL responses succeed; FMCNT counts pictures
   completed in the activation. Completion/error asserts IRQ if GIE=1.
5. The host reads STAT and drains `4*STM_LEN` bytes; disable and re-enable to continue.
   Disabling after completion preserves STM_LEN.

Transfer/configuration/workload failures or exhausted service budgets produce ERROR, not NORMAL.
Clearing ECE while BUSY waits for the accepted memory call to return, stops further work, then sets ERROR.
Active-low reset clears registers/status/count/IRQ, with SPARA0 reset to `0x12C`;
stale completions are discarded. Accepted DDR accesses are not rolled back.

## 5. Model limitations

This is a functional DMA VP. The producer supplies codec/GOP scheduling, NAL generation and deblocking
arithmetic. DF accepts filtered tiles or callbacks executed vertically
then horizontally; CMB-to-REFM copying requires explicit bypass replay selection.
NAL defaults to little-endian and supports BigEndian; EOS bytes are `00 00 01 0B`.
TLM latency is consumed before completion; AXI pin/cycle timing is not modeled.
The service budget cannot preempt a target whose `b_transport` does not return.

Build/test instructions and detailed API: [README.md](README.md).
