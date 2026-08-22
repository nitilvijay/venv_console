#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <regex>
#include <algorithm>
#include <filesystem>
#include <cstdlib>
#include <cctype>
#include <omp.h>
#include <ncurses.h>

using namespace std;
namespace fs = std::filesystem;

// ============================================================
// Data Structures
// ============================================================

struct VenvInfo
{
    string full_path;
    string display_path;
    string python_version;
    long long size_mb;
    vector<string> packages;
};

enum FocusPanel
{
    FOCUS_ENVS,
    FOCUS_PACKAGES
};

// ============================================================
// Package Name Unification
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

// ============================================================
// Native Directory Size Calculation
// ============================================================

long long get_directory_size_mb(const fs::path &dir_path)
{
    uintmax_t total_bytes = 0;
    try
    {
        if (fs::exists(dir_path) && fs::is_directory(dir_path))
        {
            for (const auto &entry : fs::recursive_directory_iterator(dir_path, fs::directory_options::skip_permission_denied))
            {
                if (entry.is_regular_file() && !entry.is_symlink())
                {
                    total_bytes += entry.file_size();
                }
            }
        }
    }
    catch (const fs::filesystem_error &)
    {
        // Skip inaccessible paths
    }
    return static_cast<long long>(total_bytes / (1024 * 1024));
}

// ============================================================
// Parallel Directory Scan (OpenMP Tasks)
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
// String Helper Functions
// ============================================================

string to_lower(const string &str)
{
    string lower_str = str;
    transform(lower_str.begin(), lower_str.end(), lower_str.begin(), [](unsigned char c) {
        return tolower(c);
    });
    return lower_str;
}

string truncate_string(const string &str, size_t max_len)
{
    if (str.length() <= max_len)
    {
        return str;
    }
    if (max_len <= 3)
    {
        return str.substr(0, max_len);
    }
    return "..." + str.substr(str.length() - (max_len - 3));
}

// ============================================================
// TUI Application
// ============================================================

