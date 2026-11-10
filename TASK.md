
# TASK.md — Separate standalone demos from the benchmark

## Mission

Refactor the existing **File Analyzer** C repository into two clearly separated, top-level project areas:

- **`demo/`** — Two **genuinely independent**, small, manually compiled C applications: one statically linked to its analyzers and one dynamically loading `.so` plugins. They are intended for a live university presentation, **not for performance measurement**.
- **`benchmark/`** — The existing, automated static-vs-dynamic benchmark. **Keep its behavior, measurements, workload, results, and tests intact.** Make it self-contained so it does not depend on any files from `demo/` or another root-level source directory.

**This is a targeted repository restructuring, not a redesign of the working benchmark.** Prefer clarity and ease of demonstration over deduplication. In particular, **code duplication between the two demo applications is explicitly acceptable and preferred over introducing shared dependencies**.

Inspect the repository before editing. Preserve existing functionality and user data. Implement the changes; do not stop after writing a plan.

## 1. Required final directory layout

Aim for this straightforward layout (small additional files are fine):

```text
file-analyzer/
├── demo/
│   ├── static/
│   │   ├── main.c
│   │   ├── txt.c
│   │   ├── csv.c
│   │   ├── log.c
│   │   ├── analyzers.h          # Optional; local to static/
│   │   ├── examples/            # Local sample TXT, CSV and LOG files
│   │   └── README.md
│   │
│   └── dynamic/
│       ├── main.c
│       ├── plugin_api.h         # Local plugin ABI declarations
│       ├── txt_plugin.c
│       ├── csv_plugin.c
│       ├── log_plugin.c
│       ├── plugins/             # Active .so files; initially empty
│       ├── examples/            # Local sample TXT, CSV and LOG files
│       └── README.md
│
├── benchmark/
│   ├── src/                    # Existing benchmark C application
│   ├── analyzers/              # Own analyzer source + API
│   ├── common/                 # Own registry and build rules
│   ├── runner/                 # Existing process-level C runner
│   ├── tools/                  # benchmark.py, generate.py, plugins.py, etc.
│   ├── tests/                  # Benchmark/functional tests
│   ├── datasets/
│   ├── results/
│   ├── plugins-built/
│   ├── plugins-active/
│   ├── bin/
│   ├── build/
│   ├── Makefile
│   └── README.md               # Optional; document benchmarks here
│
├── README.md                   # Short index; links to both areas
├── TASK.md
└── .gitignore
```

Interpretation:

- The **root has only two application subtrees**: `benchmark/` and `demo/`. Root documentation, `.gitignore`, and Git metadata are fine. Do not leave active root-level `common/`, `analyzers/`, `tools/`, `tests/`, `plugins/`, `main.c`, or old executable directories.
- **No Makefiles whatsoever under `demo/`**, including `demo/Makefile`, `demo/static/Makefile`, and `demo/dynamic/Makefile`.
- **No root Makefile is necessary.** Keep the benchmark's Makefile inside `benchmark/`.
- `demo/static/` must build using **only files within `demo/static/`**.
- `demo/dynamic/` must build using **only files within `demo/dynamic/`**; its plugin sources and API must also be local.
- Neither demo may include headers using `../` or `../../`, link to objects outside its directory, invoke Python tooling under `benchmark/`, depend on `benchmark/` at runtime, or require a generated shared module from anywhere else.
- The benchmark must **not** import or compile anything from either demo directory. Move/copy its existing root-level dependencies into `benchmark/` and adjust paths as necessary.
- There is no requirement that the two demo implementations share code. Self-contained copies are desirable here.

## 2. `demo/static/` — entirely standalone C application

Build a simple, interactive static-analyzer version by adapting the existing functional application and analyzer implementations.

### Compilation must work manually

From `demo/static/`:

```bash
gcc -std=c11 -O2 -Wall -Wextra -o file-analyzer main.c txt.c csv.c log.c
./file-analyzer
```

**These commands must work exactly as written** on Linux/WSL with GCC, without Make, Python scripts, generated headers, external repository includes, or external project object files.

Requirements:

