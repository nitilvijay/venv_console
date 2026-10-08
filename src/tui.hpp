#pragma once

#include <vector>

#include "scanner.hpp"

// Interactive ncurses browser: environments on the left, packages on the right
void run_tui(const std::vector<VenvInfo> &venvs, long long total_size_mb);
