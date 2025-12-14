# Process Scheduler Simulator
A powerful, interactive, and visual Process Scheduler Simulator built in C using GTK+ 3.0. This application allows you to simulate, visualize, and compare different CPU scheduling algorithms in real-time.

## Features
* **Real-time Visualization**: Watch the Gantt chart build dynamically as processes are scheduled.
* **Interactive Controls**: Start, Pause, Reset, and even "Kill" running processes on the fly.
* **Pluggable Architecture**: Scheduling policies are loaded dynamically (.so files), allowing you to add new algorithms without recompiling the main app.
* **3D Statistics**: View real-time 3D isometric bar charts comparing Average Turnaround Time (TAT) and Waiting Time (WT).
* **Detailed Metrics**: Track process states (New, Ready, Running, Finished) and history of simulation runs.
* **Custom Configuration**: Load process lists from text files and adjust the Time Quantum for Round Robin.

## Technologies Used
* **Language**: C (C99/POSIX)
* **GUI Framework**: GTK+ 3.0
* **Graphics**: Cairo (for Gantt charts and 3D stats)
* **Build System**: GNU Make
* **Dynamic Linking**: dlfcn for runtime policy loading

## Prerequisites
You need a Linux environment with GCC and GTK+ 3.0 development libraries installed.
```bash
# Ubuntu / Debian
sudo apt-get update
sudo apt-get install build-essential libgtk-3-dev
```

## Installation & Build
1. **Clone the repository** (or extract the archive):
    ```bash
    cd Projet_Se
    ```
2. **Compile the project**:
    This will build the main scheduler executable and all policy shared objects in politiques/.
    ```bash
    make
    ```
3. **Clean up** (optional):
    To remove compiled object files and executables:
    ```bash
    make clean
    ```

## Usage
1. **Run the Simulator**:
    ```bash
    ./scheduler
    ./scheduler sample_config.txt
    ```
2. **Load Processes**:
    * Click Browse and select a configuration file (e.g., sample_config.txt).
    * *File Format*: Name Arrival Burst Priority (one process per line).
3. **Select Policy**:
    * Choose a scheduling algorithm from the dropdown (e.g., fifo, rr, priority).
    * If you choose Round Robin (rr), adjust the Quantum spinner.
4. **Control Simulation**:
    * Click Start to start.
    * Click Pause to pause.
    * Click Reset to reset the simulation and reload the config.

## Project Structure
```
├── include
│   └── process.h
├── LICENSE
├── Makefile
├── politiques
│   ├── aging-multilevel.c
│   ├── fifo.c
│   ├── multilevel.c
│   ├── priority.c
│   ├── process.h
│   └── rr.c
├── README.md
├── sample_config.txt
├── src
│   ├── gui.c
│   ├── parser.c
│   └── policies_loader.c
└── style.css
```

## Adding New Policies
You can add new scheduling algorithms without touching the core code!
1. Create a new .c file in politiques/ (e.g., sjf.c).
2. Implement the policy_select function:
    ```c
    #include "process.h"
    int policy_select(Process *proc, int n, int time, int quantum) {
        // Return the index of the chosen process
    }
    ```
3. Run make again. The new policy will automatically appear in the dropdown menu!

=======

## Main Features
1- Reading a process configuration file:
  - name
  - arrival date
  - duration (burst)
  - priority
  - comments + empty lines handled
2- Implemented scheduling policies:
  - **FIFO**
  - **Round Robin**
  - **Preemptive Priority**
  - **Aging (dynamic priority)**
  - **Multi-Level**
3- Complete Makefile to compile the project and generate the .so files
4- GTK+ 3 graphical UI with real-time simulation monitoring
  - Graphical Gantt chart
5- Dynamic loading of policies (dlopen, .so)
---

## Configuration File Format
Example process file:
# name arrival burst priority
P1 0 5 2

P2 1 6 1

P3 2 2 3


## Scheduling Policies API
Each scheduling policy must implement:
```c
int policy_select(Process *proc, int n, int time, int quantum);
```
Parameter Description
- proc : Array of processes
- n : Total number
- time : Current time
- quantum : Quantum (RR only)
The function returns the index of the process to execute, or -1 if none is ready.

## Graphical Interface
The GTK UI allows:
- loading a process file
- selecting a policy (automatically detected in politiques/)
- monitoring the simulation in real time
- displaying an interactive Gantt chart

## License
License: GENERAL PUBLIC LICENSE

## Team (SCRUM)
Work completed in a Scrum group (5 members):
- KHEMIRI Eya
- AYADI Ichrak
- SATOURI Ranim
- KHOUDI Firas
- SASSI Naziha