void run_tui(const vector<VenvInfo> &venvs, long long total_size_mb)
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors())
    {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_CYAN, -1);          // Title / Info
        init_pair(2, COLOR_BLACK, COLOR_CYAN); // Selected row
        init_pair(3, COLOR_GREEN, -1);         // Active border
        init_pair(4, COLOR_WHITE, -1);         // Inactive border
        init_pair(5, COLOR_BLACK, COLOR_WHITE);// Footer / Status
        init_pair(6, COLOR_YELLOW, -1);        // Search prompt
    }

    int selected_env = 0;
    int env_scroll_top = 0;
    int selected_pkg = 0;
    int pkg_scroll_top = 0;

    FocusPanel current_focus = FOCUS_ENVS;
    string search_query = "";
    bool in_search_input = false;

    bool running = true;

    while (running)
    {
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        erase();

        if (max_y < 10 || max_x < 40)
        {
            mvprintw(0, 0, "Terminal too small. Please resize.");
            refresh();
            int ch = getch();
            if (ch == 'q' || ch == 'Q')
            {
                break;
            }
            continue;
        }

        // 1. Header
        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(0, 2, "Python Virtual Environment Scanner");
        attroff(COLOR_PAIR(1) | A_BOLD);
        mvprintw(0, max_x - 22, "[ %zu Environments ]", venvs.size());

        // Layout dimensions
        int left_width = max(30, (max_x * 6) / 10);
        int right_width = max_x - left_width - 1;
        int panel_height = max_y - 3; // header (row 0), panels (row 1 to max_y-2), footer (row max_y-1)

        // 2. Left Panel: Environments Table
        if (current_focus == FOCUS_ENVS)
        {
            attron(COLOR_PAIR(3) | A_BOLD);
        }
        else
        {
            attron(COLOR_PAIR(4));
        }

        // Draw Left Box
        for (int y = 1; y <= panel_height; ++y)
        {
            mvaddch(y, 0, (y == 1) ? ACS_ULCORNER : (y == panel_height ? ACS_LLCORNER : ACS_VLINE));
            mvaddch(y, left_width - 1, (y == 1) ? ACS_URCORNER : (y == panel_height ? ACS_LRCORNER : ACS_VLINE));
        }
        for (int x = 1; x < left_width - 1; ++x)
        {
            mvaddch(1, x, ACS_HLINE);
            mvaddch(panel_height, x, ACS_HLINE);
        }
        mvprintw(1, 2, " Environments %s", (current_focus == FOCUS_ENVS ? "[Active]" : ""));
        if (current_focus == FOCUS_ENVS)
        {
            attroff(COLOR_PAIR(3) | A_BOLD);
        }
        else
        {
            attroff(COLOR_PAIR(4));
        }

        // Environments Table Headers
        int path_col_w = max(10, left_width - 24);
        attron(A_BOLD | A_UNDERLINE);
        mvprintw(2, 2, "%-*s %-8s %-8s", path_col_w, "Environment Path", "Python", "Size(MB)");
        attroff(A_BOLD | A_UNDERLINE);

        // Adjust scroll window for envs
        int env_visible_rows = panel_height - 3;
        if (selected_env < 0) selected_env = 0;
        if (!venvs.empty() && selected_env >= (int)venvs.size()) selected_env = (int)venvs.size() - 1;

        if (selected_env < env_scroll_top)
        {
            env_scroll_top = selected_env;
        }
        else if (selected_env >= env_scroll_top + env_visible_rows)
        {
            env_scroll_top = selected_env - env_visible_rows + 1;
        }

        // Draw Environments Rows
        for (int i = 0; i < env_visible_rows; ++i)
        {
            int env_idx = env_scroll_top + i;
            int row_y = 3 + i;
            if (env_idx < (int)venvs.size())
            {
                const auto &v = venvs[env_idx];
                string truncated_path = truncate_string(v.display_path, path_col_w);

                char row_buf[256];
                snprintf(row_buf, sizeof(row_buf), "%-*s %-8s %-8lld",
                         path_col_w, truncated_path.c_str(), v.python_version.c_str(), v.size_mb);

                if (env_idx == selected_env)
                {
                    attron(COLOR_PAIR(2) | A_BOLD);
                    mvprintw(row_y, 2, "%-*s", left_width - 4, row_buf);
                    attroff(COLOR_PAIR(2) | A_BOLD);
                }
                else
                {
                    mvprintw(row_y, 2, "%-*s", left_width - 4, row_buf);
                }
            }
        }

        // 3. Right Panel: Packages & Search
        int right_start_x = left_width;
        if (current_focus == FOCUS_PACKAGES)
        {
            attron(COLOR_PAIR(3) | A_BOLD);
        }
        else
        {
            attron(COLOR_PAIR(4));
        }

        // Draw Right Box
        for (int y = 1; y <= panel_height; ++y)
        {
            mvaddch(y, right_start_x, (y == 1) ? ACS_ULCORNER : (y == panel_height ? ACS_LLCORNER : ACS_VLINE));
            mvaddch(y, max_x - 1, (y == 1) ? ACS_URCORNER : (y == panel_height ? ACS_LRCORNER : ACS_VLINE));
        }
        for (int x = right_start_x + 1; x < max_x - 1; ++x)
        {
            mvaddch(1, x, ACS_HLINE);
            mvaddch(panel_height, x, ACS_HLINE);
        }
        mvprintw(1, right_start_x + 2, " Installed Packages %s", (current_focus == FOCUS_PACKAGES ? "[Active]" : ""));
        if (current_focus == FOCUS_PACKAGES)
        {
            attroff(COLOR_PAIR(3) | A_BOLD);
        }
        else
        {
            attroff(COLOR_PAIR(4));
        }

        // Search Input Bar
        attron(COLOR_PAIR(6));
        mvprintw(2, right_start_x + 2, "Search: ");
        attroff(COLOR_PAIR(6));

        int search_box_w = max(5, right_width - 12);
        string search_display = search_query.empty() ? (in_search_input ? "" : "<Type to search...>") : search_query;
        if (in_search_input)
        {
            attron(A_UNDERLINE | A_BOLD);
            mvprintw(2, right_start_x + 10, "%-*s", search_box_w, search_display.c_str());
            attroff(A_UNDERLINE | A_BOLD);
        }
        else
        {
            mvprintw(2, right_start_x + 10, "%-*s", search_box_w, search_display.c_str());
        }

        // Filter packages for selected venv
        vector<string> filtered_pkgs;
        if (!venvs.empty() && selected_env >= 0 && selected_env < (int)venvs.size())
        {
            string query_lower = to_lower(search_query);
            for (const auto &pkg : venvs[selected_env].packages)
            {
                if (query_lower.empty() || to_lower(pkg).find(query_lower) != string::npos)
                {
                    filtered_pkgs.push_back(pkg);
                }
            }
        }

        // Package List Scrolling
        int pkg_visible_rows = panel_height - 4;
        if (selected_pkg < 0) selected_pkg = 0;
        if (!filtered_pkgs.empty() && selected_pkg >= (int)filtered_pkgs.size()) selected_pkg = (int)filtered_pkgs.size() - 1;

        if (selected_pkg < pkg_scroll_top)
        {
            pkg_scroll_top = selected_pkg;
        }
        else if (selected_pkg >= pkg_scroll_top + pkg_visible_rows)
        {
            pkg_scroll_top = selected_pkg - pkg_visible_rows + 1;
        }

        // Draw Packages
        for (int i = 0; i < pkg_visible_rows; ++i)
        {
            int pkg_idx = pkg_scroll_top + i;
            int row_y = 3 + i;
            if (pkg_idx < (int)filtered_pkgs.size())
            {
                string pkg_name = truncate_string(filtered_pkgs[pkg_idx], right_width - 6);
                if (current_focus == FOCUS_PACKAGES && pkg_idx == selected_pkg && !in_search_input)
                {
                    attron(COLOR_PAIR(2) | A_BOLD);
                    mvprintw(row_y, right_start_x + 2, " %-*s", right_width - 6, pkg_name.c_str());
                    attroff(COLOR_PAIR(2) | A_BOLD);
                }
                else
                {
                    mvprintw(row_y, right_start_x + 2, " %-*s", right_width - 6, pkg_name.c_str());
                }
            }
        }

        if (filtered_pkgs.empty() && !venvs.empty())
        {
            mvprintw(4, right_start_x + 4, "(No matching packages)");
        }

        // 4. Footer
        attron(COLOR_PAIR(5) | A_BOLD);
        for (int x = 0; x < max_x; ++x)
        {
            mvaddch(max_y - 1, x, ' ');
        }
        mvprintw(max_y - 1, 2, " Total Size: %lld MB  |  [Tab/Left/Right] Switch Panel  |  [Up/Down] Navigate  |  [/] Search  |  [q] Quit", total_size_mb);
        attroff(COLOR_PAIR(5) | A_BOLD);

        if (in_search_input)
        {
            curs_set(1);
            wmove(stdscr, 2, right_start_x + 10 + (int)search_query.length());
        }
        else
        {
            curs_set(0);
        }

        refresh();

        // 5. Input Handling
        int ch = getch();

        if (in_search_input)
        {
            if (ch == 27 || ch == '\n' || ch == KEY_ENTER) // ESC or Enter exits search input
            {
                in_search_input = false;
            }
            else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
            {
                if (!search_query.empty())
                {
                    search_query.pop_back();
                    selected_pkg = 0;
                    pkg_scroll_top = 0;
                }
            }
            else if (ch == '\t')
            {
                in_search_input = false;
                current_focus = FOCUS_ENVS;
            }
            else if (isprint(ch))
            {
                search_query.push_back((char)ch);
                selected_pkg = 0;
                pkg_scroll_top = 0;
            }
        }
        else
        {
            switch (ch)
            {
            case 'q':
            case 'Q':
                running = false;
                break;

            case '\t':
                current_focus = (current_focus == FOCUS_ENVS) ? FOCUS_PACKAGES : FOCUS_ENVS;
                break;

            case KEY_RIGHT:
                current_focus = FOCUS_PACKAGES;
                break;

            case KEY_LEFT:
                current_focus = FOCUS_ENVS;
                break;

            case '/':
                current_focus = FOCUS_PACKAGES;
                in_search_input = true;
                break;

            case KEY_UP:
                if (current_focus == FOCUS_ENVS)
                {
                    if (selected_env > 0)
                    {
                        selected_env--;
                        selected_pkg = 0;
                        pkg_scroll_top = 0;
                    }
                }
                else
                {
                    if (selected_pkg > 0)
                    {
                        selected_pkg--;
                    }
                }
                break;

            case KEY_DOWN:
                if (current_focus == FOCUS_ENVS)
                {
                    if (selected_env + 1 < (int)venvs.size())
                    {
                        selected_env++;
                        selected_pkg = 0;
                        pkg_scroll_top = 0;
                    }
                }
                else
                {
                    if (selected_pkg + 1 < (int)filtered_pkgs.size())
                    {
                        selected_pkg++;
                    }
                }
                break;

            case KEY_PPAGE: // Page Up
                if (current_focus == FOCUS_ENVS)
                {
                    selected_env = max(0, selected_env - env_visible_rows);
                    selected_pkg = 0;
                    pkg_scroll_top = 0;
                }
                else
                {
                    selected_pkg = max(0, selected_pkg - pkg_visible_rows);
                }
                break;

            case KEY_NPAGE: // Page Down
                if (current_focus == FOCUS_ENVS)
                {
                    selected_env = min((int)venvs.size() - 1, selected_env + env_visible_rows);
                    selected_pkg = 0;
                    pkg_scroll_top = 0;
                }
                else
                {
                    selected_pkg = min((int)filtered_pkgs.size() - 1, selected_pkg + pkg_visible_rows);
                }
                break;

            case KEY_HOME:
                if (current_focus == FOCUS_ENVS)
                {
                    selected_env = 0;
                    selected_pkg = 0;
                    pkg_scroll_top = 0;
                }
                else
                {
                    selected_pkg = 0;
                }
                break;

            case KEY_END:
                if (current_focus == FOCUS_ENVS)
                {
                    selected_env = max(0, (int)venvs.size() - 1);
                    selected_pkg = 0;
                    pkg_scroll_top = 0;
                }
                else
                {
                    selected_pkg = max(0, (int)filtered_pkgs.size() - 1);
                }
                break;

            case KEY_RESIZE:
                // Terminal resized, will redraw on next loop
                break;

            default:
                break;
            }
        }
    }

    endwin();
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

    cout << "Scanning home directory for virtual environments in parallel..." << endl;

    vector<string> venv_paths;

    #pragma omp parallel
    {
        #pragma omp single
        {
            scan_parallel(home_dir, venv_paths);
        }
    }

    cout << "Found " << venv_paths.size() << " environments. Analyzing packages and computing sizes..." << endl;

    vector<VenvInfo> venvs(venv_paths.size());
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

            vector<string> raw_packages;
            try
            {
                if (fs::exists(site_packages_path) && fs::is_directory(site_packages_path))
                {
                    for (const auto &item : fs::directory_iterator(site_packages_path, fs::directory_options::skip_permission_denied))
                    {
                        raw_packages.push_back(item.path().filename().string());
                    }
                }
            }
            catch (const fs::filesystem_error &)
            {
                // Skip if inaccessible
            }

            set<string> unified = unify_pkg_names(raw_packages);
            vector<string> pkg_list(unified.begin(), unified.end());
            sort(pkg_list.begin(), pkg_list.end());

            long long env_size = get_directory_size_mb(site_packages_path);

            string display_path = venv_dir.string();
            if (display_path.rfind(home_dir, 0) == 0)
            {
                display_path = "~" + display_path.substr(home_dir.length());
            }

            venvs[i] = {
                venv_dir.string(),
                display_path,
                full_ver,
                env_size,
                pkg_list
            };

            total_size += env_size;
        }
    }

    // Filter out invalid/empty entries if any
    vector<VenvInfo> valid_venvs;
    for (auto &v : venvs)
    {
        if (!v.display_path.empty())
        {
            valid_venvs.push_back(move(v));
        }
    }

    // Sort by size descending (largest virtual environments first)
    sort(valid_venvs.begin(), valid_venvs.end(), [](const VenvInfo &a, const VenvInfo &b) {
        return a.size_mb > b.size_mb;
    });

    run_tui(valid_venvs, total_size);

    return 0;
}