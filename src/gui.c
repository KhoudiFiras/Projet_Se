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
static GtkWidget *btn_abort;
static GtkWidget *btn_reset;
static GtkWidget *tree_ready;
static GtkWidget *label_running;
static GtkWidget *drawing_gantt;
static GtkListStore *store_ready;

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
}



static gboolean on_draw_gantt(GtkWidget *widget, cairo_t *cr, gpointer data) {
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




/* simulation tick executed by g_timeout_add */
static gboolean simulation_tick(gpointer data) {
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
        g_print("Simulation finished at time %d\n", sim_time);
        return FALSE; /* stop timeout */
    }

    return TRUE; /* keep timer running */
}

/* UI callbacks */
static void on_browse(GtkButton *b, gpointer data) {
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
            sim_time = 0; finished = 0; for (int i=0;i<MAX_TIME;i++) gantt[i] = -1;
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

/* callback Start — utilise le spin passé en user_data */
static void on_start_clicked(GtkWidget *widget, gpointer data) {
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
    sim_running = false;
}

static void on_abort_clicked(GtkButton *b, gpointer data) {
    sim_running = false;
    /* reset everything */
    sim_time = 0; finished = 0;
    for (int i=0;i<proc_count;i++) {
        procs[i].remaining = procs[i].burst;
        procs[i].start_time = -1;
        procs[i].end_time = -1;
        procs[i].waited = 0;
        procs[i].state = NEW;
    }
    for (int i=0;i<MAX_TIME;i++) gantt[i] = -1;
    refresh_ready_view();
    gtk_widget_queue_draw(drawing_gantt);
}

static void on_reset_clicked(GtkButton *b, gpointer data) {
    on_abort_clicked(b, data);
}

/* Discover policies (scan politiques/ for .so) and populate combo */
static void discover_and_populate_policies() {
    /* use the discover_policies implemented in policies_loader.c */
    policy_count = discover_policies("politiques", policies, MAX_POL);
    if (policy_count <= 0) {
        g_print("No policies found in politiques/\n");
        return;
    }
    for (int i=0;i<policy_count;i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_policy), policies[i].name);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_policy), 0);
    current_policy_index = 0;
}

/* Build GUI and connect everything */
void build_gui() {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Ordonnanceur Multi-tâche");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    /* control panel */
    GtkWidget *frame_ctrl = gtk_frame_new("Zone de contrôle");
    gtk_box_pack_start(GTK_BOX(vbox), frame_ctrl, FALSE, FALSE, 4);
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

    btn_start = gtk_button_new_with_label("Démarrer");
    btn_pause = gtk_button_new_with_label("Pause");
    btn_abort = gtk_button_new_with_label("Abort");
    btn_reset = gtk_button_new_with_label("Réinitialiser");

    g_signal_connect(btn_start, "clicked", G_CALLBACK(on_start_clicked), spin);
    g_signal_connect(btn_pause, "clicked", G_CALLBACK(on_pause_clicked), NULL);
    g_signal_connect(btn_abort, "clicked", G_CALLBACK(on_abort_clicked), NULL);
    g_signal_connect(btn_reset, "clicked", G_CALLBACK(on_reset_clicked), NULL);

    gtk_grid_attach(GTK_GRID(grid), lbl_file, 0,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), entry_file, 1,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), btn_browse, 2,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Politique:"), 0,1,1,1);
    gtk_grid_attach(GTK_GRID(grid), combo_policy, 1,1,1,1);
    gtk_grid_attach(GTK_GRID(grid), lbl_q, 2,1,1,1);
    gtk_grid_attach(GTK_GRID(grid), spin, 3,1,1,1);
    gtk_grid_attach(GTK_GRID(grid), btn_start, 0,2,1,1);
    gtk_grid_attach(GTK_GRID(grid), btn_pause, 1,2,1,1);
    gtk_grid_attach(GTK_GRID(grid), btn_abort, 2,2,1,1);
    gtk_grid_attach(GTK_GRID(grid), btn_reset, 3,2,1,1);

    /* gantt area */
    GtkWidget *frame_g = gtk_frame_new("Diagramme de Gantt");
    gtk_box_pack_start(GTK_BOX(vbox), frame_g, TRUE, TRUE, 4);
    drawing_gantt = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_gantt, 900, 300);
    gtk_container_add(GTK_CONTAINER(frame_g), drawing_gantt);
    g_signal_connect(drawing_gantt, "draw", G_CALLBACK(on_draw_gantt), NULL);

    /* system state: running + ready list */
    GtkWidget *frame_state = gtk_frame_new("État du Système");
    gtk_box_pack_start(GTK_BOX(vbox), frame_state, FALSE, FALSE, 4);
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_add(GTK_CONTAINER(frame_state), h);

    /* Running panel */
    GtkWidget *frame_run = gtk_frame_new("En Exécution");
    gtk_box_pack_start(GTK_BOX(h), frame_run, TRUE, TRUE, 4);
    label_running = gtk_label_new("Idle");
    gtk_container_add(GTK_CONTAINER(frame_run), label_running);

    /* ready queue */
    GtkWidget *frame_ready = gtk_frame_new("File d'attente");
    gtk_box_pack_start(GTK_BOX(h), frame_ready, TRUE, TRUE, 4);

    store_ready = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);
    tree_ready = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store_ready));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_ready), -1, "Nom", renderer, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_ready), -1, "Arr", renderer, "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_ready), -1, "Dur", renderer, "text", 2, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_ready), -1, "Prio", renderer, "text", 3, NULL);

    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(sw, 350, 150);
    gtk_container_add(GTK_CONTAINER(sw), tree_ready);
    gtk_container_add(GTK_CONTAINER(frame_ready), sw);

    /* discover policies and add to combo */
    discover_and_populate_policies();

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
 

