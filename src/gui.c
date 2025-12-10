#define _POSIX_C_SOURCE 200809L
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include "process.h"

/* forward from parser and policy loader */
int parse_config(const char *path, Process *arr, int maxp);
int discover_policies(const char *dirpath, void *out, int max); // we will include the real type below
int load_policy(void *entry);    // prototypes exist in policies_loader.c but we will cast later
void unload_policy(void *entry);

/* Include the PolicyEntry definition here to avoid header clutter */
typedef struct {
    char path[512];
    char name[128];
    void *handle;
    void *symbol;
    void *reset_symbol;
} PolicyEntry;

/* Parameters */
#define MAX_PROC 128
#define MAX_POL 32
#define TICK_MS 500  /* GUI tick per simulation time unit in milliseconds */

/* Application state */
static Process procs[MAX_PROC];
static int proc_count = 0;
static int sim_time = 0;
static int finished = 0;
static bool sim_running = false;
static int quantum = 2;

/* Gantt history: record at each time which process index ran (-1 none) */
static int gantt[MAX_TIME];

/* Policies discovered */
static PolicyEntry policies[MAX_POL];
static int policy_count = 0;
static int current_policy_index = 0;

/* Function pointer type for policy_select */
typedef int (*policy_select_t)(Process*, int, int, int);

/* GTK widgets */
static GtkWidget *window;
static GtkWidget *entry_file;
static GtkWidget *combo_policy;
static GtkWidget *spin_quantum;
static GtkWidget *btn_start;
static GtkWidget *btn_pause;
static GtkWidget *btn_reset;
static GtkWidget *tree_ready;
static GtkWidget *tree_finished;
static GtkWidget *label_running;
static GtkWidget *drawing_gantt;
static GtkListStore *store_ready;
static GtkListStore *store_ready;
static GtkListStore *store_finished;
static GtkListStore *store_history;

static void discover_and_populate_policies();

/* Stats History */
typedef struct {
    char policy[128];
    int quantum;
    double avg_tat;
    double avg_wt;
} RunStats;

#define MAX_HISTORY 100
static RunStats history[MAX_HISTORY];
static int history_count = 0;
static bool stats_recorded = false;

/* Helper: update Ready queue view */
static void refresh_ready_view() {
    gtk_list_store_clear(store_ready);
    for (int i=0;i<proc_count;i++) {
        if (procs[i].state == READY || procs[i].state == NEW) {
            GtkTreeIter iter;
            gtk_list_store_append(store_ready, &iter);
            gtk_list_store_set(store_ready, &iter,
                               0, procs[i].name,
                               1, procs[i].arrival,
                               2, procs[i].burst,
                               3, procs[i].priority,
                               -1);
        }
    }
    
    /* Update Finished List */
    gtk_list_store_clear(store_finished);
    for (int i=0;i<proc_count;i++) {
        if (procs[i].state == FINISHED) {
            GtkTreeIter iter;
            gtk_list_store_append(store_finished, &iter);
            
            /* Determine status: Killed if remaining > 0, else Done */
            const char *status = (procs[i].remaining > 0) ? "Killed" : "Done";
            
            gtk_list_store_set(store_finished, &iter,
                               0, procs[i].name,
                               1, procs[i].arrival,
                               2, procs[i].end_time,
                               3, status,
                               -1);
        }
    }
}





static void record_current_run() {
    if (stats_recorded) return;
    if (history_count >= MAX_HISTORY) return;
    if (sim_time == 0) return; // Don't record empty runs

    double avg_tat = 0, avg_wt = 0;
    int n = 0;
    for (int i=0; i<proc_count; i++) {
        if (procs[i].state == FINISHED) {
            int tat = procs[i].end_time - procs[i].arrival;
            int wt = tat - procs[i].burst;
            avg_tat += tat;
            avg_wt += wt;
            n++;
        }
    }
    if (n > 0) { avg_tat /= n; avg_wt /= n; }
    else return; // No finished processes

    /* Save to history */
    strncpy(history[history_count].policy, policies[current_policy_index].name, 127);
    history[history_count].quantum = quantum;
    history[history_count].avg_tat = avg_tat;
    history[history_count].avg_wt = avg_wt;
    history_count++;
    stats_recorded = true;

    /* Update UI */
    GtkTreeIter iter;
    gtk_list_store_append(store_history, &iter);
    gtk_list_store_set(store_history, &iter,
                       0, policies[current_policy_index].name,
                       1, quantum,
                       2, avg_tat,
                       3, avg_wt,
                       -1);
}

