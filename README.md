# Python Virtual Environment Manager & Scanner ⚡

A high-performance tool built with C++ and OpenMP to rapidly discover Python virtual environments across your system, compute their accurate disk usage, and explore installed packages interactively.

---

## Motivation

As developers, we tend to create and experiment with many main and side projects, most of which involve Python. Each project inevitably comes with its own virtual environment (`.venv`). Over time, dozens of these environments accumulate across directories, quietly consuming gigabytes of disk space unnoticed.

This tool provides a centralized, fast console interface to give full visibility into all virtual environments on your drive—their true disk usage and their installed packages.

---

## Features & Parallel Architecture

- **OpenMP Directory Traversal (`#pragma omp task`)**: Recursively searches the entire home directory tree using task parallelism to locate all `pyvenv.cfg` files within seconds.
- **Concurrent Environment Processing (`#pragma omp parallel for`)**: Analyzes all discovered environments concurrently. Using dynamic scheduling (`schedule(dynamic)`), worker threads simultaneously inspect package directories, unify package distribution names, and compute disk usage across multiple environments in parallel.
- **Accurate Disk Usage**: Calculates real filesystem block allocations using POSIX `st_blocks` (matching `du -sm` / `du -shm` exact disk usage).
- **Interactive TUI**: Split-view terminal interface powered by `ncurses` featuring live package filtering, scrollable tables, and disk footprint summaries.
- **Package Deduplication**: Intelligently unifies distribution folders (e.g. `django` vs `django-5.0.dist-info`).

---

## Prerequisites

On Debian/Ubuntu/Arch/Fedora:
- `g++` (supporting C++17)
- `OpenMP` (included with GCC)
- `libncurses` (for the TUI)

```bash
# Ubuntu / Debian
sudo apt install build-essential libncurses-dev

# Arch Linux
sudo pacman -S base-devel ncurses

# Fedora
sudo dnf install gcc-c++ ncurses-devel
```

---

## Build & Run

### 1. Interactive Terminal UI (TUI)
```bash
g++ -O3 -fopenmp -std=c++17 venv_tui.cpp -lncurses -o venv_tui
./venv_tui
```

### 2. Fast CLI Scanner
```bash
g++ -O3 -fopenmp -std=c++17 venv_mng_parallel.cpp -o venv_mng_parallel
./venv_mng_parallel
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