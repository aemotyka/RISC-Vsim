# Instruction-fetch performance

Replacing repeated file scans with indexed reads reduced simulator runtime by **31.13×** on the 8,192-increment workload.

## Change

Previously, each fetch rewound the input and scanned to the requested word. A straight-line run of N instructions therefore scanned O(N²) records.

The new loader counts records, allocates an array, and parses each word once. Fetches use a bounds-checked index: O(N) setup and O(1) per fetch, with four bytes of array storage per input word. The pipeline logic is unchanged.

## Measurements

| Increments | File scan median | Indexed median | Speedup |
| ---: | ---: | ---: | ---: |
| 512 | 24.536 ms | 12.634 ms | 1.94× |
| 2,048 | 135.870 ms | 22.516 ms | 6.03× |
| 8,192 | 1,339.576 ms | 43.031 ms | 31.13× |

One warmup, then five measured runs per size on the same machine. Apple clang 21.0.0; macOS 26.5.2; reported architecture x86_64. Flags: `-O2 -DNDEBUG -Wall -Wextra -Werror`.

Each workload jumps over the fixed data region, increments x5 repeatedly, then halts. Timing includes process startup, input validation/loading, simulation, and summary writing; compilation is excluded. Cycle traces are disabled with `T2147483647:2147483647`.

All registers, data memory, cycles, stalls, and forwarding counts match between versions. All six sanitizer test suites pass, including the unchanged complete Fibonacci trace.

Raw runs, environment, input/source hashes, and full summaries: [baseline.json](../benchmarks/results/baseline.json), [indexed.json](../benchmarks/results/indexed.json).

## Repeating the comparison

Use revision `6aa03459225884305696183dc4d59d8d28ae7b4c` for the baseline with its harness, and `9bc5ebde8725007fe36e40fd6e242957cdd1f6f5` for indexed fetch. The original measured baseline simulator source is `ef3ea0aaf78ceda40ffb4efb5933b8feb0315a62`; its C/header contents are unchanged in the baseline harness revision.

With the corresponding source checked out, run:

```bash
python3 benchmarks/fetch.py --label baseline-repeat --revision 6aa03459225884305696183dc4d59d8d28ae7b4c --output benchmarks/results/baseline-repeat.json
```

```bash
python3 benchmarks/fetch.py --label indexed-repeat --revision 9bc5ebde8725007fe36e40fd6e242957cdd1f6f5 --output benchmarks/results/indexed-repeat.json
```

These are wall-clock results for this workload and machine, not faster simulated CPU cycles. Startup and other simulator work remain in the measurements; the speedup is not a measurement of isolated fetch latency.
