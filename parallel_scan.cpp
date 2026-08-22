#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <omp.h>

using namespace std;
namespace fs = std::filesystem;

// ============================================================
// Serial directory scan
// ============================================================

void scan_serial(const string &path,
                 long long &files,
                 long long &directories)
{
    try
    {
        for (const auto &entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied))
        {
            if (entry.is_directory())
            {
                directories++;
                scan_serial(entry.path().string(), files, directories);
            }
            else if (entry.is_regular_file())
            {
                files++;
            }
        }
    }
    catch (const fs::filesystem_error &)
    {
        // Skip inaccessible folders
    }
}

// ============================================================
// Task-parallel directory scan
// ============================================================

void scan_parallel(const string &path, long long &files, long long &directories)
{
    try
    {
        for (const auto &entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied))
        {
            if (entry.is_directory())
            {
                #pragma omp atomic
                directories++;

                string subfolder = entry.path().string();

                // Explicitly copy subfolder into each task and share reference counters
                #pragma omp task firstprivate(subfolder) shared(files, directories)
                scan_parallel(subfolder, files, directories);
            }
            else if (entry.is_regular_file())
            {
                //Check if the file name is pyvenv.cfg
                if(entry.path().filename() == "pyvenv.cfg")
                {
                    cout << "Found pyvenv.cfg in: " << entry.path().parent_path() << endl;
                }
                #pragma omp atomic
                files++;
            }
        }

        // Wait for all child tasks created in this scope to complete
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

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cout << "Usage: " << argv[0] << " <directory>\n";
        return 1;
    }

    string root = argv[1];

    // ========================================================
    // SERIAL SCAN
    // ========================================================

    long long serial_files = 0;
    long long serial_directories = 0;

    auto start = chrono::high_resolution_clock::now();

    scan_serial(root, serial_files, serial_directories);

    auto end = chrono::high_resolution_clock::now();

    double serial_time = chrono::duration<double>(end - start).count();

    // ========================================================
    // TASK-PARALLEL SCAN
    // ========================================================

    long long parallel_files = 0;
    long long parallel_directories = 0;

    start = chrono::high_resolution_clock::now();

    #pragma omp parallel
    {
        #pragma omp single
        {
            scan_parallel(root, parallel_files, parallel_directories);
        }
    }

    end = chrono::high_resolution_clock::now();

    double parallel_time = chrono::duration<double>(end - start).count();

    // ========================================================
    // RESULTS
    // ========================================================

    cout << "\n========================================\n";
    cout << "       DIRECTORY TREE SCAN RESULTS\n";
    cout << "========================================\n";

    cout << "\nMetric                  Serial      Task-parallel\n";
    cout << "-------------------------------------------------\n";

    cout << "Files found             "
         << serial_files
         << "          "
         << parallel_files
         << "\n";

    cout << "Directories found       "
         << serial_directories
         << "          "
         << parallel_directories
         << "\n";

    cout << "Execution time (s)      "
         << serial_time
         << "        "
         << parallel_time
         << "\n";

    // ========================================================
    // Verify results
    // ========================================================

    if (serial_files == parallel_files &&
        serial_directories == parallel_directories)
    {
        cout << "\nVerification: PASS\n";
        cout << "Both versions found the same number of files "
             << "and directories.\n";
    }
    else
    {
        cout << "\nVerification: FAIL\n";
    }

    cout << "\nThreads used: "
         << omp_get_max_threads()
         << "\n";

    return 0;
}