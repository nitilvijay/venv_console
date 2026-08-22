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
    try
    {
        if (fs::exists(dir_path) && fs::is_directory(dir_path))
        {
            struct stat st;
            if (lstat(dir_path.c_str(), &st) == 0)
            {
                total_512_blocks += st.st_blocks;
            }

            for (const auto &entry : fs::recursive_directory_iterator(dir_path, fs::directory_options::skip_permission_denied))
            {
                if (lstat(entry.path().c_str(), &st) == 0)
                {
                    total_512_blocks += st.st_blocks;
                }
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
    try
    {
        for (const auto &entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied))
        {
            if (entry.is_directory())
            {
                string subfolder = entry.path().string();

                #pragma omp task firstprivate(subfolder) shared(venv_paths)
                scan_parallel(subfolder, venv_paths);
            }
            else if (entry.is_regular_file())
            {
                if (entry.path().filename() == "pyvenv.cfg")
                {
                    #pragma omp critical
                    {
                        venv_paths.push_back(entry.path().string());
                    }
                }
            }
        }

        #pragma omp taskwait
    }
    catch (const fs::filesystem_error &)
    {
        // Skip inaccessible folders
    }
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