# Python Virtual Environment Console

A high-performance tool built with C++ and OpenMP to rapidly discover Python virtual environments across your system, compute their accurate disk usage, and explore installed packages interactively.

---

## Features

- **Blazing Fast Scanning**: Uses OpenMP task-parallelism (`#pragma omp task`) to scan the entire home directory in seconds.
- **Accurate Disk Usage**: Calculates real filesystem block allocations (matching `du -sm` / `du -shm`).
- **Interactive TUI**: Split-view terminal interface powered by `ncurses` showing all environments and real-time searchable package lists.
- **Deduplication**: Unifies package distribution names (e.g. `django` vs `django-5.0.dist-info`).

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