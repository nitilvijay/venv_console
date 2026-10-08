#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <getopt.h>
#include <unistd.h>
#include <omp.h>

#include "scanner.hpp"
#include "tui.hpp"

using namespace std;
namespace fs = std::filesystem;

#ifndef VENVSCAN_VERSION
#define VENVSCAN_VERSION "dev"
#endif

// Exit codes follow the usual Unix convention
constexpr int EXIT_OK = 0;
constexpr int EXIT_RUNTIME_ERROR = 1;
constexpr int EXIT_USAGE_ERROR = 2;

// ============================================================
// Help / Version
// ============================================================

void print_usage(ostream &out)
{
    out << "Usage: venvscan [OPTIONS] [PATH]\n"
           "\n"
           "Find Python virtual environments, their packages and disk usage.\n"
           "\n"
           "Arguments:\n"
           "  PATH               directory to scan (default: $HOME)\n"
           "\n"
           "Options:\n"
           "  -l, --list         print a plain-text report instead of the TUI\n"
           "                     (default when output is not a terminal)\n"
           "  -j, --threads N    number of threads (default: all cores)\n"
           "  -h, --help         show this help and exit\n"
           "  -V, --version      show version and exit\n";
}

// ============================================================
// Plain-text Report (--list)
// ============================================================

void print_list(const vector<VenvInfo> &venvs, long long total_size_mb)
{
    for (const auto &v : venvs)
    {
        cout << v.display_path << "  (Python " << v.python_version << ", "
             << v.size_mb << " MB, " << v.packages.size() << " packages)\n";
        for (const auto &pkg : v.packages)
        {
            cout << "    " << pkg.name << " " << pkg.version << " (" << format_size(pkg.size_bytes) << ")\n";
        }
    }
    cout << "Total: " << venvs.size() << (venvs.size() == 1 ? " environment, " : " environments, ") << total_size_mb << " MB\n";
}

// ============================================================
// Main
// ============================================================

int main(int argc, char *argv[])
{
    bool list_mode = false;

    const option long_options[] = {
        {"list", no_argument, nullptr, 'l'},
        {"threads", required_argument, nullptr, 'j'},
        {"help", no_argument, nullptr, 'h'},
        {"version", no_argument, nullptr, 'V'},
        {nullptr, 0, nullptr, 0},
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "lj:hV", long_options, nullptr)) != -1)
    {
        switch (opt)
        {
        case 'l':
            list_mode = true;
            break;

        case 'j':
        {
            char *end;
            long threads = strtol(optarg, &end, 10);
            if (*optarg == '\0' || *end != '\0' || threads < 1)
            {
                cerr << "venvscan: invalid thread count '" << optarg << "'\n";
                return EXIT_USAGE_ERROR;
            }
            omp_set_num_threads((int)threads);
            break;
        }

        case 'h':
            print_usage(cout);
            return EXIT_OK;

        case 'V':
            cout << "venvscan " << VENVSCAN_VERSION << "\n";
            return EXIT_OK;

        default: // getopt_long already printed the error
            cerr << "Try 'venvscan --help' for more information.\n";
            return EXIT_USAGE_ERROR;
        }
    }

    if (argc - optind > 1)
    {
        cerr << "venvscan: too many arguments\n"
             << "Try 'venvscan --help' for more information.\n";
        return EXIT_USAGE_ERROR;
    }

    const char *home_env = getenv("HOME");
    string home_dir = home_env ? home_env : "";

    string root;
    if (optind < argc)
    {
        root = argv[optind];
    }
    else if (!home_dir.empty())
    {
        root = home_dir;
    }
    else
    {
        cerr << "venvscan: HOME is not set; pass a PATH to scan\n";
        return EXIT_RUNTIME_ERROR;
    }

    error_code ec;
    if (!fs::is_directory(root, ec))
    {
        cerr << "venvscan: '" << root << "' is not a directory\n";
        return EXIT_RUNTIME_ERROR;
    }

    // The TUI needs a terminal; fall back to the report when piped or redirected
    if (!isatty(STDOUT_FILENO))
    {
        list_mode = true;
    }

    if (!list_mode)
    {
        cerr << "Scanning " << root << " for virtual environments..." << endl;
    }

    long long total_size_mb = 0;
    vector<VenvInfo> venvs = scan_venvs(root, home_dir, total_size_mb);

    if (list_mode)
    {
        print_list(venvs, total_size_mb);
    }
    else
    {
        run_tui(venvs, total_size_mb);
    }

    return EXIT_OK;
}