1. `main.c`, `txt.c`, `csv.c`, and `log.c` all live directly inside `demo/static/`.
2. Provide local declarations (`analyzers.h` if useful) for the analyzer functions.
3. Ensure analyzer functions have distinct linker symbols, e.g. `txt_analyze`, `csv_analyze`, `log_analyze` and corresponding metadata functions. Do **not** define three identically named global `analyze` functions, which would cause linker collisions.
4. Compile analyzer code directly into the executable. Do not use `dlopen`, `dlsym`, `.so` files, `ar`, or `.a` archives.
5. Keep the terminal interface simple: list available analyzers, analyze a file, show information/help, and quit. The three analyzers must be available immediately, even with no `.so` files anywhere on disk.
6. Preserve the current TXT/CSV/LOG analysis behavior and useful output. Prefer adapting the existing known-working algorithms instead of inventing replacements.
7. Bundle small example files under this directory so the demo can be run from this directory alone.
8. Don't print benchmark timing or memory metrics.

## 3. `demo/dynamic/` — entirely standalone C + `.so` application

This version exists to **visibly demonstrate runtime extensibility** with shared libraries. It must not rely on `demo/static/`, `benchmark/`, or root-level source files.

### Manual GCC commands must work

From `demo/dynamic/`:

```bash
# Build the dynamic application's executable:
gcc -std=c11 -O2 -Wall -Wextra -o file-analyzer main.c -ldl

# Compile each plugin separately:
gcc -std=c11 -O2 -Wall -Wextra -fPIC -shared -o txt_plugin.so txt_plugin.c
gcc -std=c11 -O2 -Wall -Wextra -fPIC -shared -o csv_plugin.so csv_plugin.c
gcc -std=c11 -O2 -Wall -Wextra -fPIC -shared -o log_plugin.so log_plugin.c

# Start the application with an empty active plugin directory:
mkdir -p plugins
./file-analyzer
```

**These commands must work exactly as written** with no Makefiles or Python tooling. `main.c` must contain or directly use local logic for plugin discovery, registry maintenance, loading, dispatch, and cleanup. If additional local `.c` files become necessary, simplify the architecture or adjust the documented one-line GCC command accordingly; the priority is a truly standalone, easy-to-compile demo.

### Demo behavior

- Default plugin directory: `./plugins` relative to `demo/dynamic/`; optionally accept an explicit directory argument. Document that the expected working directory is `demo/dynamic/`.
- With `plugins/` empty, the program starts successfully and reports no analyzers installed/loaded.
- The program **does not need to consult Python** to find or load plugins. It scans `plugins/` and loads `.so` files itself with `dlopen()` and resolves required symbols with `dlsym()`.
- Preserve a clear plugin ABI such as:

  ```c
  const char *plugin_name(void);
  const char *plugin_extension(void);
  const char *info(void);  // Match the actual existing ABI; inspect before editing.
  int analyze(const char *file_path, FILE *output);
  ```

  **Check the actual current `plugin_api.h` and sources.** Use the existing ABI consistently rather than changing a working signature based on the example above.
- Supported interactive commands: at minimum `help`, `list`, `refresh`, `analyze PATH`, `unload .EXT`, and `quit` (or equivalent intuitive commands).
- `refresh` discovers newly placed `.so` files **while the same process remains running**. It must not repeatedly register/load duplicate plugins.
- TXT can be loaded and used first; CSV can then be added, refreshed, and used without restarting. TXT must still work afterward.
- LOG should also be supported for the demo.
- `unload` must safely remove the registration and call `dlclose()` for the correct handle. Do not call previously obtained function pointers after unload.
- Clear error handling for unknown extensions, absent plugin files, invalid libraries, missing exported symbols, and duplicate plugins. No crashes.
- Do not automatically reload/update an already-loaded plugin merely because its on-disk `.so` file changed; treat updating a loaded library as an explicit, carefully documented operation. Removing an on-disk `.so` is not the same as calling `dlclose()`.
- No benchmark timers, CSV statistics, or process-runner dependency.

### Required live-demo sequence

Open terminal A in `demo/dynamic/`:

```bash
./file-analyzer
# list  -> no analyzers
```

Open terminal B in the **same directory**, with the application still running in terminal A:

```bash
cp txt_plugin.so plugins/
```

Back in terminal A:

```text
refresh
list
analyze examples/ejemplo.txt
```

Then in terminal B:

```bash
cp csv_plugin.so plugins/
```

Back in terminal A:

