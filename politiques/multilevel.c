#include "process.h"

/* * État interne de l'ordonnanceur 
 */
static int current_idx = -1;    // Indice du processus en cours d'exécution
static int rr_time = 0;         // Temps écoulé dans le quantum actuel
static int last_idx = -1;       // Pour mémoriser la position pour le Round-Robin

/*
 * Réinitialisation de l'état (appelé par le simulateur au début)
 */
void policy_reset() {
    current_idx = -1;
    rr_time = 0;
    last_idx = -1;
}

/*
 * Algorithme Multilevel Queue (MLQ)
 * 1. Priorité Préemptive entre les niveaux.
 * 2. Round-Robin (Tourniquet) au sein du même niveau.
 */
int policy_select(Process *proc, int n, int time, int quantum) {
    
    // 1. Mettre à jour les états : NEW -> READY
    for (int i = 0; i < n; i++) {
        if (proc[i].state == NEW && proc[i].arrival <= time) {
            proc[i].state = READY;
        }
    }

    // 2. Trouver la meilleure priorité (plus petite valeur) disponible
    int best_prio = -1;
    
    for (int i = 0; i < n; i++) {
        // On cherche parmi les processus prêts ou en cours qui ont encore du travail
        if ((proc[i].state == READY || proc[i].state == RUNNING) && proc[i].remaining > 0) {
            if (best_prio == -1 || proc[i].priority < best_prio) {
                best_prio = proc[i].priority;
            }
        }
    }

    // S'il n'y a aucun processus disponible
    if (best_prio == -1) {
        current_idx = -1;
        return -1;
    }

    // 3. Gestion du processus courant (s'il existe)
    if (current_idx != -1) {
        // Cas A : Le processus a terminé son exécution
        if (proc[current_idx].remaining <= 0) {
            proc[current_idx].state = FINISHED;
            proc[current_idx].end_time = time;
            current_idx = -1;
            rr_time = 0;
        }
        // Cas B : Préemption par priorité (un processus plus prioritaire est arrivé)
        else if (proc[current_idx].priority > best_prio) {
            proc[current_idx].state = READY;
            // On ne reset pas last_idx ici pour garder l'équité future, mais on change de focus
            current_idx = -1; 
            rr_time = 0;
        }
        // Cas C : Même niveau de priorité -> Vérification du Quantum (Round Robin)
        else if (proc[current_idx].priority == best_prio) {
            if (rr_time >= quantum) {
                // Quantum écoulé -> Yield
                proc[current_idx].state = READY;
                current_idx = -1;
                rr_time = 0;
            } else {
                // On continue avec le même processus
                rr_time++;
                return current_idx;
            }
        }
    }

    // 4. Sélection du prochain processus (Round Robin dans le niveau best_prio)
    // On cherche circulairement à partir de la dernière position (last_idx)
    // pour garantir l'équité du tourniquet.
    
    int start_search = (last_idx + 1) % n;
    int candidate = -1;

    for (int i = 0; i < n; i++) {
        int idx = (start_search + i) % n;
        
        if ((proc[idx].state == READY || proc[idx].state == RUNNING) &&
            proc[idx].remaining > 0 &&
            proc[idx].priority == best_prio) {
            
            candidate = idx;
            break; // On a trouvé le suivant dans la file virtuelle
        }
    }

    // 5. Mise à jour de l'état pour le nouveau processus élu
    if (candidate != -1) {
        current_idx = candidate;
        last_idx = candidate; // On mémorise qu'il a eu la main
        proc[current_idx].state = RUNNING;
        
        // Si c'est la toute première fois qu'il tourne (optionnel, selon simulateur)
        if (proc[current_idx].burst == proc[current_idx].remaining) {
            proc[current_idx].start_time = time;
        }
        
        rr_time = 1; // On entame le premier tick du quantum
        return current_idx;
    }

    return -1;
}
