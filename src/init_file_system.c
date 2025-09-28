#include <stdlib.h>
#include "file_system.h"
#include <string.h>
#include <stdio.h>

extern Files directory_table;

RootDirectory* init_file_system() {
    RootDirectory *fs = malloc(sizeof(RootDirectory));
    if (!fs) return NULL;

    fs->root = malloc(sizeof(Directory));
    if (!fs->root) { free(fs); return NULL; }

    memset(fs->root, 0, sizeof(Directory));

    strncpy(fs->root->directory_name, "/", MAX_FILENAME_LEN);
    fs->root->directory_name[MAX_FILENAME_LEN - 1] = '\0';
    fs->root->parent = NULL;
    
    fs->root->disk_cluster = superblock.dir_start;

    fs->root->subdirs = malloc(sizeof(Directory*) * MAX_SUBDIRS);
    if (!fs->root->subdirs) {
        free(fs->root);
        free(fs);
        return NULL;
    }
    memset(fs->root->subdirs, 0, sizeof(Directory*) * MAX_SUBDIRS);
    fs->root->subdir_count = 0;

    fs->root->filesArrayReference = &directory_table;

    fs->cwd = fs->root;

    return fs;
}


