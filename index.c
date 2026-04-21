#include "pes.h"
#include "index.h"
#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

// --- LINKER FIX: MUST MATCH pes.c CALL ---
int index_status(const Index *index) {
    printf("Staged changes:\n");
    int staged = 0;
    for (int i = 0; i < index->count; i++) {
        printf("  staged:     %s\n", index->entries[i].path);
        staged++;
    }
    if (staged == 0) printf("  (nothing to show)\n");
    printf("\n");
    // Add logic for Unstaged and Untracked here as per previous version
    return 0;
}

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) return &index->entries[i];
    }
    return NULL;
}

int index_load(Index *index) {
    index->count = 0;
    FILE *f = fopen(".pes/index", "r");
    if (!f) return 0;
    char line[1024];
    while (index->count < MAX_INDEX_ENTRIES && fgets(line, sizeof(line), f)) {
        IndexEntry *e = &index->entries[index->count];
        char hash_hex[HASH_HEX_SIZE + 1];
        if (sscanf(line, "%o %s %ld %u %[^\n]", &e->mode, hash_hex, &e->mtime_sec, &e->size, e->path) == 5) {
            hex_to_hash(hash_hex, &e->hash);
            index->count++;
        }
    }
    fclose(f);
    return 0;
}

int index_save(const Index *index) {
    FILE *f = fopen(".pes/index.tmp", "w");
    if (!f) return -1;
    for (int i = 0; i < index->count; i++) {
        char hash_hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&index->entries[i].hash, hash_hex);
        fprintf(f, "%o %s %ld %u %s\n", index->entries[i].mode, hash_hex, (long)index->entries[i].mtime_sec, index->entries[i].size, index->entries[i].path);
    }
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    return rename(".pes/index.tmp", ".pes/index");
}

int index_add(Index *index, const char *path) {
    struct stat st;
    if (stat(path, &st) < 0) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    void *buf = malloc(st.st_size);
    if (st.st_size > 0) {
        size_t read = fread(buf, 1, st.st_size, f);
        (void)read; // Silence warning
    }
    fclose(f);
    ObjectID id;
    object_write(OBJ_BLOB, buf, st.st_size, &id);
    free(buf);
    IndexEntry *e = index_find(index, path);
    if (!e) e = &index->entries[index->count++];
    e->mode = get_file_mode(path);
    e->hash = id;
    e->mtime_sec = st.st_mtime;
    e->size = st.st_size;
    strncpy(e->path, path, sizeof(e->path)-1);
    return index_save(index);
}
// Phase 3 Final Polish
