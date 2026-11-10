# File Analyzer development instructions

Read `TASK.md` for the experiment requirements. The original `main.c`, `analyzers/`, `example/`, and legacy benchmark artifacts are a recoverable baseline. Preserve analyzer algorithms and the existing plugin ABI; `info()` returns `const char *`.

Demo and benchmark use `common/registry.c` for analyzer acquisition, dispatch, and cleanup, and compile the same source files in `analyzers/`. Keep processing paths and compiler optimization comparable; the only acquisition distinction is linked function pointers versus `dlopen`/`dlsym`. The demo builds TXT, CSV and LOG; the benchmark builds only TXT and CSV. Use `tools/plugins.py` to manage each application’s separate staging and active directories. Keep plugin staging, installation, and process-local handles distinct. Never time compilation or plugin deployment as process execution. Preserve prior datasets and results, avoid destructive cleanup, and do not present failed or unvalidated measurements as valid.

Run `make test` after changes and consult `README.md` for build, demonstration, and benchmark procedures. The original benchmark implementation is retained under `benchmark/legacy/`.
