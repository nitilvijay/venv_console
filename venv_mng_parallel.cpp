#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <regex>
#include <filesystem>
#include <cstdlib>
#include <omp.h>

using namespace std;
namespace fs = std::filesystem;

struct PackageInfo
{
    string name;            // Display name from METADATA, e.g. "GitPython"
    string normalized_name; // PEP 503 name, e.g. "gitpython"
    string version;
    long long size_bytes;   // Sum of the file sizes listed in RECORD
};

// ============================================================
// Installed Packages (read from *.dist-info, like `pip list`)
// ============================================================

// PEP 503: lowercase, and collapse runs of '-', '_' and '.' into a single '-'
string normalize_name(const string &name)
{
    string normalized;
    for (unsigned char c : name)
    {
        if (c == '-' || c == '_' || c == '.')
        {
            if (normalized.empty() || normalized.back() != '-')
            {
                normalized.push_back('-');
            }
        }
        else
        {
            normalized.push_back(tolower(c));
        }
    }
    return normalized;
}

PackageInfo read_dist_info(const fs::path &dist_info)
{
    PackageInfo pkg{"", "", "", 0};
    string line;

    // METADATA headers end at the first blank line
    ifstream metadata(dist_info / "METADATA");
    while (getline(metadata, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.empty())
        {
            break;
        }
        if (line.rfind("Name: ", 0) == 0)
        {
            pkg.name = line.substr(6);
        }
        else if (line.rfind("Version: ", 0) == 0)
        {
            pkg.version = line.substr(9);
        }
    }

    // Fall back to the folder name: <name>-<version>.dist-info
    if (pkg.name.empty())
    {
        string stem = dist_info.stem().string();
        size_t dash_pos = stem.find('-');
        pkg.name = stem.substr(0, dash_pos);
        if (dash_pos != string::npos)
        {
            pkg.version = stem.substr(dash_pos + 1);
        }
    }
    pkg.normalized_name = normalize_name(pkg.name);

    // RECORD is CSV: path,hash,size (size is empty for RECORD itself and .pyc files)
    ifstream record(dist_info / "RECORD");
    while (getline(record, line))
    {
        size_t comma_pos = line.rfind(',');
        if (comma_pos == string::npos)
        {
            continue;
        }
        const char *size_str = line.c_str() + comma_pos + 1;
        char *end;
        long long size = strtoll(size_str, &end, 10);
        if (end != size_str)
        {
            pkg.size_bytes += size;
        }
    }

    return pkg;
}

vector<PackageInfo> list_packages(const fs::path &site_packages_path)
{
    // Keyed by normalized name: deduplicates and sorts in one go
    map<string, PackageInfo> packages;
    try
    {
        for (const auto &item : fs::directory_iterator(site_packages_path, fs::directory_options::skip_permission_denied))
        {
            if (item.is_directory() && item.path().extension() == ".dist-info")
            {
                PackageInfo pkg = read_dist_info(item.path());
                packages.emplace(pkg.normalized_name, move(pkg));
            }
        }
    }
    catch (const fs::filesystem_error &)
    {
        // Skip if error reading directory
    }

    vector<PackageInfo> result;
    for (auto &entry : packages)
    {
        result.push_back(move(entry.second));
    }
    return result;
}

string format_size(long long bytes)
{
    const char *units[] = {"B", "KB", "MB", "GB"};
    double size = bytes;
    int unit = 0;
    while (size >= 1024 && unit < 3)
    {
        size /= 1024;
        ++unit;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), unit == 0 ? "%.0f %s" : "%.1f %s", size, units[unit]);
    return buf;
}

#include <sys/stat.h>

// ============================================================
// Calculate directory disk usage in Megabytes (MB) matching `du -sm`
// ============================================================

