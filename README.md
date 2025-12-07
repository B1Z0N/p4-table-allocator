# p4-table-allocator

A toy compiler pass that assigns P4-style match-action tables to switch pipeline stages. It reads a JSON description and tries to fit the tables into the available SRAM and TCAM, respecting their dependencies.

## Background

In a match-action pipeline, each stage looks up packet fields and applies actions. A table may need a field written by an earlier table, and each stage has a fixed amount of memory. Those two constraints are what this program models.

For background, see the [RMT paper](https://web.stanford.edu/class/archive/cs/cs244/cs244.1156/papers/sdn-chip-sigcomm-2013.pdf) by Bosshart et al. (SIGCOMM 2013) and [Compiling Packet Programs to Reconfigurable Switches](https://www.usenix.org/conference/nsdi15/technical-sessions/presentation/jose) by Jose et al. (NSDI 2015). This project uses a much simpler model than either paper.

## Build and run

You need a C++20 compiler and CMake 3.24 or newer. CMake downloads nlohmann/json and Catch2 during the first build.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

```sh
./allocator examples/firewall.json
./allocator --algorithm greedy examples/firewall.json
./allocator --compare examples/heuristic_only.json
./allocator --json examples/oversized_table.json
```

The default algorithm is `constrained`. The `allocator` script runs `build/allocator`; if you use a different build directory, run its binary directly.

## Input

```json
{
  "pipeline": { "stages": 4, "sram_per_stage": 100, "tcam_per_stage": 20 },
  "tables": [
    { "name": "eth_check", "type": "exact", "size": 30 },
    { "name": "ipv4_lpm", "type": "tcam", "size": 15, "match_deps": ["eth_check"] },
    { "name": "acl", "type": "tcam", "size": 10, "action_deps": ["ipv4_lpm"] }
  ]
}
```

`exact` tables use SRAM. `tcam` tables use TCAM for wildcard or longest-prefix matches. Sizes and capacities are abstract memory units, not bytes.

A match dependency requires a later stage. An action dependency allows the same stage, but never an earlier one. Dependency lists can be omitted, and tables can appear in any order in the file. Stage numbering starts at zero.

Use unique table names, nonnegative integer sizes and capacities, and at least one stage. Unknown dependencies, cycles, and invalid input produce an error.

## Placement

Both algorithms choose a table whose dependencies have already been placed, then look for the earliest stage with enough memory.

**Greedy** picks ready tables in input order. **Constrained** first picks the table with the longest remaining chain of match dependencies. It breaks ties by size relative to the stage's memory capacity, then by input order. This helps avoid filling early stages with independent tables while a longer chain still needs room.

A table larger than one stage's capacity is split across consecutive stages. Its dependents wait for the final fragment. Tables small enough for one empty stage are kept whole, even if splitting could fill holes elsewhere.

Neither algorithm backtracks. A failed allocation doesn't prove that no valid placement exists.

## Examples and results

The forwarding, firewall, and load-balancer examples are small hand-written inputs. Two more examples, `heuristic_only` and `depth_reduction`, are deliberately arranged to show where ordering matters.

The numbers below are stages used. A dash means the allocation failed.

| Example | Greedy | Constrained |
| --- | ---: | ---: |
| basic_forwarding | 2 | 2 |
| depth_reduction | 3 | 2 |
| does_not_fit | — | — |
| firewall | 3 | 3 |
| heuristic_only | — | 3 |
| load_balancer | 3 | 3 |
| mixed_dependencies | 3 | 3 |
| oversized_table | 4 | 4 |

Greedy fits 6 of the 8 examples; constrained fits 7. On `depth_reduction`, constrained uses two stages instead of three. These examples show a couple of useful cases, but they're too small to say much about general performance. The heuristic can do worse on other inputs.

To rerun the comparison, use `./allocator --compare examples/heuristic_only.json`, or run `python3 scripts/results.py` for all examples. Python is only needed for that optional script.

## Output

The text report lists stages, table fragments, and SRAM/TCAM usage, followed by `fits` or `doesn't fit`. When a table can't be placed, the report keeps the earlier placements and names the table that failed.

`--json` gives the same information as JSON, including each table's first and last stage. Unplaced tables have `null` stage values. Combine it with `--compare` to get reports for both algorithms.

Exit codes: `0` means it fits, `1` means invalid input or arguments, and `2` means it doesn't fit. With `--compare`, `0` means at least one algorithm fits.

## Tests

Catch2 tests cover input validation, dependency ordering and cycles, memory limits, splitting, reports, and the examples where the heuristic improves placement.

## Limitations and next steps

This isn't a P4 frontend or a backend for a real switch. The memory model leaves out table width, memory block sizes, action resources, and PHV allocation. Splitting accounts for memory and ordering only; it doesn't implement the hardware logic needed to combine the fragments.

The next useful comparison would be an ILP allocator, to see how far these heuristics are from an optimal placement. Larger inputs and more realistic memory blocks would also help.

## License

MIT.
