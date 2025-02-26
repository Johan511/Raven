LTTNG commands to run

```sh
$ lttng create chunk_transfer_perf
$ lttng enable-event --userspace chunk_transfer_perf_lttng:object_recv 
$ lttng start
$ lttng stop
```

NOTE: We seem to make sure shell can call `sudo` because otherwise logs are not generated
