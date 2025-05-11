cmake_minimum_required(VERSION 3.10)

# Project name and language
project(MOShell C)

# Set the C standard
set(CMAKE_C_STANDARD 99)

# Add source directory
add_executable(moshell src/MOshell.c)

