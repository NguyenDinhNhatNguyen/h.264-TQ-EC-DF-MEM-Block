# DMA transaction model

The bridge accepts byte-range requests and validates command, 32-bit address, data length and byte enables.
For each segment:

```text
lane = address % bus_bytes
bytes = min(remaining, 4096 - address % 4096, max_beats * bus_bytes - lane)
beats = ceil((lane + bytes) / bus_bytes)
external_address = address - lane
```

The external buffer contains beats*bus_bytes bytes; bytes outside the request are disabled. Repeating byte-enable
indices use the original request offset, so splitting a request does not shift their phase.
Reads copy back enabled bytes only. The arbiter locks the entire request, including bridge segmentation.

DDR responds after latency and commit; the bridge also consumes the annotated delay of a replacement
target. Errors stop the request while preserving earlier committed segments. No automatic retry is performed.

This is an LT lane/burst abstraction; it does not model per-cycle AW/W/R, legacy beat scheduling
or a 16-word FIFO. Review the general limitations before comparing RTL throughput/protocol behavior.

The arbiter uses a priority queue with aging: lower ranks are selected first, with arrival order within a rank;
requests bypassed eight times receive priority. Default ranks are equal; a release adapter may call
set_priority(binding_index,rank). Active transactions are not preempted, even when responses
are delayed. Aging is a VP policy to ensure progress, not an assumed RTL priority table.
PDF section 5.3 does not list a specific priority order.
