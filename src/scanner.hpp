#pragma once

#include <string>
#include <vector>

// ============================================================
// Data Structures
// ============================================================

struct PackageInfo
{
    std::string name;            // Display name from METADATA, e.g. "GitPython"
    std::string normalized_name; // PEP 503 name, e.g. "gitpython"
    std::string version;
    long long size_bytes;        // Sum of the file sizes listed in RECORD
};

struct VenvInfo
{
    std::string full_path;
    std::string display_path;
    std::string python_version;
    long long size_mb;
    std::vector<PackageInfo> packages;
};

// Finds every venv under `root` and analyses them in parallel, largest first.
// Paths under `home_dir` are displayed as "~/..." (pass "" to disable).
std::vector<VenvInfo> scan_venvs(const std::string &root, const std::string &home_dir, long long &total_size_mb);

// PEP 503: lowercase, and collapse runs of '-', '_' and '.' into a single '-'
std::string normalize_name(const std::string &name);

// Human-readable size, e.g. "38.3 KB"
std::string format_size(long long bytes);
