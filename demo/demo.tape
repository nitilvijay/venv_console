# Records demo/demo.gif for the README.
# Run from the repo root after building and running demo/setup.sh:
#   vhs demo/demo.tape

Output demo/demo.gif

Set Shell "bash"
Set FontSize 16
Set Width 1200
Set Height 600
Set Padding 20
Set Theme "Catppuccin Mocha"
Set WindowBar Colorful
Set TypingSpeed 75ms

# Setup (not recorded): use the freshly built binary and a clean prompt
Hide
Type "export PATH=$PWD/build:$PATH PS1='$ ' && clear"
Enter
Show

Type "venvscan ~/venvscan-demo"
Sleep 500ms
Enter
Sleep 2s

# Browse environments (largest first); the package list follows the selection
Down
Sleep 1s
Down
Sleep 1s
Down
Sleep 1s

# Switch to the packages panel and scroll
Right
Sleep 500ms
Down@200ms 6
Sleep 1s

# Back to the largest venv and live-search its packages
Up@100ms 6
Left
Sleep 300ms
PageUp
Sleep 1s
Type "/"
Sleep 300ms
Type "num"
Sleep 1.5s
Escape
Sleep 1s

Type "q"
Sleep 1s
