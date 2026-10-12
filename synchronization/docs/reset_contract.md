# Reset / synchronization

Owner: Huy. Public API: `h264/sync/reset_domain.h` (header-only).
ResetDomain increments its generation on assertion and remains active until release. EpochExtension carries
the generation over TLM to DDR. StickyCompletion stores state; its event is only used for wake-up.

Do not introduce a separate clock for every module in the LT VP. EncoderVp releases reset after 2 * period
(default: 8 ns/cycle). The rstn signal is active-low; IRQ updates follow SystemC delta-cycle scheduling.
Reset regression: `h264_reset`. Recovery and drain contracts: [recovery_and_syntax.md](../../control/docs/recovery_and_syntax.md).