static gboolean on_draw_gantt(GtkWidget *widget, cairo_t *cr, gpointer data) {
    (void)data;
    GtkAllocation alloc;

    gtk_widget_get_allocation(widget, &alloc);

    int w = alloc.width;
    int h = alloc.height;
    int left = 50, top = 30; // marge pour axe temps
    int row_h = 24;
    int cols = (sim_time + 10) > 1 ? (sim_time + 10) : 1;
    double cell_w = (double)(w - left - 20) / (cols);

    /* background */
    cairo_set_source_rgb(cr, 1,1,1);
    cairo_paint(cr);

    /* --- Axe du temps --- */
    cairo_set_source_rgb(cr, 0,0,0);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);
    for (int t=0; t<=cols && t<MAX_TIME; t++) {
        double x = left + t*cell_w;
        /* tick vertical */
        cairo_set_source_rgb(cr, 0.8,0.8,0.8);
        cairo_rectangle(cr, x, top-10, 1, h-top-20);
        cairo_fill(cr);

        /* label du temps tous les 1 ou 2 unités selon l'espace */
        if (t % 1 == 0) { // ici on peut mettre 1 ou 2 pour espacer
            cairo_set_source_rgb(cr, 0,0,0);
            char buf[16]; snprintf(buf, sizeof(buf), "%d", t);
            cairo_move_to(cr, x-4, top);
            cairo_show_text(cr, buf);
        }
    }



    /* draw process bars */
    for (int p = 0; p < proc_count; p++) {
        int y = top + p * row_h + 2;

        /* name */
        cairo_set_source_rgb(cr, 0,0,0);
        cairo_move_to(cr, 5, y+12);
        cairo_show_text(cr, procs[p].name);

        for (int t = 0; t <= sim_time && t < MAX_TIME; t++) {
            int who = gantt[t];
            double x = left + t*cell_w;

            if (who == p)
                cairo_set_source_rgb(cr, 0.2, 0.6, 0.9); // en cours
            else
                cairo_set_source_rgb(cr, 0.95, 0.95, 0.95); // idle

            cairo_rectangle(cr, x, y, cell_w-1, row_h-4);
            cairo_fill(cr);
        }
    }

    return FALSE;
}

