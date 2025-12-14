
# Process Scheduler Simulator

A powerful, interactive, and visual Process Scheduler Simulator built in C using GTK+ 3.0. This application allows you to simulate, visualize, and compare different CPU scheduling algorithms in real-time.

## Features

*   **Real-time Visualization**: Watch the Gantt chart build dynamically as processes are scheduled.
*   **Interactive Controls**: Start, Pause, Reset, and even "Kill" running processes on the fly.
*   **Pluggable Architecture**: Scheduling policies are loaded dynamically (`.so` files), allowing you to add new algorithms without recompiling the main app.
*   **3D Statistics**: View real-time 3D isometric bar charts comparing Average Turnaround Time (TAT) and Waiting Time (WT).
*   **Detailed Metrics**: Track process states (New, Ready, Running, Finished) and history of simulation runs.
*   **Custom Configuration**: Load process lists from text files and adjust the Time Quantum for Round Robin.

## Technologies Used

*   **Language**: C (C99/POSIX)
*   **GUI Framework**: GTK+ 3.0
*   **Graphics**: Cairo (for Gantt charts and 3D stats)
*   **Build System**: GNU Make
*   **Dynamic Linking**: `dlfcn` for runtime policy loading

## Prerequisites

You need a Linux environment with GCC and GTK+ 3.0 development libraries installed.

```bash
# Ubuntu / Debian
sudo apt-get update
sudo apt-get install build-essential libgtk-3-dev
```

## Installation & Build

1.  **Clone the repository** (or extract the archive):
    ```bash
    cd Projet_Se
    ```

2.  **Compile the project**:
    This will build the main `scheduler` executable and all policy shared objects in `politiques/`.
    ```bash
    make
    ```

3.  **Clean up** (optional):
    To remove compiled object files and executables:
    ```bash
    make clean
    ```

## Usage

1.  **Run the Simulator**:
    ```bash
    ./scheduler 
    ./scheduler sample_config.txt
    ```

2.  **Load Processes**:
    *   Click **Parcourir** (Browse) and select a configuration file (e.g., `sample_config.txt`).
    *   *File Format*: `Name Arrival Burst Priority` (one process per line).

3.  **Select Policy**:
    *   Choose a scheduling algorithm from the dropdown (e.g., `fifo`, `rr`, `priority`).
    *   If you choose Round Robin (`rr`), adjust the **Quantum** spinner.

4.  **Control Simulation**:
    *   Click **▶️ Démarrer** to start.
    *   Click **⏸️ Pause** to pause.
    *   Click **🔄 Réinitialiser** to reset the simulation and reload the config.

## 📂 Project Structure

```
├── include
│   └── process.h
├── LICENSE
├── Makefile
├── politiques
│   ├── aging-multilevel.c
│   ├── fifo.c
│   ├── multilevel.c
│   ├── priority.c
│   ├── process.h
│   └── rr.c
├── README.md
├── sample_config.txt
├── src
│   ├── gui.c
│   ├── parser.c
│   └── policies_loader.c
└── style.css
```

## Adding New Policies

You can add new scheduling algorithms without touching the core code!

1.  Create a new `.c` file in `politiques/` (e.g., `sjf.c`).
2.  Implement the `policy_select` function:
    ```c
    #include "process.h"
    int policy_select(Process *proc, int n, int time, int quantum) {
        // Return the index of the chosen process
    }
    ```
3.  Run `make` again. The new policy will automatically appear in the dropdown menu!
=======



## Fonctionnalités principales

1- Lecture d’un fichier de configuration des processus :
  - nom
  - date d’arrivée
  - durée (burst)
  - priorité
  - commentaires + lignes vides gérés
2- Politiques d’ordonnancement implémentées :
  - **FIFO**
  - **Round Robin**
  - **Priorité préemptive**
  - **Aging (priorité dynamique)** 
  - **Multi-Level**
3- Makefile complet pour compiler le projet et générer les `.so`

4- IHM graphique GTK+ 3 avec suivi de la simulation en temps réel
  - Diagramme de Gantt graphique
5- Chargement dynamique des politiques (`dlopen`, `.so`)


---




## Format du fichier de configuration
Exemple de fichier processus :


\# name arrival burst priority


P1 0 5 2


P2 1 6 1


P3 2 2 3


---


## API des politiques d’ordonnancement
Chaque politique d’ordonnancement doit implémenter :


```c
int policy_select(Process *proc, int n, int time, int quantum);


```
Paramètre   Description
- proc : Tableau des processus
- n   :  Nombre total
- time  :Temps courant
- quantum    : Quantum (RR uniquement)


La fonction renvoie l’index du processus à exécuter, ou -1 si aucun n’est prêt.





## Interface graphique
L’IHM GTK permet :


- charger un fichier de processus


- sélectionner une politique (détectée automatiquement dans politiques/)


- suivre la simulation en temps réel


- afficher un diagramme de Gantt interactif




## Licence
Licence : GENERAL PUBLIC LICENSE



## Équipe (SCRUM)
Travail réalisé en groupe scrum (5 membres) :

- KHEMIRI Eya

- AYADI Ichrak

- SATOURI Ranim

- KHOUDI Firas

- SASSI Naziha



