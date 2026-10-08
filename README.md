# venvscan

![License: MIT](https://img.shields.io/badge/license-MIT-blue) ![Platform: Linux](https://img.shields.io/badge/platform-linux-lightgrey) ![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C)

**Find every forgotten Python virtual environment on your machine, and see how much disk it's eating.**

> On my laptop: **30 venvs, 27 GB**, found and measured in **under half a second**.

`venvscan` walks your home directory in parallel (C++ / OpenMP), finds every venv created with `python -m venv`, and shows its Python version, its installed packages and its real disk usage in an interactive terminal UI.

![venvscan demo](demo/demo.gif)

```bash
git clone https://github.com/nitilvijay/venv_console.git && cd venv_console
cmake -B build && cmake --build build
./build/venvscan
```

---

## Motivation

As developers, we tend to create and experiment with many main and side projects, most of which involve Python. Each project inevitably comes with its own virtual environment (`.venv`). Over time, dozens of these environments accumulate across directories, quietly consuming gigabytes of disk space unnoticed.

This tool provides a centralized, fast console interface to give full visibility into all virtual environments on your drive—their true disk usage and their installed packages.

---

## Features & Parallel Architecture

- **OpenMP Directory Traversal (`#pragma omp task`)**: Recursively searches the entire home directory tree using task parallelism to locate all `pyvenv.cfg` files within seconds.
- **Concurrent Environment Processing (`#pragma omp parallel for`)**: Analyzes all discovered environments concurrently. Using dynamic scheduling (`schedule(dynamic)`), worker threads simultaneously inspect package directories, read package metadata, and compute disk usage across multiple environments in parallel.
- **Accurate Disk Usage**: Calculates real filesystem block allocations using POSIX `st_blocks` (matching `du -sm` / `du -shm` exact disk usage).
- **Interactive TUI**: Split-view terminal interface powered by `ncurses` featuring live package filtering, scrollable tables, and disk footprint summaries.
- **Package Metadata (like `pip list`)**: Reads each `*.dist-info` for the package name and version, sums `RECORD` for per-package size, and normalizes names per PEP 503.

---

## Prerequisites

On Debian/Ubuntu/Arch/Fedora:
- `g++` (supporting C++17)
- `cmake` (3.16+)
- `OpenMP` (included with GCC)
- `libncurses` (for the TUI)

```bash
# Ubuntu / Debian
sudo apt install build-essential cmake libncurses-dev

# Arch Linux
sudo pacman -S base-devel cmake ncurses

# Fedora
sudo dnf install gcc-c++ cmake ncurses-devel
```

---

## Build & Install

```bash
cmake -B build
cmake --build build
sudo cmake --install build      # installs to /usr/local/bin/venvscan
```

## Usage

```
venvscan [OPTIONS] [PATH]

  PATH               directory to scan (default: $HOME)
  -l, --list         print a plain-text report instead of the TUI
                     (default when output is not a terminal)
  -j, --threads N    number of threads (default: all cores)
  -h, --help         show help and exit
  -V, --version      show version and exit
```

```bash
venvscan                       # interactive TUI over your home directory
venvscan ~/projects            # scan one directory
venvscan -l | grep torch       # plain-text report, pipe-friendly
```

---

## TUI Keybindings

| Key | Action |
| :--- | :--- |
| <kbd>Tab</kbd> / <kbd>←</kbd> / <kbd>→</kbd> | Switch between Environments and Packages panels |
| <kbd>↑</kbd> / <kbd>↓</kbd> | Navigate list / table rows |
| <kbd>PgUp</kbd> / <kbd>PgDn</kbd> | Page up / down |
| <kbd>/</kbd> | Jump to Package search bar (<kbd>Esc</kbd> or <kbd>Enter</kbd> to exit search) |
| <kbd>q</kbd> | Quit application |
---

## License

[MIT](LICENSE)
