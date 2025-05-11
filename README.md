## MOShell
As part of my journey to learn more about programming and Linux, this is one of the projects I created in C! It’s a simple shell written purely in C.
How to Run This Project:

```
git clone https://github.com/Ramarajusairajesh/MOShell/
cd MOShell/src
gcc ./MOshell.c
./a.out
```
About the Project:

This shell supports basic command execution, similar to how a terminal works. It's designed to help me understand how shells parse input, handle system calls like fork() and execvp(), and manage user interaction at a low level. While it's still a work in progress, it demonstrates core concepts such as:

    Reading user input from the terminal

    Tokenizing commands

    Executing system commands

    Handling basic errors

If you are a guy who uses CMake for anything, here you go:
```
mkdir build
cd build
cmake ..
make
./moshell
```
but just use gcc directly for this to be simple !
