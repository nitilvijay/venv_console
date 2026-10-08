import streamlit as st
import subprocess as sp
import os
import re
import pandas as pd

def unify_pkg_names(pkg_list):
    # Original logic
    unified_list = set()
    for pkg in pkg_list:
        if "-" in pkg:
            unified_list.add(pkg.split("-")[0])
        elif "." in pkg:
            unified_list.add(pkg.split(".")[0])
        else:
            unified_list.add(pkg)
    return unified_list

st.set_page_config(page_title="Venv Scanner", page_icon="🐍")
st.title("🐍 Virtual Environment Storage Scanner")

if st.button("Scan My System"):
    with st.spinner('Scanning home directory (this might take a few seconds)...'):
        result = sp.run(
            ["find", os.path.expanduser("~"), "-type", "f", "-name", "pyvenv.cfg"],
            stdout=sp.PIPE, stderr=sp.DEVNULL, text=True
        )
        
    paths = [p for p in result.stdout.split("\n") if p]
    total_size = 0
    data = []

    progress_bar = st.progress(0)
    for i, venv_path in enumerate(paths):
        with open(venv_path, "r") as f:
            content = f.read()
            
        version_match = re.search(r"version = (\d+\.\d+\.\d+)", content)
        if version_match:
            version = version_match.group(1)
            site_packages_path = f"{venv_path[:-11]}/lib/python{version[:4]}/site-packages"
            
            try:
                packages = os.listdir(site_packages_path)
                size = sp.run(["du", "-shm", site_packages_path], stdout=sp.PIPE, stderr=sp.DEVNULL, text=True)
                env_size = int(size.stdout.split("\t")[0])
                total_size += env_size
                
                display_path = venv_path.replace(os.path.expanduser("~"), "~").replace("/pyvenv.cfg", "")
                data.append({"Path": display_path, "Python Version": version, "Size (MB)": env_size})
            except FileNotFoundError:
                pass
                
        progress_bar.progress((i + 1) / len(paths))

    st.success("Scan Complete!")
    
    # Display metrics and interactive dataframe
    col1, col2 = st.columns(2)
    col1.metric("Total Environments", len(data))
    col2.metric("Total Storage Used", f"{total_size} MB")
    
    df = pd.DataFrame(data)
    st.dataframe(df, use_container_width=True)