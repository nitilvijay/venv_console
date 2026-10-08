#include "tui.hpp"
#include "scanner.hpp"

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ncurses.h>

using namespace std;

enum FocusPanel
{
    FOCUS_ENVS,
    FOCUS_PACKAGES
};

// ============================================================
// String Helper Functions
// ============================================================

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
        vector<const PackageInfo *> filtered_pkgs;
        if (!venvs.empty() && selected_env >= 0 && selected_env < (int)venvs.size())
        {
            string query_normalized = normalize_name(search_query);
            for (const auto &pkg : venvs[selected_env].packages)
            {
                if (query_normalized.empty() || pkg.normalized_name.find(query_normalized) != string::npos)
                {
                    filtered_pkgs.push_back(&pkg);
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
                const PackageInfo &pkg = *filtered_pkgs[pkg_idx];
                // Columns: name | version (12) | size (9); drop version when the panel is narrow
                int row_w = right_width - 6;
                bool show_version = row_w - 23 >= 16;
                int name_col_w = max(1, row_w - (show_version ? 23 : 10));
                string name = pkg.name.substr(0, name_col_w);

                char row_buf[256];
                if (show_version)
                {
                    snprintf(row_buf, sizeof(row_buf), "%-*s %-12s %9s", name_col_w, name.c_str(),
                             pkg.version.substr(0, 12).c_str(), format_size(pkg.size_bytes).c_str());
                }
                else
                {
                    snprintf(row_buf, sizeof(row_buf), "%-*s %9s", name_col_w, name.c_str(),
                             format_size(pkg.size_bytes).c_str());
                }
                string row_text = string(row_buf).substr(0, row_w);

                if (current_focus == FOCUS_PACKAGES && pkg_idx == selected_pkg && !in_search_input)
                {
                    attron(COLOR_PAIR(2) | A_BOLD);
                    mvprintw(row_y, right_start_x + 2, " %-*s", right_width - 6, row_text.c_str());
                    attroff(COLOR_PAIR(2) | A_BOLD);
                }
                else
                {
                    mvprintw(row_y, right_start_x + 2, " %-*s", right_width - 6, row_text.c_str());
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
