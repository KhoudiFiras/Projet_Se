#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <dlfcn.h>

/* Structure to keep loaded policy handles and names */
typedef int (*policy_fn_t)(void*, int, int, int); // actual signature casted later

typedef struct {
    char path[512];
    char name[128];
    void *handle;
    void *symbol; // pointer to function
} PolicyEntry;

/* Scan directory for .so files and fill entries. Returns number found (<= max). */
int discover_policies(const char *dirpath, PolicyEntry *out, int max) {
    DIR *d = opendir(dirpath);
    if (!d) return 0;
    struct dirent *ent;
    int c = 0;
    while ((ent = readdir(d)) != NULL && c < max) {
        if (ent->d_type == DT_REG || ent->d_type == DT_LNK || ent->d_type == DT_UNKNOWN) {
            size_t L = strlen(ent->d_name);
            if (L > 3 && strcmp(ent->d_name + L - 3, ".so") == 0) {
                snprintf(out[c].path, sizeof(out[c].path), "%s/%s", dirpath, ent->d_name);
                /* name without extension */
                strncpy(out[c].name, ent->d_name, sizeof(out[c].name)-1);
                char *dot = strrchr(out[c].name, '.'); if (dot) *dot = '\0';
                out[c].handle = NULL;
                out[c].symbol = NULL;
                c++;
            }
        }
    }
    closedir(d);
    return c;
}

/* Load policy by index and resolve symbol "policy_select" */
int load_policy(PolicyEntry *entry) {
    if (!entry) return -1;
    if (entry->handle) return 0; // already loaded
    entry->handle = dlopen(entry->path, RTLD_NOW);
    if (!entry->handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return -1;
    }
    dlerror();
    entry->symbol = dlsym(entry->handle, "policy_select");
    char *err = dlerror();
    if (err) {
        fprintf(stderr, "dlsym failed: %s\n", err);
        dlclose(entry->handle);
        entry->handle = NULL;
        return -1;
    }
    return 0;
}

/* Unload all loaded policies */
void unload_policy(PolicyEntry *entry) {
    if (!entry) return;
    if (entry->handle) {
        dlclose(entry->handle);
        entry->handle = NULL;
        entry->symbol = NULL;
    }
}

