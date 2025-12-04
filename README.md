# Ordonnanceur Multitâche Sous Linux


## Présentation
Ce projet implémente un simulateur d’**ordonnancement multitâche de processus sous Linux**, permettant de visualiser et tester plusieurs politiques d’ordonnancement classiques.


Le programme charge dynamiquement les politiques d’ordonnancement depuis des modules `.so`, lit un fichier décrivant un ensemble de processus, puis simule leur exécution via :


- une **interface graphique GTK** avec **diagramme de Gantt**


---


## Fonctionnalités principales


### Version minimale
1- Lecture d’un fichier de configuration des processus :
  - nom
  - date d’arrivée
  - durée (burst)
  - priorité
  - commentaires + lignes vides gérés
- Affichage textuel de la simulation
- Politiques d’ordonnancement implémentées :
  - **FIFO**
  - **Round Robin**
  - **Priorité préemptive**
- Makefile complet pour compiler le projet et générer les `.so`


### Fonctionnalités avancées
- Politique avancée : **Aging / Multi-Level** (priorité dynamique)
- IHM graphique GTK+ 3 avec suivi de la simulation en temps réel
- Diagramme de Gantt graphique
- Chargement dynamique des politiques (`dlopen`, `.so`)


---


## Structure du projet
Project_Se/


    ├── Makefile


    ├── sample_config.txt


    ├── scheduler


    ├── include/


      ├── process.h    (Struct Process { String [ ] nom , int temp_arrivé , int temp_execution }


    ├── src/
  
      ├── main.c


      ├── parser.c 


      ├── policies_loader.c


      ├── utils.c
     
      └── gui.c


    ├── politiques/


      ├── fifo.c


      ├── roundrobin.c


      ├── priority.c


      └── Aging.c


    └── README.md




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
proc  Tableau des processus
n     Nombre total
time  Temps courant
quantum     Quantum (RR uniquement)


La fonction renvoie l’index du processus à exécuter, ou -1 si aucun n’est prêt.


Compilation & exécution






Installer GTK (si nécessaire)
``` 
sudo apt install libgtk-3-dev


make


./scheduler


make clean


```
## Interface graphique
L’IHM GTK permet :


- charger un fichier de processus


- sélectionner une politique (détectée automatiquement dans politiques/)


- suivre la simulation en temps réel


- afficher un diagramme de Gantt interactif




## Licence
Licence : GENERAL PUBLIC LICENSE


Certaines parties de ce projet, notamment les fonctions utilitaires, le code de l'interface graphique GTK et la gestion des processus, ont pu être générées ou assistées par des outils d'intelligence 
artificielle (par exemple ChatGPT). Ces parties sont également distribuées sous la 
GNU General Public License v3.0.


## Équipe (SCRUM)
Travail réalisé en groupe scrum (5–7 membres) :


Sprint Planning


Daily meetings


Sprint Review


Retrospective


Le rapport SCRUM est fourni en PDF dans le rendu final.
