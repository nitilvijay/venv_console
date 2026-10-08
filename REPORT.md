# Project Report: Python Virtual Environment Scanner

## Goal
Python developers create a venv (`python -m venv`) for almost every project. Over time these pile up across the home directory and quietly use gigabytes of disk. This tool finds every venv and shows its Python version, its installed packages and its real disk usage.

**Detection idea:** every venv has a `pyvenv.cfg` at its root. From the `version = X.Y.Z` line in that file, the tool builds the path `lib/pythonX.Y/site-packages`, then lists the packages there and measures its size.

---

## Evolution

### Phase 1: Python prototype (`venv_mng.py`)
- Runs `find ~ -name pyvenv.cfg` as a subprocess, reads the version with a regex and builds the `site-packages` path.
- Lists the packages and merges names like `django` / `django-5.0.dist-info` (`unify_pkg_names`).
- Gets the size by running `du -sm` as a subprocess.
- Path construction later generalised so it also works for older Python versions.

### Phase 2: Python UIs (`cli_tool.py`, `cli_textual.py`, `web_ui.py`)
- Tried `rich`, then moved fully to **Textual**: tables, progress bar, package search, keyboard navigation.
- Used `SortedList` to keep venvs ordered by size as they are found.
- Tried a **Streamlit** web UI.
- The bottleneck stayed the same: one serial `find` plus one `du` subprocess per venv.

### Phase 3: Parallel scanning experiment (`parallel_scan.cpp`)
- Compared a serial recursive directory walk with an **OpenMP task-parallel** one (`#pragma omp task` for each subdirectory).
- Checked that both walks counted the same files and directories, and measured the speedup.

### Phase 4: C++ port (`venv_mng_parallel.cpp`)
- Replaced `find` with the OpenMP task scanner (`parallel` + `single` region to start it, `critical` block to collect results).
- Analysed venvs concurrently with `#pragma omp parallel for reduction(+:total_size) schedule(dynamic)`. Dynamic scheduling suits venvs, whose sizes vary a lot.
- Replaced `du` with native `lstat` and summed `st_blocks` (allocated blocks, which includes fragmentation), so the numbers match `du -sm`.

### Phase 5: ncurses TUI (`venv_tui.cpp`)
- Same engine as Phase 4, with a split-pane terminal UI: venvs sorted by size on the left, a searchable package list on the right, total size in the footer.

### Phase 6: Scanner optimisations (current)
| Change | Why |
| :--- | :--- |
| Removed `#pragma omp taskwait` (CLI and TUI) | The implicit barrier at the end of the parallel region already waits for all tasks; parent tasks no longer block on their children. |
| Stop descending once `pyvenv.cfg` is found | A venv's own tree (`site-packages` can hold thousands of folders) never contains another project venv, so walking it was wasted work. |
| Skip symlinks during the scan | Prevents infinite loops and stops the same venv being counted twice through a symlinked path. This also skips the venv's `lib64 -> lib` link. |
| Count hard links once (by `st_dev` + `st_ino`) | Same rule as `du`: a file with several hard links is counted once. |

**Result on the dev machine:** scan + analysis time went from **~0.65 s to ~0.45 s** (about 30% faster). The same 30 venvs were found, and every size matches `du -sm` exactly (27114 MB total).

### Phase 7: Package metadata from `*.dist-info` (current)
This works like `pip list`: instead of guessing names from the folders in `site-packages`, the tool reads the metadata pip writes for each installed distribution.

| Change | How |
| :--- | :--- |
| Name + version from `.dist-info` | One entry per `*.dist-info` folder. `Name:` and `Version:` come from the `METADATA` header (e.g. `GitPython 3.1.50`). If `METADATA` is missing, they are taken from the folder name. |
| Size per package from `RECORD` | Adds up the size column of `RECORD` (`path,hash,size`). Shown in the TUI next to each package. |
| PEP 503 name normalisation | Lowercase, and runs of `-` `_` `.` become `-`. Used to remove duplicates, to sort, and for search (`python_date` finds `python-dateutil`). |

This replaces the old cut-at-`-`-or-`.` heuristic, which mixed import names with distribution names (`git` + `gitpython`, `PIL` + `pillow`) and let through noise like `__pycache__` and `_distutils_hack`.

**Verification:** the package list (names + versions) matches `pip list` exactly for **22 of 23** venvs where pip could run. The one mismatch is `~/brainfuel/.django`: its `bin/python` is a symlink to the system Python, which has since been upgraded to 3.14, so it now has both `lib/python3.13` and `lib/python3.14`. pip reads the 3.14 one, while the tool follows `pyvenv.cfg` to the 3.13 one. The other 7 were skipped because they have no pip or a broken interpreter. Scan time is unchanged (~0.45 s).

**RECORD size vs disk usage (to investigate next):** across all 30 venvs, RECORD adds up to **23998 MB**, apparent file size is **25749 MB** and real disk usage is **27098 MB**, so RECORD covers about 89%. Small venvs (only pip + setuptools) are as low as ~40%. Likely causes:
- `.pyc` files compiled at install time are listed in RECORD with no size (5010 `.pyc` files, 90 MB in this repo's `.venv`)
- Disk usage rounds each file up to 4 KB blocks, which adds a lot when there are many small files
- Files no package owns (`__pycache__` at the top level, `_distutils_hack`)
- RECORD can also be *larger*: it lists files outside `site-packages` (`bin/`, `share/`) and files that were changed after install

### Phase 8: Clean-up and a real CLI (`venvscan`)
Getting the project ready to publish.

| Change | Details |
| :--- | :--- |
| Repo layout | Core code in `src/`; the Python prototypes and `parallel_scan.cpp` moved to `experiments/`; `.gitignore` for build output and `.venv/`. |
| One binary | The TUI and CLI each had their own copy of the scan code. It's now split into `scanner.cpp` (scan, sizes, packages), `tui.cpp` (ncurses) and `main.cpp` (arguments). |
| Arguments | `getopt_long` (the POSIX parser `ls`, `grep` and `du` use): `[PATH]`, `-l/--list`, `-j/--threads N`, `-h/--help`, `-V/--version`. Exit codes: 0 success, 1 runtime error, 2 bad arguments. |
| Auto list mode | When output goes to a pipe or file, the tool prints the plain-text report instead of the TUI (`venvscan \| grep torch`). |
| Build system | CMake: `find_package` locates OpenMP and ncurses on each platform, the version is defined once in `project()` and passed to `--version`, and `cmake --install` puts the binary in `/usr/local/bin`. |

**Verification:** the `-l` report has the same package lines and the same total as the old CLI (30 venvs, 27114 MB). Thread scaling with `-j`: 1 thread 2.7 s → 4 threads 0.82 s → all cores 0.44 s.

---

## Known limitations / next steps
- Only standard `python -m venv` venvs are supported. uv, virtualenv and Poetry write `version_info = ...`, which the regex skips.
- The size covers `site-packages` only, not the whole venv directory.
- RECORD-based package sizes don't add up to the venv's disk usage (see above); this needs a decision on how to report it.
- Only `.dist-info` is read; old `.egg-info` installs (`setup.py install/develop`) are not listed.
- Venvs with more than one `lib/pythonX.Y` folder (after a base-Python upgrade) only show the folder named in `pyvenv.cfg`.
- Ideas for later: mark packages the user asked for (`REQUESTED`), `Requires-Dist` dependency graph, import-name mapping from `RECORD`, editable-install tags, views across venvs.
