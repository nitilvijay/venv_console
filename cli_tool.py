import subprocess as sp
import os
import re
from rich.console import Console
from rich.table import Table
from rich.progress import track
from rich.panel import Panel

console = Console()

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
    # Title panel
    console.print(Panel.fit("[bold blue]🐍 Python Virtual Environment Scanner[/bold blue]", border_style="cyan"))

    # Loading spinner for the slow `find` command
    with console.status("[bold green]Scanning home directory for virtual environments (this may take a moment)...[/bold green]"):
        result = sp.run(
            ["find", os.path.expanduser("~"), "-type", "f", "-name", "pyvenv.cfg"],
            stdout=sp.PIPE, stderr=sp.DEVNULL, text=True
        )

    l = [p for p in result.stdout.split("\n") if p]

    if not l:
        console.print("[bold red]No virtual environments found.[/bold red]")
        return

    # Initialize a beautiful terminal table
    table = Table(show_header=True, header_style="bold magenta")
    table.add_column("Environment Path", style="dim", overflow="fold")
    table.add_column("Python Version", justify="center")
    table.add_column("Size (MB)", justify="right", style="green")

    total_size = 0

    # Progress bar for the analysis phase
    for venv_path in track(l, description="Analyzing packages and sizes..."):
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
                total_size += env_size

                # Clean up path for display
                display_path = venv_path.replace(os.path.expanduser("~"), "~").replace("/pyvenv.cfg", "")
                table.add_row(display_path, version, str(env_size))
            except FileNotFoundError:
                # Silently skip if the lib folder structure is non-standard
                pass

    # Print the table and the final total
    console.print(table)
    console.print(f"\n[bold yellow]Total storage consumed by virtual environments: {total_size} MB[/bold yellow]")

if __name__ == "__main__":
    main()