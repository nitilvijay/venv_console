import subprocess as sp
import os
import re
from sortedcontainers import SortedList #sophisticated block-based data structure that's extremely fast in Python.
from textual.app import App, ComposeResult
from textual.containers import Horizontal, Vertical
from textual.widgets import DataTable, Label, ProgressBar

sl = SortedList()

class EnvTable(App):
    def __init__(self, rows):
        super().__init__()
        self.rows = rows
        self.total_size = 0

    def compose(self) -> ComposeResult:
        with Vertical():
            yield Label("Python Virtual Environment Scanner", id="title")
            yield ProgressBar(total=1, id="scan-progress")
            yield Label("Ready", id="status")
            with Horizontal():
                yield DataTable(id="envs")
                yield DataTable(id="packages")

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
        pkg_table = self.query_one("#packages", DataTable)
        pkg_table.clear()
        packages = sorted(self.rows[row_index][3])
        for package in packages:
            pkg_table.add_row(package)

    def on_key(self, event):
        if event.key == "enter":
            table = self.query_one("#envs", DataTable)
            self.show_packages(table.cursor_row)

    def run_scan(self):
        env_table = self.query_one("#envs", DataTable)
        progress = self.query_one("#scan-progress", ProgressBar)
        status = self.query_one("#status", Label)

        env_table.clear()
        self.rows.clear()
        sl.clear()
        self.total_size = 0
        status.update("Scanning home directory for virtual environments...")

        result = sp.run(
            ["find", os.path.expanduser("~"), "-type", "f", "-name", "pyvenv.cfg"],
            stdout=sp.PIPE, stderr=sp.DEVNULL, text=True
        )

        l = [p for p in result.stdout.split("\n") if p]
        progress.total = len(l) if l else 1
        progress.progress = 0

        if not l:
            status.update("No virtual environments found.")
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

            progress.advance(1)

        # Populate table from sorted list
        for row in sl:
            env_table.add_row(row[2], row[1], str(row[0]))

        status.update(f"Scan complete. Total size: {self.total_size} MB")

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