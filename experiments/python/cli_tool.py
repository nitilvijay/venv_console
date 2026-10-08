import subprocess as sp
import os
import re
from sortedcontainers import SortedList #sophisticated block-based data structure that's extremely fast in Python.
from textual import work
from textual.app import App, ComposeResult
from textual.containers import Horizontal, Vertical
from textual.widgets import DataTable, Label, ProgressBar, Input

sl = SortedList()

class EnvTable(App):
    CSS = """
    DataTable {
        scrollbar-size: 0 0;
    }
    
    #footer {
        dock: bottom;
        height: 1;
        background: $accent;
        color: $text;
        content-align: center middle;
        text-style: bold;
    }
    """
    
    def __init__(self, rows):
        super().__init__()
        self.rows = rows
        self.total_size = 0
        self.current_focus = "envs"  # Track which table has focus

    def compose(self) -> ComposeResult:
        with Vertical():
            yield Label("Python Virtual Environment Scanner", id="title")
            yield ProgressBar(total=1, id="scan-progress")
            yield Label("Ready", id="status")
            with Horizontal():
                yield DataTable(id="envs")
                with Vertical():
                    yield Input(placeholder="Search packages...", id="search-input")
                    yield DataTable(id="packages")
            yield Label("Total Size: 0 MB", id="footer")

    def on_mount(self):
        env_table = self.query_one("#envs", DataTable)
        pkg_table = self.query_one("#packages", DataTable)
        env_table.add_columns(
            "Environment Path",
            "Python Version",
            "Size (MB)"
        )
        pkg_table.add_column("Installed Packages")
        env_table.focus()
        self.run_scan()

    def show_packages(self, row_index):
        if row_index < len(self.rows):
            pkg_table = self.query_one("#packages", DataTable)
            search_input = self.query_one("#search-input", Input)
            pkg_table.clear()
            packages = sorted(self.rows[row_index][3])
            search_term = search_input.value.lower()
            
            for package in packages:
                if search_term in package.lower():
                    pkg_table.add_row(package)

    def on_input_changed(self, event: Input.Changed) -> None:
        if event.input.id == "search-input":
            env_table = self.query_one("#envs", DataTable)
            if env_table.row_count > 0:
                self.show_packages(env_table.cursor_row)

    def on_key(self, event):
        env_table = self.query_one("#envs", DataTable)
        pkg_table = self.query_one("#packages", DataTable)
        search_input = self.query_one("#search-input", Input)
        
        if self.current_focus == "envs":
            if event.key == "right":
                event.prevent_default()
                self.current_focus = "packages"
                search_input.focus()
            elif event.key in ("up", "down"):
                self.show_packages(env_table.cursor_row)
                
        elif self.current_focus == "packages":
            if event.key == "left":
                event.prevent_default()
                self.current_focus = "envs"
                env_table.focus()
            elif event.key in ("up", "down"):
                event.prevent_default()

    @work(thread=True)
    def run_scan(self):
        env_table = self.query_one("#envs", DataTable)
        progress = self.query_one("#scan-progress", ProgressBar)
        status = self.query_one("#status", Label)
        footer = self.query_one("#footer", Label)

        self.call_from_thread(env_table.clear)
        self.rows.clear()
        sl.clear()
        self.total_size = 0
        self.call_from_thread(status.update, "Scanning home directory for virtual environments...")

        result = sp.run(
            ["find", os.path.expanduser("~"), "-type", "f", "-name", "pyvenv.cfg"],
            stdout=sp.PIPE, stderr=sp.DEVNULL, text=True
        )

        l = [p for p in result.stdout.split("\n") if p]
        self.call_from_thread(setattr, progress, "total", len(l) if l else 1)
        self.call_from_thread(setattr, progress, "progress", 0)

        if not l:
            self.call_from_thread(status.update, "No virtual environments found.")
            return

        for venv_path in l:
            with open(venv_path, "r") as f:
                content = f.read()

            rm_pycnf = venv_path.rfind("/")

            version_match = re.search(r"version = (\d+\.\d+\.\d+)", content)
            if version_match:
                version = version_match.group(1)
                full_ver = version_match.group(1)
                second_dot_pos = full_ver.find(".",2)

                site_packages_path = f"{venv_path[:rm_pycnf]}/lib/python{version_match.group(1)[:second_dot_pos]}/site-packages"

                try:
                    packages = os.listdir(site_packages_path)
                    unified_packages = unify_pkg_names(packages)

                    size = sp.run(["du", "-shm", site_packages_path], stdout=sp.PIPE, stderr=sp.DEVNULL, text=True)
                    env_size = int(size.stdout.split("\t")[0])
                    self.total_size += env_size

                    display_path = venv_path.replace(os.path.expanduser("~"), "~").replace("/pyvenv.cfg", "")
                    row = (env_size, version, display_path, unified_packages)
                    sl.add(row)
                    self.rows.append(row)
                except FileNotFoundError:
                    pass

            self.call_from_thread(progress.advance, 1)

        # Populate table from sorted list
        for row in sl:
            self.call_from_thread(env_table.add_row, row[2], row[1], str(row[0]))

        self.call_from_thread(status.update, f"Scan complete. Total size: {self.total_size} MB")
        self.call_from_thread(footer.update, f"Total Size: {self.total_size} MB")

def unify_pkg_names(pkg_list):
    # Original logic preserved
    unified_list = set()
    for pkg in pkg_list:
        if "-" in pkg:
            unified_list.add(pkg.split("-")[0])
        elif "." in pkg:
            unified_list.add(pkg.split(".")[0])
        else:
            unified_list.add(pkg)
    return unified_list

def main():
    rows = list(sl)
    EnvTable(rows).run()

if __name__ == "__main__":
    main()