```text
refresh
list
analyze examples/ejemplo.csv
analyze examples/ejemplo.txt
```

All must succeed **without restarting or recompiling `file-analyzer`**. The plugin `.so` files are built ahead of time and installed by ordinary `cp` commands. The demo does not require Python, Make or any cross-directory scripts. Include examples of optionally adding LOG and explicitly unloading/removing plugins in the local README.

Use ordinary copying only for *first-time installation* of a plugin not already mapped by the running process. Never write or overwrite the bytes of a mapped library in place; document a safe procedure for updates if supported.

## 4. `benchmark/` — preserve the working experiment

**The benchmark's correctness and its exported experimental results matter far more than aesthetics.** Preserve the validated benchmark and move its source dependencies under `benchmark/` so it is self-contained.

Current intended sequence for **each fresh benchmark process**:

```text
Start process
    -> acquire TXT analyzer
    -> analyze 10 TXT files
    -> acquire CSV analyzer
    -> analyze 10 CSV files
    -> cleanup and exit
```

The **static benchmark** selects linked TXT/CSV analyzer functions; the **dynamic benchmark** uses `dlopen()`/`dlsym()` to load TXT at the TXT phase and CSV only after TXT processing has completed. Each plugin is loaded at most once per process.

Do not conflate the demo's interactive install workflow with the benchmark. The benchmark controller prepares the required `.so` files on disk **before timed execution**. The benchmark application loads them in C during its phases. No interaction with Python while the process runs, no plugin compilation or file copying inside the timed window, and no reloading for each processed file.

### Keep exactly the current fixed benchmark configuration

- **10 TXT files and 10 CSV files**, each exactly **524,288 bytes (512 KiB)**.
- 20 files total, approximately 10 MiB of data.
- Default **30 measured trials per variant**, **5 warm-ups per variant**, with configurable `--trials` and `--warmups`.
- Alternating execution order; one fresh process per trial.
- Same analyzer sources, algorithms, function-pointer dispatch, input order, compiler optimization settings, and reporting policy for the static and dynamic benchmark versions, except where linking necessarily differs.
- Static benchmark links **TXT and CSV only**, not LOG.
- `CLOCK_MONOTONIC` / external C process runner for end-to-end elapsed time and `wait4()` / `ru_maxrss` for Peak RSS, as currently implemented.
- Preserve separate TXT and CSV analyzer acquisition time, processing times, output correctness checks, library/executable sizes, run metadata, warm-up handling, and errors.
- Preserve `raw.csv`, `summary.csv`, `warmups.csv`, and `metadata.json` (existing names, columns, units, and calculations if at all possible).
- Preserve the run-specific output directories and plugin-state restoration logic.
- **Never delete, overwrite, or silently regenerate existing datasets/results.** In particular, preserve prior benchmark measurements; do not mix them with new measurements after architecture changes.
- Retain the existing `benchmark/Makefile` and benchmark-oriented build automation. This restriction against Makefiles applies to the **demo only**.

### Make the benchmark self-contained without redesigning it

Move the relevant previous root-level `analyzers/`, `common/`, project plugin-management script, and project-wide benchmark tests into `benchmark/` where required:

- `benchmark/analyzers/`
- `benchmark/common/` (including build rules)
- `benchmark/tools/plugins.py`
- `benchmark/tests/`

Then adjust includes, Makefile paths, Python imports, metadata source hashes, subprocess working directories, and test paths. **Avoid changing the benchmark's core processing algorithm or measurement logic.** The benchmark should build and execute with `demo/` entirely absent.

Suggested commands, from inside `benchmark/` (make them work and document the final exact commands):

```bash
make all
make test
python3 tools/generate.py
python3 tools/benchmark.py --trials 30 --warmups 5
```

If the existing dataset generator is already correctly handling the fixed workload, retain it. `make test` must not silently erase previously recorded results.

## 5. Tests and acceptance criteria

After the restructuring, perform all of these checks. Fix failures, rather than weakening or removing tests.

### Independent-demo checks