/* Isometric 3D Bar Chart for Stats */
static gboolean on_draw_stats(GtkWidget *widget, cairo_t *cr, gpointer data) {
    (void)data;
    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);
    int w = alloc.width;
    int h = alloc.height;

    /* Calculate Stats */
    double avg_tat = 0, avg_wt = 0;
    int n = 0;
    for (int i=0; i<proc_count; i++) {
        if (procs[i].state == FINISHED) {
            int tat = procs[i].end_time - procs[i].arrival;
            int wt = tat - procs[i].burst;
            avg_tat += tat;
            avg_wt += wt;
            n++;
        }
    }
    if (n > 0) { avg_tat /= n; avg_wt /= n; }

    /* Background */
    cairo_set_source_rgb(cr, 1, 1, 1); // White background
    cairo_paint(cr);

    /* Isometric projection helper */
    // x_iso = (x - y) * cos(30)
    // y_iso = (x + y) * sin(30) - z
    // We will draw simple 3D bars manually
    
    double bar_w = 60;
    double max_val = (avg_tat > avg_wt ? avg_tat : avg_wt);
    if (max_val < 10) max_val = 10;
    double scale = (h - 100) / max_val;

    /* Draw TAT Bar */
    double x1 = w/3 - bar_w/2;
    double y1 = h - 50;
    double h1 = avg_tat * scale;
    
    // Front face
    cairo_set_source_rgb(cr, 0.2, 0.6, 0.9); // Blue
    cairo_rectangle(cr, x1, y1 - h1, bar_w, h1);
    cairo_fill(cr);
    
    // Top face
    cairo_new_path(cr);
    cairo_move_to(cr, x1, y1 - h1);
    cairo_line_to(cr, x1 + 20, y1 - h1 - 20);
    cairo_line_to(cr, x1 + bar_w + 20, y1 - h1 - 20);
    cairo_line_to(cr, x1 + bar_w, y1 - h1);
    cairo_close_path(cr);
    cairo_set_source_rgb(cr, 0.4, 0.8, 1.0); // Lighter Blue
    cairo_fill(cr);
    
    // Side face
    cairo_new_path(cr);
    cairo_move_to(cr, x1 + bar_w, y1 - h1);
    cairo_line_to(cr, x1 + bar_w + 20, y1 - h1 - 20);
    cairo_line_to(cr, x1 + bar_w + 20, y1 - 20);
    cairo_line_to(cr, x1 + bar_w, y1);
    cairo_close_path(cr);
    cairo_set_source_rgb(cr, 0.1, 0.4, 0.7); // Darker Blue
    cairo_fill(cr);

    /* Draw WT Bar */
    double x2 = 2*w/3 - bar_w/2;
    double y2 = h - 50;
    double h2 = avg_wt * scale;

    // Front face
    cairo_set_source_rgb(cr, 0.9, 0.3, 0.3); // Red
    cairo_rectangle(cr, x2, y2 - h2, bar_w, h2);
    cairo_fill(cr);

    // Top face
    cairo_new_path(cr);
    cairo_move_to(cr, x2, y2 - h2);
    cairo_line_to(cr, x2 + 20, y2 - h2 - 20);
    cairo_line_to(cr, x2 + bar_w + 20, y2 - h2 - 20);
    cairo_line_to(cr, x2 + bar_w, y2 - h2);
    cairo_close_path(cr);
    cairo_set_source_rgb(cr, 1.0, 0.5, 0.5); // Lighter Red
    cairo_fill(cr);

    // Side face
    cairo_new_path(cr);
    cairo_move_to(cr, x2 + bar_w, y2 - h2);
    cairo_line_to(cr, x2 + bar_w + 20, y2 - h2 - 20);
    cairo_line_to(cr, x2 + bar_w + 20, y2 - 20);
    cairo_line_to(cr, x2 + bar_w, y2);
    cairo_close_path(cr);
    cairo_set_source_rgb(cr, 0.7, 0.1, 0.1); // Darker Red
    cairo_fill(cr);

    /* Labels */
    cairo_set_source_rgb(cr, 0, 0, 0); // Black text
    cairo_set_font_size(cr, 14);
    
    char buf[64];
    snprintf(buf, sizeof(buf), "Avg TAT: %.2f", avg_tat);
    cairo_move_to(cr, x1, y1 + 20);
    cairo_show_text(cr, buf);

    snprintf(buf, sizeof(buf), "Avg WT: %.2f", avg_wt);
    cairo_move_to(cr, x2, y2 + 20);
    cairo_show_text(cr, buf);

    return FALSE;
}




