import subprocess as sp
import os
import re

def unify_pkg_names(pkg_list):
    unified_list = set()
    for pkg in pkg_list:
        if "-" in pkg:
            unified_list.add(pkg.split("-")[0])
        elif "." in pkg:
            unified_list.add(pkg.split(".")[0])
        else:
            unified_list.add(pkg)
    return unified_list

#Finding all pyenv.cfg files in the home directory and its subdirectories
#It gives path
result = sp.run(
    ["find", os.path.expanduser("~"), "-type", "f", "-name", "pyvenv.cfg"],
    stdout=sp.PIPE,
    stderr=sp.DEVNULL,
    text=True
)

l = result.stdout.split("\n")
l.pop()


print("Virtual environments found:")
# for venv_path, path in enumerate(l):
#     print(f"{venv_path+1}. {path}")

total_size = 0

for venv_path in l:
    f = open(venv_path, "r")
    # print(f"Virtual environment at: {venv_path}")

    #Get the version from the file using regex
    content = f.read()
    version_match = re.search(r"version = (\d+\.\d+\.\d+)", content)
    if version_match:
        
        #Get the packages list from ./lib/pythonX.X/site-packages
        
        site_packages_path = f"{venv_path[:-11]}/lib/python{version_match.group(1)[:4]}/site-packages"
        print(site_packages_path)


        packages = []

        for item in os.listdir(site_packages_path):
            packages.append(item)
        #print(packages)

        #Unifying package names to avoid duplicates django and django-12.2.dist-info
        unified_packages = unify_pkg_names(packages)
        # print("Installed packages and tools: ", unified_packages)

        # for a in packages:
        #     if a not in unified_packages:
        #         print(a)

        size = sp.run(["du","-shm", site_packages_path], stdout=sp.PIPE, stderr=sp.DEVNULL, text=True)
        print("Total size of installed packages and tools: ",size.stdout.split("\t")[0])
        total_size += int(size.stdout.split("\t")[0])
    f.close()

print(f"Total size of all virtual environments: {total_size} MB")