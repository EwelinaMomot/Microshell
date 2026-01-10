# Microshell

Microshell is a simple Unix-like command-line shell implemented in C, compliant with POSIX.
The project demonstrates core operating system mechanisms such as process creation,
signal handling, job control, and basic file operations, while providing an interactive
command-line interface similar to standard UNIX shells.

Author: Ewelina Momot

---

## Overview of Functionality

Microshell provides the following groups of functionality:

- Interactive command prompt with colored output
- Execution of external system programs using `execvp()`
- Built-in commands for directory navigation and file management
- Tab-based autocompletion for files and directories
- Signal handling (Ctrl+C, Ctrl+Z, Ctrl+D)
- Basic job control with foreground and background processes
- Interactive and recursive options for file operations

All functionality is implemented manually without relying on existing shell frameworks.

---
## Installation and Compilation

Compile using the provided Makefile:

make

Clean the build:

make clean


## Usage

Run the shell:

./microshell


## Requirements

- POSIX-compliant operating system (Linux recommended)
- GCC or compatible C compiler
- Standard C and POSIX libraries
  
---
## Detailed Functionality Description

### 1. Interactive Prompt
- Displays the current login name and working directory
- Uses ANSI escape codes to show a colored prompt
- Automatically updates after directory changes
- Remains active even after child processes are interrupted or suspended

---

### 2. Input Handling
- Reads user input character by character in raw terminal mode
- Supports:
  - `ENTER` – execute command
  - `BACKSPACE` – delete last character
  - `TAB` – trigger autocompletion
  - `Ctrl+D` (EOF) – exit the shell gracefully
- Restores terminal settings on exit

---

### 3. Tab Completion
- Autocompletes file and directory names in the current working directory
- Supports:
  - single match completion
  - completion to the longest common prefix
  - listing all matching entries when multiple matches exist

---

### 4. Built-in Commands

#### cd
Changes the current working directory.
**Supported forms:**
```bash
cd <directory>
cd ~
cd -
cd ..
cd or cd ~: go to the home directory
cd <directory>: go to the specified directory
cd -: return to the previously visited directory
```

---
### cp
Copies files or directories.

### Usage
```bash
cp `<source>` `<destination>`  
cp `-i` `<source>` `<destination>`  
cp `-r` `<directory>` `<destination>`
```
### Behavior
- Copies regular files
- Recursively copies directories with `-r`
- Interactive mode `-i` asks before overwriting existing files
- Correctly handles copying into existing directories
- Displays appropriate error messages


---
## mv

Moves or renames files and directories.

### Usage
```bash
mv `<source>` `<destination>`  
mv `-i` `<source>` `<destination>`  
mv `-b` `<source>` `<destination>`
```
### Behavior
- Moves files or directories
- Renames files when destination is a filename
- Interactive overwrite confirmation with `-i`
- Backup mode `-b` creates a backup file with `~` suffix
- Uses `rename()` when possible, falls back to copy + unlink across filesystems
- Supports moving multiple files into a directory


---
## help

Displays a detailed description of all available commands.

---
## exit

Terminates the microshell. Restores terminal settings before exiting


---
## 5. External Command Execution

Executes system programs using `fork()` and `execvp()`.

### Details
- Uses the system `PATH` environment variable

### Examples
- ls
- pwd
- grep
- ping
- sleep

Displays a custom error message when a command is not found.


---
## 6. Signal Handling

### Ctrl+C (SIGINT)
- Sent to the foreground process
- Terminates the running program
- Does not terminate the shell

### Ctrl+Z (SIGTSTP)
- Suspends the foreground process
- Shell remains active and responsive

### Ctrl+D (EOF)
- Exits the shell cleanly


---
## 7. Job Control (Foreground / Background)

### fg
- Resumes the most recently suspended process
- Brings it to the foreground
- Shell waits until the process exits or is suspended again

### bg
- Resumes the most recently suspended process in the background
- Shell immediately returns to the prompt

### Job Control Details
- Each external command runs in its own process group
- Terminal control is transferred using `tcsetpgrp()`


---
## 8. Process and Terminal Management

- Microshell initializes and manages its own process group
- Foreground process groups receive terminal signals
- Background processes do not receive Ctrl+C or Ctrl+Z
- Behavior is consistent with standard UNIX shell semantics


cd <directory>: go to the specified directory

cd -: return to the previously visited directory
