#undef LTTNG_UST_TRACEPOINT_PROVIDER
#define LTTNG_UST_TRACEPOINT_PROVIDER chunk_transfer_perf_lttng

#undef LTTNG_UST_TRACEPOINT_INCLUDE
#define LTTNG_UST_TRACEPOINT_INCLUDE "chunk_transfer_perf_lttng.h"

#if !defined(_chunk_transfer_perf_lttng) || defined(LTTNG_UST_TRACEPOINT_HEADER_MULTI_READ)
#define _chunk_transfer_perf_lttng

#include <lttng/tracepoint.h>
#include <stdint.h>
#include <sys/types.h>

LTTNG_UST_TRACEPOINT_EVENT(
chunk_transfer_perf_lttng,
object_recv,
LTTNG_UST_TP_ARGS(pid_t, pid, uint64_t, timeDiff, uint64_t, groupId, uint64_t, objectId, uint64_t, objectSize),
LTTNG_UST_TP_FIELDS(lttng_ust_field_integer(pid_t, pid, pid)
                    lttng_ust_field_integer(uint64_t, timeDiff, timeDiff)
                    lttng_ust_field_integer(uint64_t, groupId, groupId)
                    lttng_ust_field_integer(uint64_t, objectId, objectId)
                    lttng_ust_field_integer(uint64_t, objectSize, objectSize)))


LTTNG_UST_TRACEPOINT_EVENT(
chunk_transfer_perf_lttng,
netem,
LTTNG_UST_TP_ARGS(double, lossPercentage, double, kBitRate, double, delayMs, double, delayJitter),
LTTNG_UST_TP_FIELDS(lttng_ust_field_float(double, lossPercentage, lossPercentage)
                    lttng_ust_field_float(double, kBitRate, kBitRate)
                    lttng_ust_field_float(double, delayMs, delayMs)
                    lttng_ust_field_float(double, delayJitter, delayJitter)))

#endif /* _chunk_transfer_perf_lttng */

#include <lttng/tracepoint-event.h>