/* simulation tick executed by g_timeout_add */
static gboolean simulation_tick(gpointer data) {
    (void)data;
    if (!sim_running) return TRUE; /* keep the timeout registered */

    /* discover policy function */
    if (current_policy_index < 0 || current_policy_index >= policy_count) return TRUE;
    if (!policies[current_policy_index].symbol) {
        /* attempt to load */
        if (load_policy(&policies[current_policy_index]) != 0) {
            g_printerr("Failed to load policy\n");
            return TRUE;
        }
    }
    policy_select_t fn = (policy_select_t)policies[current_policy_index].symbol;

    /* call policy to choose process index */
    int idx = fn(procs, proc_count, sim_time, quantum); /* sign: Process*, n, time, quantum */

    /* Ensure any previously running process that is not the selected one is marked READY */
    for (int i = 0; i < proc_count; i++) {
        if (procs[i].state == RUNNING && i != idx) {
            procs[i].state = READY;
        }
    }

    for (int i=0;i<proc_count;i++) {
        /* set ready if arrived and not finished */
        if (procs[i].arrival <= sim_time && procs[i].state != FINISHED) {
            if (procs[i].state == NEW) procs[i].state = READY;
        }
    }

    if (idx >= 0 && idx < proc_count) {
        /* run one unit */
        if (procs[idx].start_time == -1) procs[idx].start_time = sim_time;
        procs[idx].state = RUNNING;
        procs[idx].remaining--;
        /* increment waited for others ready */
        for (int j=0;j<proc_count;j++) if (j!=idx && procs[j].state == READY) procs[j].waited++;
        /* record in gantt */
        if (sim_time < MAX_TIME) gantt[sim_time] = idx;

        if (procs[idx].remaining <= 0) {
            procs[idx].state = FINISHED;
            procs[idx].end_time = sim_time + 1;
            finished++;
        }
    } else {
        /* idle (no process), mark as -1 */
        if (sim_time < MAX_TIME) gantt[sim_time] = -1;
    }

    sim_time++;

    /* update UI parts */
    refresh_ready_view();

    /* running label: find any RUNNING */
    bool anyrun = false;
    for (int i=0;i<proc_count;i++) {
        if (procs[i].state == RUNNING) {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s (remaining: %d)", procs[i].name, procs[i].remaining);
            gtk_label_set_text(GTK_LABEL(label_running), buf);
            anyrun = true;
            break;
        }
    }
    if (!anyrun) gtk_label_set_text(GTK_LABEL(label_running), "Idle");

    /* redraw gantt */
    gtk_widget_queue_draw(drawing_gantt);

    /* stop if finished all */
    if (finished >= proc_count) {
        sim_running = false;
        record_current_run();
        g_print("Simulation finished at time %d\n", sim_time);
        return FALSE; /* stop timeout */
    }

    return TRUE; /* keep timer running */
}

/* UI callbacks */
static void on_browse(GtkButton *b, gpointer data) {
    (void)b; (void)data;
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Select process file", GTK_WINDOW(window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(entry_file), fn);

        /* parse and populate processes */
        proc_count = parse_config(fn, procs, MAX_PROC);
        if (proc_count < 0) {
            g_printerr("Error parsing file\n");
            proc_count = 0;
        } else {
            /* reset state fields and gantt */
            sim_time = 0; finished = 0; stats_recorded = false; for (int i=0;i<MAX_TIME;i++) gantt[i] = -1;
            for (int i=0;i<proc_count;i++) {
                procs[i].remaining = procs[i].burst;
                procs[i].start_time = -1;
                procs[i].end_time = -1;
                procs[i].waited = 0;
                procs[i].state = NEW;
            }
            refresh_ready_view();
            gtk_widget_queue_draw(drawing_gantt);
        }
        g_free(fn);
    }
    gtk_widget_destroy(dialog);
}

static void on_policy_changed(GtkComboBoxText *cb, gpointer data) {
    (void)data;
    const char *name = gtk_combo_box_text_get_active_text(cb);
    if (!name) return;
    /* find matching policy entry by name */
    for (int i=0;i<policy_count;i++) {
        if (strcmp(policies[i].name, name) == 0) {
            current_policy_index = i;
            g_print("Selected policy: %s\n", policies[i].name);
            return;
        }
    }
}

/* Refresh policies callback */
static void on_refresh_clicked(GtkButton *b, gpointer data) {
    (void)b; (void)data;
    
    /* Cleanup existing policies */
    // We need to declare cleanup_policies prototype or include header. 
    // Since we don't have a header for loader, we'll declare it here or assume it's linked.
    // Better to declare it.
    extern void cleanup_policies(PolicyEntry *entries, int count);
    
    cleanup_policies(policies, policy_count);
    policy_count = 0;
    
    /* Clear combo */
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combo_policy));
    
    /* Rediscover */
    discover_and_populate_policies();
    
    g_print("Policies refreshed. Found %d policies.\n", policy_count);
}

/* callback Start — utilise le spin passé en user_data */
static void on_start_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    GtkWidget *spin = GTK_WIDGET(data);
    /* récupérer la valeur choisie et l'écrire dans la variable globale */
    quantum = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
    printf("Quantum choisi = %d\n", quantum);

    if (!sim_running) {
        sim_running = true;
        /* démarre le timer si ce n'est pas déjà fait */
        g_timeout_add(TICK_MS, simulation_tick, NULL);
    }
}


static void on_pause_clicked(GtkButton *b, gpointer data) {
    (void)b; (void)data;
    sim_running = false;
}

