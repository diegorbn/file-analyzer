# AGENTS.md — Benchmark Development Instructions

## Project Context

This repository contains a Linux File Analyzer written in C.

The original application uses dynamically loaded shared libraries (`.so`) to analyze TXT, CSV, and LOG files.

The `benchmark/` directory is an independent experiment comparing the original dynamically linked application against a statically linked version of the same application.

Read `TASK.md` for complete implementation requirements.

## 1. Scope

All new implementation work belongs inside `benchmark/`.

You may inspect existing source files outside this directory, but must not modify them.

In particular, preserve:

- `../main.c`
- `../analyzers/plugin_api.h`
- `../analyzers/txt_plugin.c`
- `../analyzers/csv_plugin.c`
- `../analyzers/log_plugin.c`
- `../info.md`
- `../example/`
- `../plugins/`, if present

The original application must remain functional and unchanged.

Do not introduce unrelated refactoring.

## 2. Required Architecture

The benchmark contains three distinct components.

### Dynamic application

Build the existing application from its original sources.

It must continue using `dlopen()`, `dlsym()`, and `dlclose()`.

Preserve the original interactive menu and plugin behavior.

### Static application

Create an independent copy of the original application.

Copy and reuse its analyzer source code.

Compile the analyzers into `.a` libraries and link them into a separate executable.

Remove runtime shared-library loading from this version.

Register statically linked analyzer functions directly.

Preserve the original parsing algorithms and user-facing behavior wherever possible.

### Benchmark runner

Create a separate program that launches and measures both executables.

It must not implement file analysis itself.

It must not substitute synthetic calculations for actual TXT and CSV processing.

Prefer C and POSIX APIs for the runner.

## 3. Existing Analyzer Interface

The original plugin API exports:

- `plugin_name()`
- `plugin_extension()`
- `info()`
- `analyze(const char *, FILE *)`

All three analyzer sources export these names.

When linking them statically into one executable, resolve the symbol collisions through build-time renaming or an equivalent approach.

The renamed symbols must retain the exact original function behavior.

Do not modify the parsing algorithms merely to make them link.

The static application's analyzer code must be traceable to the existing implementations.

## 4. Fairness Requirements

Use the same:

- Analyzer algorithms.
- Input datasets.
- File traversal order.
- Compiler optimization level.
- Reporting behavior.
- Input/output redirection.
- Number of processed files.
- Benchmark environment.

Do not compare one version processing files in batches against another version processing one file per process.

Both applications must process an entire workload during one execution.

The runner should automate the existing menu through standard input rather than benchmarking manual interaction.

Avoid unnecessary differences between the two application versions.

## 5. Linking Requirements

The static version must link analyzer modules using `.a` archives.

It is not necessary to statically link libc.

Do not use a globally static system executable as the comparison baseline.

The dynamic version must retain the original runtime plugin-loading implementation.

Do not simplify both variants into artificial benchmark-only programs.

Any unavoidable architectural differences must be documented.

## 6. Measurement Rules

The primary metric is full application execution time.

Use monotonic wall-clock timing and multiple runs.

Do not generate datasets or compile code inside timed sections.

Do not print per-file reports to an interactive terminal while benchmarking.

Measure both applications under equivalent conditions.

Record warm-up and measured runs separately.

Alternate execution order where practical to reduce systematic bias.

Preserve raw measurements; do not report only averages.

Never invent, estimate, or fabricate benchmark results.

## 7. Correctness Before Performance

Both applications must produce equivalent file-analysis results.

Verify actual analyzer reports, not just execution times.

Test TXT, CSV, and LOG functionality.

Fail correctness validation if the applications disagree.

Do not label performance results valid when correctness checks fail.

Handle missing files, invalid input, missing plugins, and abnormal process termination.

Do not silently ignore failures.

## 8. Resource Measurement

Measure executable and library sizes.

Collect peak RSS if supported by the environment.

Explain that RSS is not equivalent to unique physical memory consumption and that shared pages can be counted in multiple processes.

Do not assume a shared library uses less total memory in every situation.

Document filesystem cache effects and benchmark limitations.

## 9. Build and Runtime Environment

Target Linux, particularly Ubuntu under WSL.

Prefer standard C, GCC, Make, and POSIX APIs.

Avoid unnecessary external dependencies.

Keep generated files inside `benchmark/`.

Use local `.gitignore` rules for generated datasets, libraries, binaries, temporary files, and measurements as appropriate.

The project should support straightforward build, test, run, and clean commands.

Use appropriate compiler warnings and error checks.

## 10. Implementation Quality

Keep source code readable and well organized.

Use clear naming conventions.

Check return values from filesystem, process, memory-allocation, and dynamic-loading operations.

Release allocated resources and close file descriptors.

Avoid undefined behavior.

Do not call a dynamically loaded function after its library has been unloaded.

Do not introduce unrelated frameworks, databases, graphical interfaces, or unnecessary features.

Favor maintainability and reproducibility over unnecessary complexity.

## 11. Reporting

Create a README explaining all commands and experimental assumptions.

At completion, provide:

- Files added or changed.
- Build commands.
- Benchmark execution commands.
- Tests executed and their outcomes.
- Actual measured results, if available.
- Differences and limitations observed.
- Any unfinished work.

Distinguish factual measurements from hypotheses.

Do not automatically conclude that static linking is faster or that dynamic linking is more memory-efficient.

## 12. Success Criteria

The final implementation must provide:

1. The original dynamically loaded File Analyzer.
2. A separate statically linked version with equivalent analysis functionality.
3. A standalone benchmark runner.
4. Reproducible TXT and CSV workloads.
5. Verified equivalent analyzer outputs.
6. Repeatable performance measurements.
7. CSV result exports.
8. Clear documentation.

All of this must be achieved without changing the original application's source code.

Follow `TASK.md` and complete the implementation, build, tests, and documentation.