1. In a copy containing **only `demo/static/`**, the exact static GCC command compiles and the executable analyzes TXT, CSV, and LOG samples.
2. In a copy containing **only `demo/dynamic/`**, the exact GCC commands build the executable and all three `.so` files.
3. The dynamic demo starts with an empty `plugins/` directory; `list` shows none.
4. While that same process remains alive: copy TXT; `refresh`; analyze TXT; copy CSV; `refresh`; analyze CSV; analyze TXT again. Verify all outputs.
5. Test LOG activation, duplicate refresh, missing/invalid plugin, unknown extension, unload, and safe cleanup.
6. Confirm neither demo contains includes or scripts referencing `../common`, `../analyzers`, `../tools`, `benchmark/`, or the other demo variant.
7. Confirm there is **no demo Makefile** and no Make invocation needed for these tests.

### Benchmark regression checks

1. Build the benchmark from **inside `benchmark/`** using its own Makefile, with no root shared source or root Makefile.
2. Run its existing full test suite and verify every passing behavior still passes.
3. Verify the static benchmark contains TXT and CSV but no LOG analyzer.
4. Verify the loading sequence and exactly 10 TXT / 10 CSV analyses per trial.
5. Verify output equality between static and dynamic, including stable file ordering.
6. Verify plugin activation/reset/restoration and no Python coordination during the timed C process.
7. Verify new process per trial, alternated variant order, proper warm-up exclusion, and consistent statistics.
8. Run a short smoke benchmark (e.g., two measured trials and one warm-up per variant), without overwriting any historical CSVs.
9. Confirm every benchmark timing field retains its prior meaning and units and that the existing raw/summary schema remains compatible.
10. Confirm the benchmark continues to function if the entire `demo/` tree is temporarily moved out of the working copy.

### Cleanup checks

- Remove **obsolete** root-level application sources, old root `plugins/`, root `common/`, root `analyzers/`, root `tools/`, root `tests/`, root compiled binaries, and root Makefile from the active structure **only after verifying they are no longer needed**.
- Preserve historical source safely in Git history or a clearly marked archive if necessary. Do not delete untracked user work or recorded benchmark data.
- Keep `benchmark/legacy/` only if intentionally preserving that reference implementation, clearly label it as inactive, and make sure normal builds/tests do not depend on it.
- Update `.gitignore` for executables, `.o`, generated `.so`, temporary reports, etc. Keep placeholder files where empty active-plugin directories need to exist after cloning.
- Update root `README.md` as a short index, and put hands-on manual GCC commands into the README of **each** demo version; document benchmark commands inside `benchmark/README.md`.

## 6. Engineering constraints

- **Don't create new abstraction layers to avoid demo code duplication.** The user explicitly wants two independent, explainable demo programs.
- For the demo, prefer ordinary C files and a few clear functions over frameworks or generalized build infrastructure.
- For the benchmark, avoid refactoring established logic unrelated to the dependency relocation. The goal is to preserve trustworthy measurements.
- Do not add benchmark timing to either demo application.
- Do not optimize one benchmark variant to produce a preferred winner; do not insert artificial delays.
- `dlopen()` / `dlsym()` / `dlclose()` must happen in the dynamic C application, **not in Python**.
- Do not claim that removing an installed `.so` file unloads it from a still-running process.
- Do not report performance conclusions based on a smoke run.

## 7. Work plan and final report

1. Inspect all relevant source files, tests, scripts, Git status, and build relationships. Present a concise migration plan.
2. Make the benchmark self-contained in `benchmark/` with the smallest feasible set of changes. Run its tests.
3. Create fully local `demo/static/` from working source and verify the exact single-command GCC build.
4. Create fully local `demo/dynamic/` with manual `.so` compilation and verify the two-terminal hot-install workflow.
5. Remove/archive genuinely obsolete active files safely, update docs and `.gitignore`.
6. Rebuild and rerun independent-demo tests, full benchmark regression tests, and a short benchmark smoke run.

At the end, report:

- Final directory tree.
- Exact GCC commands for each demo (including all three `.so` files).
- Exact benchmark build, test, and final-run commands.
- A quick two-terminal demonstration script.
- Files moved, copied, removed or preserved.
- Tests executed and their actual outcomes.
- Any remaining dependencies, limitations, or unverified behavior.

**Success means:** I can copy `demo/static/` onto its own, compile it with one `gcc` command and run it; I can copy `demo/dynamic/` onto its own, compile its C executable and `.so` files with plain `gcc`, add TXT then CSV while the process remains running, and see the new capabilities; and I can enter `benchmark/` and run the existing rigorous benchmark **without any dependency on either demo**.