static void on_reset_clicked(GtkButton *b, gpointer data) {
    (void)b; (void)data;
    if (sim_running || finished > 0) record_current_run();
    sim_running = false;
    
    /* Reload config to restore initial state (priorities, bursts, etc.) */
    const char *fn = gtk_entry_get_text(GTK_ENTRY(entry_file));
    int loaded = -1;
    if (fn && strlen(fn) > 0) {
        loaded = parse_config(fn, procs, MAX_PROC);
        if (loaded >= 0) {
            proc_count = loaded;
            g_print("Configuration reloaded from %s\n", fn);
        } else {
            g_printerr("Failed to reload config, using fallback reset.\n");
        }
    }

    /* Fallback reset if reload failed or no file */
    if (loaded < 0) {
        for (int i=0;i<proc_count;i++) {
            procs[i].remaining = procs[i].burst;
            procs[i].start_time = -1;
            procs[i].end_time = -1;
            procs[i].waited = 0;
            procs[i].state = NEW;
        }
    }

    /* Common reset */
    sim_time = 0; finished = 0;
    stats_recorded = false;
    for (int i=0;i<MAX_TIME;i++) gantt[i] = -1;
    
    /* Reset policy state if available */
    if (current_policy_index >= 0 && current_policy_index < policy_count) {
        typedef void (*reset_fn_t)();
        if (policies[current_policy_index].reset_symbol) {
            reset_fn_t fn = (reset_fn_t)policies[current_policy_index].reset_symbol;
            fn();
        }
    }

    refresh_ready_view();
    gtk_widget_queue_draw(drawing_gantt);
}

static void on_kill_clicked(GtkButton *b, gpointer data) {
    (void)b; (void)data;
    if (!sim_running) return;
    
    /* Find running process */
    int idx = -1;
    for (int i=0; i<proc_count; i++) {
        if (procs[i].state == RUNNING) {
            idx = i;
            break;
        }
    }
    
    if (idx != -1) {
        procs[idx].state = FINISHED;
        procs[idx].end_time = sim_time;
        finished++;
        g_print("Process %s killed by user at time %d\n", procs[idx].name, sim_time);
        
        /* Force immediate reschedule by clearing gantt for this tick (optional, but cleaner) */
        // simulation_tick will handle picking the next one on next tick
    }
}

/* Discover policies (scan politiques/ for .so) and populate combo */
static void discover_and_populate_policies() {
    /* use the discover_policies implemented in policies_loader.c */
    policy_count = discover_policies("politiques", policies, MAX_POL);
    if (policy_count <= 0) {
        g_print("No policies found in politiques/\n");
        return;
    }
    
    int default_idx = 0;
    for (int i=0;i<policy_count;i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_policy), policies[i].name);
        if (strcmp(policies[i].name, "fifo") == 0) {
            default_idx = i;
        }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_policy), default_idx);
    current_policy_index = default_idx;
}

