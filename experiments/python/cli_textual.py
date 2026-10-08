import os
import re
import subprocess as sp
from textual import work
from textual.app import App, ComposeResult
from textual.containers import Container
from textual.widgets import DataTable, Footer, Header, Label, LoadingIndicator


def unify_pkg_names(pkg_list):
    # Preserved 1:1 from original logic
    unified_list = set()
    for pkg in pkg_list:
        if "-" in pkg:
            unified_list.add(pkg.split("-")[0])
        elif "." in pkg:
            unified_list.add(pkg.split(".")[0])
        else:
            unified_list.add(pkg)
    return unified_list


class VenvScannerApp(App):
    TITLE = "Python Venv Scanner"
    SUB_TITLE = "Local Environment Storage Analyzer"

    CSS = """
    Screen {
        layout: vertical;
    }
    #loader {
        height: 1fr;
        content-align: center middle;
    }
    DataTable {
        height: 1fr;
    }
    #status-bar {
        dock: bottom;
        height: 3;
        background: $boost;
        color: $text;
        content-align: center middle;
        text-style: bold;
    }
    """

    BINDINGS = [
        ("q", "quit", "Quit"),
        ("r", "action_rescan", "Rescan Drive"),
    ]

    def compose(self) -> ComposeResult:
        yield Header(show_clock=True)
        yield LoadingIndicator(id="loader")
        yield DataTable(id="venv-table")
        yield Label("Initializing system scan...", id="status-bar")
        yield Footer()

    def on_mount(self) -> None:
        table = self.query_one(DataTable)
        table.cursor_type = "row"
        table.zebra_stripes = True
        table.add_columns("Environment Path", "Python", "Unique Pkgs", "Size")
        table.display = False  # Hide table until the first scan finishes
        self.run_scan()

    def action_rescan(self) -> None:
        self.run_scan()

    @work(thread=True)
    def run_scan(self) -> None:
        """Runs your exact original blocking script inside a background thread."""
        self.call_from_thread(self._prep_ui_for_scan)

        # --- ORIGINAL FIND SUBPROCESS ---
        result = sp.run(
            ["find", os.path.expanduser("~"), "-type", "f", "-name", "pyvenv.cfg"],
            stdout=sp.PIPE,
            stderr=sp.DEVNULL,
            text=True,
        )

        l = result.stdout.split("\n")
        if l and not l[-1]:
            l.pop()

        total_size = 0

        for venv_path in l:
            try:
                with open(venv_path, "r") as f:
                    content = f.read()

                version_match = re.search(r"version = (\d+\.\d+\.\d+)", content)
                if version_match:
                    raw_ver = version_match.group(1)

                    # Preserving your exact [:-11] path math and string slice logic
                    # (Safely upgraded [:4] to split('.')[:2] to prevent Python 3.9 breaking)
                    py_major_minor = ".".join(raw_ver.split(".")[:2])
                    site_packages_path = (
                        f"{venv_path[:-11]}/lib/python{py_major_minor}/site-packages"
                    )

                    packages = (
                        os.listdir(site_packages_path)
                        if os.path.exists(site_packages_path)
                        else []
                    )

                    # --- ORIGINAL UNIFY LOGIC ---
                    unified_packages = unify_pkg_names(packages)

                    # --- ORIGINAL DU SUBPROCESS ---
                    size_proc = sp.run(
                        ["du", "-shm", site_packages_path],
                        stdout=sp.PIPE,
                        stderr=sp.DEVNULL,
                        text=True,
                    )

                    size_mb = (
                        int(size_proc.stdout.split("\t")[0])
                        if size_proc.stdout
                        else 0
                    )
                    total_size += size_mb

                    clean_path = venv_path.replace(os.path.expanduser("~"), "~")[:-11]

                    # Push row to the live UI thread safely
                    self.call_from_thread(
                        self._populate_row,
                        clean_path,
                        raw_ver,
                        str(len(unified_packages)),
                        f"{size_mb} MB",
                    )
            except Exception:
                continue

        self.call_from_thread(self._finish_scan, len(l), total_size)

    # --- Thread-Safe UI Mutators ---

    def _prep_ui_for_scan(self):
        self.query_one(DataTable).clear()
        self.query_one(DataTable).display = False
        self.query_one(LoadingIndicator).display = True
        self.query_one("#status-bar", Label).update(
            "Traversing home directory for pyvenv.cfg..."
        )

    def _populate_row(self, path: str, ver: str, pkgs: str, size: str):
        self.query_one(DataTable).add_row(path, ver, pkgs, size)

    def _finish_scan(self, total_envs: int, total_mb: int):
        self.query_one(LoadingIndicator).display = False
        table = self.query_one(DataTable)
        table.display = True
        table.focus()
        self.query_one("#status-bar", Label).update(
            f" Scanned {total_envs} Environments  │  Total Disk Footprint: {total_mb} MB "
        )


if __name__ == "__main__":
    app = VenvScannerApp()
    app.run()