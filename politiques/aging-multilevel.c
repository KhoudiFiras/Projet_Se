#include "process.h"
#include <stdio.h>

/* État interne pour le Round Robin et la mise à jour */
static int current_idx = -1;    
static int rr_time = 0;         
static int last_idx = -1;       

/* * Fonction utilitaire pour l'arrondi standard (Round Half Up)
 * 3.1 -> 3 | 3.4 -> 3 | 3.5 -> 4 | 3.9 -> 4
 */
int custom_round(float num) {
    return (int)(num + 0.5f);
}

/*
 * Recalcul des priorités
 * Formule: (Temps d'attente + Temps CPU restant) / Temps CPU Restant
 */
void update_priorities(Process *proc, int n, int time) {
    for (int i = 0; i < n; i++) {
        // On ne met à jour que les processus vivants (READY ou RUNNING)
        if ((proc[i].state == READY || proc[i].state == RUNNING) && proc[i].remaining > 0) {
            
            // 1. Calculer le temps d'attente réel à l'instant T
            // Attente = Temps écoulé total - (Temps d'arrivée + Temps déjà exécuté sur CPU)
            int executed = proc[i].burst - proc[i].remaining;
            int waiting_time = time - proc[i].arrival - executed;
            
            // Sécurité pour éviter les valeurs négatives (si horloge instable)
            if (waiting_time < 0) waiting_time = 0;

            // 2. Appliquer la formule
            float ratio = (float)(waiting_time + proc[i].remaining) / (float)proc[i].remaining;

            // 3. Appliquer l'arrondi et mettre à jour
            proc[i].priority = custom_round(ratio);
        }
    }
}

/* Reset de l'état */
void policy_reset() {
    current_idx = -1;
    rr_time = 0;
    last_idx = -1;
}

/*
 * Algorithme Multilevel Dynamique
 * - Met à jour les priorités tous les 5 ticks.
 * - Choisi la PLUS GRANDE valeur de priorité (car la formule grandit avec l'attente).
 * - Applique un Round-Robin pour les égalités.
 */
int policy_select(Process *proc, int n, int time, int quantum) {

    // 1. Arrivée des processus
    for (int i = 0; i < n; i++) {
        if (proc[i].state == NEW && proc[i].arrival <= time) {
            proc[i].state = READY;
            // Priorité initiale calculée immédiatement pour être juste
            // Au début: Waited = 0, donc (0 + R)/R = 1
        }
    }

    // 2. Mise à jour dynamique des priorités tous les 5 ticks
    // On le fait si time > 0 et time est un multiple de 5
    if (time > 0 && (time % 5 == 0)) {
        update_priorities(proc, n, time);
    }

    // 3. Trouver la MEILLEURE priorité
    // ATTENTION : Avec cette formule, plus le chiffre est grand, plus c'est urgent.
    // On cherche donc le MAX.
    int best_prio = -1; 
    
    for (int i = 0; i < n; i++) {
        if ((proc[i].state == READY || proc[i].state == RUNNING) && proc[i].remaining > 0) {
            // On cherche le strict maximum
            if (proc[i].priority > best_prio) {
                best_prio = proc[i].priority;
            }
        }
    }

    // Aucun processus disponible
    if (best_prio == -1) {
        current_idx = -1;
        return -1;
    }

    // 4. Gestion du processus courant (Préemption & Quantum)
    if (current_idx != -1) {
        // A. Processus terminé
        if (proc[current_idx].remaining <= 0) {
            proc[current_idx].state = FINISHED;
            proc[current_idx].end_time = time;
            current_idx = -1;
            rr_time = 0;
        }
        // B. Préemption : Une priorité plus haute (plus grande valeur) existe
        else if (proc[current_idx].priority < best_prio) {
            proc[current_idx].state = READY;
            current_idx = -1;
            rr_time = 0;
        }
        // C. Round Robin : Même priorité, vérification du quantum
        else if (proc[current_idx].priority == best_prio) {
            if (rr_time >= quantum) {
                proc[current_idx].state = READY;
                current_idx = -1;
                rr_time = 0;
            } else {
                rr_time++;
                return current_idx; // On continue
            }
        }
    }

    // 5. Sélection du prochain (Round Robin Circulaire parmi les 'best_prio')
    int start_search = (last_idx + 1) % n;
    int candidate = -1;

    for (int i = 0; i < n; i++) {
        int idx = (start_search + i) % n;
        
        if ((proc[idx].state == READY || proc[idx].state == RUNNING) &&
            proc[idx].remaining > 0 &&
            proc[idx].priority == best_prio) {
            
            candidate = idx;
            break;
        }
    }

    // 6. Lancement du candidat
    if (candidate != -1) {
        current_idx = candidate;
        last_idx = candidate;
        proc[current_idx].state = RUNNING;

        if (proc[current_idx].burst == proc[current_idx].remaining) {
            proc[current_idx].start_time = time;
        }
        
        rr_time = 1;
        return current_idx;
    }

    return -1;
}