/* Build GUI and connect everything */
void build_gui() {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Ordonnanceur Multi-tâche");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    /* Main Horizontal Box (Split Left/Right) */
    GtkWidget *hbox_main = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_add(GTK_CONTAINER(window), hbox_main);

    /* Left Side: Controls + Visualization */
    GtkWidget *vbox_left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(hbox_main), vbox_left, TRUE, TRUE, 4);

    /* Control Panel (Compact) */
    GtkWidget *frame_ctrl = gtk_frame_new("⚙️ Zone de contrôle");
    gtk_box_pack_start(GTK_BOX(vbox_left), frame_ctrl, FALSE, FALSE, 4);
    GtkWidget *grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(frame_ctrl), grid);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);

    GtkWidget *lbl_file = gtk_label_new("Fichier:");
    entry_file = gtk_entry_new();
    GtkWidget *btn_browse = gtk_button_new_with_label("Parcourir");
    g_signal_connect(btn_browse, "clicked", G_CALLBACK(on_browse), NULL);

    combo_policy = gtk_combo_box_text_new();
    g_signal_connect(combo_policy, "changed", G_CALLBACK(on_policy_changed), NULL);

    GtkWidget *lbl_q = gtk_label_new("Quantum:");
    GtkWidget *spin = gtk_spin_button_new_with_range(1, 1000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 2);
    spin_quantum = spin;

    btn_start = gtk_button_new_with_label("▶️ Démarrer");
    btn_pause = gtk_button_new_with_label("⏸️ Pause");
    btn_reset = gtk_button_new_with_label("🔄 Réinitialiser");
    GtkWidget *btn_kill = gtk_button_new_with_label("☠️ Tuer");

    g_signal_connect(btn_start, "clicked", G_CALLBACK(on_start_clicked), spin);
    g_signal_connect(btn_pause, "clicked", G_CALLBACK(on_pause_clicked), NULL);
    g_signal_connect(btn_reset, "clicked", G_CALLBACK(on_reset_clicked), NULL);
    g_signal_connect(btn_kill, "clicked", G_CALLBACK(on_kill_clicked), NULL);

    /* Row 0: File Selection */
    gtk_grid_attach(GTK_GRID(grid), lbl_file, 0,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), entry_file, 1,0,3,1); // Span 3 cols
    gtk_grid_attach(GTK_GRID(grid), btn_browse, 4,0,1,1);

    /* Row 0 (continued/right side): Settings */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Politique:"), 5,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), combo_policy, 6,0,1,1);
    
    // Refresh button
    GtkWidget *btn_refresh_u = gtk_button_new_with_label("🔄");
    g_signal_connect(btn_refresh_u, "clicked", G_CALLBACK(on_refresh_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), btn_refresh_u, 7,0,1,1);

    gtk_grid_attach(GTK_GRID(grid), lbl_q, 8,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), spin, 9,0,1,1);

    /* Row 1: Action Buttons */
    GtkWidget *bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_SPREAD);
    gtk_container_add(GTK_CONTAINER(bbox), btn_start);
    gtk_container_add(GTK_CONTAINER(bbox), btn_pause);
    gtk_container_add(GTK_CONTAINER(bbox), btn_reset);
    gtk_container_add(GTK_CONTAINER(bbox), btn_kill);
    
    gtk_grid_attach(GTK_GRID(grid), bbox, 0,1,9,1);

    /* Gantt Area (Scrollable) */
    GtkWidget *frame_g = gtk_frame_new("Diagramme de Gantt");
    gtk_box_pack_start(GTK_BOX(vbox_left), frame_g, TRUE, TRUE, 4);
    
    GtkWidget *scrolled_gantt = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scrolled_gantt, -1, 220); 
    gtk_container_add(GTK_CONTAINER(frame_g), scrolled_gantt);

    drawing_gantt = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_gantt, 2000, 200); 
    gtk_container_add(GTK_CONTAINER(scrolled_gantt), drawing_gantt);
    g_signal_connect(drawing_gantt, "draw", G_CALLBACK(on_draw_gantt), NULL);

    /* Bottom Area: Stats (Left) + History (Right) */
    GtkWidget *hbox_bottom = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(vbox_left), hbox_bottom, FALSE, FALSE, 4);

    /* Statistics Area */
    GtkWidget *frame_stats = gtk_frame_new("Statistiques (3D)");
    gtk_box_pack_start(GTK_BOX(hbox_bottom), frame_stats, TRUE, TRUE, 4); 
    GtkWidget *drawing_stats = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_stats, -1, 150);
    gtk_container_add(GTK_CONTAINER(frame_stats), drawing_stats);
    g_signal_connect(drawing_stats, "draw", G_CALLBACK(on_draw_stats), NULL);
    g_signal_connect_swapped(drawing_gantt, "draw", G_CALLBACK(gtk_widget_queue_draw), drawing_stats);

    /* History Area */
    GtkWidget *frame_hist = gtk_frame_new("Historique des Sessions");
    gtk_box_pack_start(GTK_BOX(hbox_bottom), frame_hist, TRUE, TRUE, 4); // Share width equally

    store_history = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_INT, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
    GtkWidget *tree_hist = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store_history));
    
    GtkCellRenderer *r_hist = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_hist), -1, "Pol", r_hist, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_hist), -1, "Q", r_hist, "text", 1, NULL);
    
    GtkCellRenderer *r_tat = gtk_cell_renderer_text_new();
    g_object_set(r_tat, "xalign", 1.0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_hist), -1, "Avg TAT", r_tat, "text", 2, NULL);

    GtkCellRenderer *r_wt = gtk_cell_renderer_text_new();
    g_object_set(r_wt, "xalign", 1.0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_hist), -1, "Avg WT", r_wt, "text", 3, NULL);

    /* Format double columns to 2 decimal places */
    // GTK TreeView simple text renderer doesn't support printf formatting easily without a data func
    // For simplicity, we'll just display the double. For "beautiful", we might want a cell data func, 
    // but let's stick to simple first.

    GtkWidget *sw_hist = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(sw_hist), tree_hist);
    gtk_container_add(GTK_CONTAINER(frame_hist), sw_hist);

    /* Right Side: System State (Full Height) */
    GtkWidget *vbox_state = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(hbox_main), vbox_state, FALSE, FALSE, 4);
    gtk_widget_set_size_request(vbox_state, 300, -1); 

    GtkWidget *frame_state = gtk_frame_new("État du Système");
    gtk_box_pack_start(GTK_BOX(vbox_state), frame_state, TRUE, TRUE, 4); // Expands to fill height
    
    GtkWidget *vbox_state_inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(frame_state), vbox_state_inner);

    /* Running Panel */
    GtkWidget *frame_run = gtk_frame_new("En Exécution");
    gtk_box_pack_start(GTK_BOX(vbox_state_inner), frame_run, FALSE, FALSE, 4);
    label_running = gtk_label_new("Idle");
    gtk_container_add(GTK_CONTAINER(frame_run), label_running);

    /* Ready Queue */
    GtkWidget *frame_ready = gtk_frame_new("File d'attente");
    gtk_box_pack_start(GTK_BOX(vbox_state_inner), frame_ready, TRUE, TRUE, 4);

    store_ready = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);
    tree_ready = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store_ready));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_ready), -1, "Nom", renderer, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_ready), -1, "Arr", renderer, "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_ready), -1, "Dur", renderer, "text", 2, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_ready), -1, "Prio", renderer, "text", 3, NULL);

    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(sw), tree_ready);
    gtk_container_add(GTK_CONTAINER(frame_ready), sw);

    /* Finished Queue */
    GtkWidget *frame_finished = gtk_frame_new("Terminés / Tués");
    gtk_box_pack_start(GTK_BOX(vbox_state_inner), frame_finished, TRUE, TRUE, 4);

    store_finished = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING);
    tree_finished = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store_finished));
    GtkCellRenderer *renderer2 = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_finished), -1, "Nom", renderer2, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_finished), -1, "Arr", renderer2, "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_finished), -1, "Fin", renderer2, "text", 2, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_finished), -1, "Etat", renderer2, "text", 3, NULL);

    GtkWidget *sw2 = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(sw2), tree_finished);
    gtk_container_add(GTK_CONTAINER(frame_finished), sw2);

    /* discover policies and add to combo */
    discover_and_populate_policies();

    /* Load CSS */
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "style.css", NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    /* Load default config if exists */
    if (access("sample_config.txt", F_OK) != -1) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            char fullpath[2048];
            snprintf(fullpath, sizeof(fullpath), "%s/sample_config.txt", cwd);
            gtk_entry_set_text(GTK_ENTRY(entry_file), fullpath);
            
            /* Trigger load */
            proc_count = parse_config(fullpath, procs, MAX_PROC);
            if (proc_count >= 0) {
                sim_time = 0; finished = 0; stats_recorded = false; 
                for (int i=0;i<MAX_TIME;i++) gantt[i] = -1;
                for (int i=0;i<proc_count;i++) {
                    procs[i].remaining = procs[i].burst;
                    procs[i].start_time = -1;
                    procs[i].end_time = -1;
                    procs[i].waited = 0;
                    procs[i].state = NEW;
                }
                refresh_ready_view();
                gtk_widget_queue_draw(drawing_gantt);
                g_print("Default config loaded: %s\n", fullpath);
            }
        }
    }

    gtk_widget_show_all(window);
}

/* entrypoint for app */
int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    build_gui();
    gtk_main();

    /* unload policies */
    for (int i=0;i<policy_count;i++) unload_policy(&policies[i]);
    return 0;
}
 

