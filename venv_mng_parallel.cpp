#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <regex>
#include <filesystem>
#include <cstdlib>
#include <omp.h>

using namespace std;
namespace fs = std::filesystem;

// ============================================================
// Unify package names (deduplicate package distributions)
// ============================================================

set<string> unify_pkg_names(const vector<string> &pkg_list)
{
    set<string> unified_list;
    for (const auto &pkg : pkg_list)
    {
        auto dash_pos = pkg.find('-');
        auto dot_pos = pkg.find('.');

        if (dash_pos != string::npos)
        {
            unified_list.insert(pkg.substr(0, dash_pos));
        }
        else if (dot_pos != string::npos)
        {
            unified_list.insert(pkg.substr(0, dot_pos));
        }
        else
        {
            unified_list.insert(pkg);
        }
    }
    return unified_list;
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

            vector<string> packages;
            try
            {
                if (fs::exists(site_packages_path) && fs::is_directory(site_packages_path))
                {
                    for (const auto &item : fs::directory_iterator(site_packages_path, fs::directory_options::skip_permission_denied))
                    {
                        packages.push_back(item.path().filename().string());
                    }
                }
            }
            catch (const fs::filesystem_error &)
            {
                // Skip if error reading directory
            }

            set<string> unified_packages = unify_pkg_names(packages);

            long long env_size = get_directory_size_mb(site_packages_path);

            #pragma omp critical
            {
                cout << site_packages_path.string() << endl;
                cout << "Total size of installed packages and tools: " << env_size << endl;
            }

            total_size += env_size;
        }
    }

    cout << "Total size of all virtual environments: " << total_size << " MB" << endl;

    return 0;
}