long long get_directory_size_mb(const fs::path &dir_path)
{
    long long total_512_blocks = 0;

    // Hard-linked files share one inode; count each inode once, like du does
    set<pair<dev_t, ino_t>> seen_inodes;
    auto add_blocks = [&](const char *p) {
        struct stat st;
        if (lstat(p, &st) != 0)
        {
            return;
        }
        if (!S_ISDIR(st.st_mode) && st.st_nlink > 1 &&
            !seen_inodes.insert({st.st_dev, st.st_ino}).second)
        {
            return;
        }
        total_512_blocks += st.st_blocks;
    };

    try
    {
        if (fs::exists(dir_path) && fs::is_directory(dir_path))
        {
            add_blocks(dir_path.c_str());

            // recursive_directory_iterator does not follow directory symlinks by default
            for (const auto &entry : fs::recursive_directory_iterator(dir_path, fs::directory_options::skip_permission_denied))
            {
                add_blocks(entry.path().c_str());
            }
        }
    }
    catch (const fs::filesystem_error &)
    {
        // Skip inaccessible files or directories
    }
    // Convert 512-byte filesystem blocks to MB (rounding up like du -sm / du -shm)
    return (total_512_blocks * 512 + 1024 * 1024 - 1) / (1024 * 1024);
}

// ============================================================
// Task-parallel directory scan to find pyvenv.cfg files
// ============================================================

void scan_parallel(const string &path, vector<string> &venv_paths)
{
    vector<string> subfolders;
    bool is_venv = false;

    try
    {
        for (const auto &entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied))
        {
            // Never follow symlinks: avoids loops and counting the same venv twice
            if (entry.is_symlink())
            {
                continue;
            }

            if (entry.is_directory())
            {
                subfolders.push_back(entry.path().string());
            }
            else if (entry.is_regular_file() && entry.path().filename() == "pyvenv.cfg")
            {
                is_venv = true;
            }
        }
    }
    catch (const fs::filesystem_error &)
    {
        // Skip inaccessible folders
    }

    // This directory is a venv: record it and don't descend into it
    if (is_venv)
    {
        #pragma omp critical
        {
            venv_paths.push_back((fs::path(path) / "pyvenv.cfg").string());
        }
        return;
    }

    for (string subfolder : subfolders)
    {
        #pragma omp task firstprivate(subfolder) shared(venv_paths)
        scan_parallel(subfolder, venv_paths);
    }

    // No taskwait: the implicit barrier at the end of the parallel region
    // waits for all tasks, so parents don't block on their children
}

// ============================================================
// Main
// ============================================================

int main()
{
    const char *home_env = getenv("HOME");
    if (!home_env)
    {
        cerr << "Error: HOME environment variable not found." << endl;
        return 1;
    }
    string home_dir = home_env;

    vector<string> venv_paths;

    #pragma omp parallel
    {
        #pragma omp single
        {
            scan_parallel(home_dir, venv_paths);
        }
    }

    cout << "Virtual environments found:" << endl;

    long long total_size = 0;
    const regex version_regex(R"(version = (\d+\.\d+\.\d+))");

    #pragma omp parallel for reduction(+:total_size) schedule(dynamic)
    for (size_t i = 0; i < venv_paths.size(); ++i)
    {
        const string &venv_path = venv_paths[i];

        ifstream f(venv_path);
        if (!f.is_open())
        {
            continue;
        }

        stringstream buffer;
        buffer << f.rdbuf();
        string content = buffer.str();
        f.close();

        smatch version_match;
        if (regex_search(content, version_match, version_regex))
        {
            string full_ver = version_match[1].str();
            size_t second_dot_pos = full_ver.find('.', 2);

            string py_version_prefix = (second_dot_pos != string::npos)
                                           ? full_ver.substr(0, second_dot_pos)
                                           : full_ver;

            fs::path venv_dir = fs::path(venv_path).parent_path();
            fs::path site_packages_path = venv_dir / "lib" / ("python" + py_version_prefix) / "site-packages";

            vector<PackageInfo> packages = list_packages(site_packages_path);

            long long env_size = get_directory_size_mb(site_packages_path);

            #pragma omp critical
            {
                cout << site_packages_path.string() << endl;
                cout << "Total size of installed packages and tools: " << env_size << endl;
                for (const auto &pkg : packages)
                {
                    cout << "    " << pkg.name << " " << pkg.version << " (" << format_size(pkg.size_bytes) << ")" << endl;
                }
            }

            total_size += env_size;
        }
    }

    cout << "Total size of all virtual environments: " << total_size << " MB" << endl;

    return 0;
}