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

---

## Known limitations / next steps
- Only standard `python -m venv` venvs are supported. uv, virtualenv and Poetry write `version_info = ...`, which the regex skips.
- The size covers `site-packages` only, not the whole venv directory.
- Package-name merging is heuristic: it leaves through entries like `__pycache__` and `_distutils_hack`, and import names vs distribution names (`yaml` vs `PyYAML`) show up as separate packages.
