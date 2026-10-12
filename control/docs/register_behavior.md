# Register behavior — PDF table 20-3

Source: SISLAB H.264/AVC Hardware Architecture Specification v2.1.11, Table 20-3,
pages 53-55; activation/completion in sections 4.3-4.5 and Table 20-6 on page 57.
The earlier claim that the PDF lacks SPARA bit mappings was incorrect: the appendix had been overlooked.

| Register | Applicable bit fields |
|---|---|
| SCON | ECE[0], GIE[1] |
| STAT | NORMAL[18], ERROR[17], BUSY[16], FMCNT[15:0] |
| FMSIZE | WIDTH[26:16], HEIGHT[10:0] |
| DFCON | ALPHA signed[10:6], BETA signed[5:1], ENABLE[0] |
| SPARA0 | INIT_QP[26:21], PKFM[20:12], CMB_FRAME_MODE[9], COMPAT_ENABLE[8], GOP_N[7:4], GOP_M[3:0] |
| SPARA1 | CMB_ON_MEM[31:25], LOG2_FG[24], LOG2_FN[23:20], LOG2_POC[19:16], FMENC[15:0] |
| SPARA2 | CROP_BOTTOM[16:6], signed SLICE_QP_DELTA[5:0] |
| REFM/NAL/CMB | Physical base[31:0] |
| STM_LEN | Accepted NAL words[31:0], activation-relative |

Reset: SPARA0=0x12c (COMPAT=1, N=2, M=12); all other registers are zero.
PKFM is stored but does not affect scheduling. Reserved bits read as zero after masking.
ControlRegs::config decodes register fields rather than taking QP/FMENC from the constructor.
HostDriver::program_sequence encodes the same mapping. Filter/syntax/crop fields are
forwarded to the real modules; the stub does not implement filtering/cropping/H.264 algorithms.

STAT/STM_LEN reads acknowledge IRQ. GIE masks the output; clearing GIE does not clear a latched event;
a new start or clearing read clears it. Completion while GIE=0 does not create a new event.
Returning an error for invalid register operations is VP policy: the PDF describes zero/error behavior
as RTL-dependent. Accesses must be 32-bit aligned, and byte enables are honored.

## Working set and sequence

FMENC is the total frame count; CMB_ON_MEM is the frame count in the working set/activation (1..127).
Preload processes min(remaining,CMB_ON_MEM); frame-address mode processes one picture at REG_CMB.
FMCNT persists across activations. STM_LEN resets on rising enable and is retained after disable for draining.
The host updates CMB/NAL/SPARA1 only while disabled and BUSY is clear. Changing SPARA1 batch bits
does not reset the sequence counter. Starting after FMENC has been reached begins a new sequence.

VP policy for a new sequence: changing FMSIZE/REFM/filter/crop, active SPARA0 fields,
or the low 25 bits of SPARA1 restarts the count. Rewriting the same value or changing PKFM does not.
This policy needs release-driver comparison for update flows outside standard reactivation.

## NAL progress / error

The pipeline calls nal_words_accepted when NAL DMA accepts a word. STM_LEN increments before the response;
finalization must wait for output/reference responses and report a total matching the counter. Errors retain
the accepted count without treating it as a valid bitstream. Old generations must not update the counter.

GOP_N=1: one I picture every M pictures, with P pictures otherwise; coding/display indices advance with the sequence.
GOP_N>1: the release adapter supplies PictureTask; a missing schedule causes ERROR rather than guessing B-picture order.
The controller checks duplicate display indices, coding indices and source slots across activations.
Actual source-slot mapping and reference lifetimes remain the responsibility of the team adapter/modules.
