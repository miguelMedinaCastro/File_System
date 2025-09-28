#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include "file_system.h"
Files directory_table;
Superblock superblock;
int32_t *FAT = NULL;

static int alloc_dir_cluster(void) {
    for (int i = 0; i < superblock.total_clusters; i++) {
        if (FAT[i] == FREE_CLUSTER) {
            FAT[i] = DIR_CLUSTER;   
            return i;
        }
    }
    return -1;
}
static void flush_directory_recursive(Directory *dir);

//static long fat_offset = 0;
//static long dir_offset = 0;
//static long data_offset = 0;

//static int image_loaded =0;

/*
static void load_directory_recursive(Directory *dir) {
    if (!dir) return;

    FILE *img = fopen(FS_IMAGE_PATH, "rb");
    if (!img) return;

    long off = (long)superblock.data_start + (long)dir->disk_cluster * superblock.cluster_size;
    fseek(img, off, SEEK_SET);

    Files *files = malloc(sizeof(Files));
    if (!files) { fclose(img); return; }

    fread(files, sizeof(Files), 1, img);
    dir->filesArrayReference = files;

    fclose(img);


}
*/


static char* my_strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* d = malloc(len);
    if (d) memcpy(d, s, len);
    return d;
}

// int fs_init_image(const)
/*

static void init_in_memory_structs(void) {

    directory_table.file_count = 0;
    for (int i = 0; i < MAX_FILES; ++i) {
        memset(&directory_table.files[i], 0, sizeof(directory_table.files[i]));
        directory_table.files[i].start_cluster = FREE_CLUSTER;
        directory_table.files[i].file_size = 0;

    }

    if (!FAT) {
        FAT = malloc(sizeof(int32_t) * NUM_CLUSTERS);
    }
    if (!FAT) {
        perror("malloc FAT failed");
        exit(1);
    }
    for (int i = 0; i < NUM_CLUSTERS; ++i) FAT[i] = FREE_CLUSTER;
}
*/
int fs_format_image(void) {
    FILE *f = fopen(FS_IMAGE_PATH, "w+b");
    if (!f) { perror("fopen(format)"); return -1; }


    memset(&superblock, 0, sizeof(superblock));
    superblock.total_clusters = NUM_CLUSTERS;
    superblock.cluster_size   = CLUSTER_SIZE;
    superblock.fat_start      = sizeof(Superblock);
    superblock.dir_start      = 0;
    superblock.data_start     = superblock.fat_start + superblock.total_clusters * sizeof(int32_t);
    superblock.max_files      = MAX_FILES;

    if (fseek(f, 0, SEEK_SET) != 0) { perror("fseek SB"); fclose(f); return -1; }
    if (fwrite(&superblock, sizeof(superblock), 1, f) != 1) { perror("fwrite SB"); fclose(f); return -1; }

    
    if (!FAT) FAT = (int32_t*)malloc(sizeof(int32_t) * superblock.total_clusters);
    if (!FAT) { perror("malloc FAT"); fclose(f); return -1; }
    for (int i = 0; i < superblock.total_clusters; i++) FAT[i] = FREE_CLUSTER;
    FAT[superblock.dir_start] = DIR_CLUSTER;

    if (fseek(f, superblock.fat_start, SEEK_SET) != 0) { perror("fseek FAT"); fclose(f); return -1; }
    if (fwrite(FAT, sizeof(int32_t), superblock.total_clusters, f) != (size_t)superblock.total_clusters) {
        perror("fwrite FAT"); fclose(f); return -1;
    }

    
    DirectoryDisk root_dsk;
    memset(&root_dsk, 0, sizeof(root_dsk));
    strncpy(root_dsk.directory_name, "/", MAX_FILENAME_LEN - 1);
    root_dsk.directory_name[MAX_FILENAME_LEN - 1] = '\0';
    root_dsk.parent_cluster = -1;
    root_dsk.files.file_count = 0; 

    long off = (long)superblock.data_start + (long)superblock.dir_start * (long)superblock.cluster_size;
    if (fseek(f, off, SEEK_SET) != 0) { perror("fseek root"); fclose(f); return -1; }
    if (fwrite(&root_dsk, sizeof(DirectoryDisk), 1, f) != 1) { perror("fwrite root"); fclose(f); return -1; }

    fflush(f);
    fclose(f);
    return 0;
}


Directory* find_directory_by_cluster(RootDirectory *fs, int32_t cluster) {
    if (!fs || !fs->root) return NULL;

    Directory *queue[1024];
    int front = 0, back = 0;
    queue[back++] = fs->root;

    while (front < back) {
        Directory *cur = queue[front++];
        if (cur->disk_cluster == cluster) return cur;

        for (int i = 0; i < cur->subdir_count; i++) {
            if (back < 1024){
            queue[back++] = cur->subdirs[i];
            }
        }
    }
    return NULL;
}


int fs_load_image(RootDirectory *fs) {
    FILE *f = fopen(FS_IMAGE_PATH, "rb");
    if (!f) {
        printf("Nenhuma imagem encontrada, formatando novo FS...\n");
        if (fs_format_image() != 0) return -1;
        f = fopen(FS_IMAGE_PATH, "rb");
        if (!f) return -1;
    }

    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -1; }
    if (fread(&superblock, sizeof(superblock), 1, f) != 1) { fclose(f); return -1; }

    if (!FAT) {
        FAT = (int32_t*) malloc(sizeof(int32_t) * superblock.total_clusters);
        if (!FAT) { fclose(f); return -1; }
    }
    if (fseek(f, superblock.fat_start, SEEK_SET) != 0) { fclose(f); return -1; }
    if (fread(FAT, sizeof(int32_t), superblock.total_clusters, f) != (size_t)superblock.total_clusters) {
        fclose(f); return -1;
    }

    long root_offset = (long)superblock.data_start + (long)superblock.dir_start * (long)superblock.cluster_size;
    if (fseek(f, root_offset, SEEK_SET) != 0) { fclose(f); return -1; }

    Files root_files_disk;
    memset(&root_files_disk, 0, sizeof(root_files_disk));
    if (fread(&root_files_disk, sizeof(Files), 1, f) != 1) {
        fclose(f);
        return -1;
    }

    if (!fs || !fs->root) { fclose(f); return -1; }
    strncpy(fs->root->directory_name, "/", MAX_FILENAME_LEN - 1);
    fs->root->directory_name[MAX_FILENAME_LEN - 1] = '\0';
    fs->root->disk_cluster = superblock.dir_start;

    memcpy(&directory_table, &root_files_disk, sizeof(Files));
    fs->root->filesArrayReference = &directory_table;

    if (!fs->root->subdirs) {
        fs->root->subdirs = (Directory**)malloc(sizeof(Directory*) * MAX_SUBDIRS);
        if (!fs->root->subdirs) { fclose(f); return -1; }
        memset(fs->root->subdirs, 0, sizeof(Directory*) * MAX_SUBDIRS);
    }
    fs->root->subdir_count = 0;
    fs->cwd = fs->root;

    for (int i = 0; i < superblock.total_clusters; i++) {
        if (FAT[i] == DIR_CLUSTER && i != superblock.dir_start) {
            DirectoryDisk dsk;
            long off = (long)superblock.data_start + (long)i * (long)superblock.cluster_size;
            if (fseek(f, off, SEEK_SET) != 0) {
                continue;
            }
            
            if (fread(&dsk, sizeof(DirectoryDisk), 1, f) != 1) {
                continue;
            }

            Directory *existing_dir = find_directory_by_cluster(fs, i);
            if (existing_dir) {
                if (existing_dir->filesArrayReference) {
                    memcpy(existing_dir->filesArrayReference, &dsk.files, sizeof(Files));
                }
            } else {
                Directory *d = (Directory*)malloc(sizeof(Directory));
                if (!d) {
                    continue;
                }
                memset(d, 0, sizeof(Directory));

                strncpy(d->directory_name, dsk.directory_name, MAX_FILENAME_LEN - 1);
                d->directory_name[MAX_FILENAME_LEN - 1] = '\0';
                d->disk_cluster = i;

                d->filesArrayReference = (Files*)malloc(sizeof(Files));
                if (!d->filesArrayReference) {
                    free(d);
                    continue;
                }
                memcpy(d->filesArrayReference, &dsk.files, sizeof(Files));

                d->subdirs = (Directory**)malloc(sizeof(Directory*) * MAX_SUBDIRS);
                if (!d->subdirs) {
                    free(d->filesArrayReference);
                    free(d);
                    continue;
                }
                memset(d->subdirs, 0, sizeof(Directory*) * MAX_SUBDIRS);
                d->subdir_count = 0;

                Directory *parent = NULL;
                if (dsk.parent_cluster == superblock.dir_start) {
                    parent = fs->root;
                } else {
                    parent = find_directory_by_cluster(fs, dsk.parent_cluster);
                }
                
                if (!parent) {
                    parent = fs->root;
                }
                
                d->parent = parent;

                if (parent->subdir_count < MAX_SUBDIRS) {
                    parent->subdirs[parent->subdir_count++] = d;
                } else {
                    free(d->subdirs);
                    free(d->filesArrayReference);
                    free(d);
                }
            }
        }
    }

    fclose(f);
    return 0;
}



void save_directory(FILE *img, Directory *dir) {
    if (!img || !dir) return;

    if (dir->filesArrayReference) {
        for (int i = 0; i < dir->filesArrayReference->file_count; i++) {
            FileEntry *file = &dir->filesArrayReference->files[i];
            if (file->start_cluster == FREE_CLUSTER || file->file_size == 0)
                continue;

            int32_t cur = file->start_cluster;
            int32_t remaining = file->file_size;
            char buf[superblock.cluster_size];
//alt no long pra não duplicar - x2
            while (cur != EOF_CLUSTER && remaining > 0) {
                long where = (long)superblock.data_start + (long)cur * (long)superblock.cluster_size;

                if (fseek(img, where, SEEK_SET) != 0) {
                    perror("fseek save_directory");
                    return;
                }

                int to_write = remaining > superblock.cluster_size ? superblock.cluster_size : remaining;
                memset(buf, 0, sizeof(buf));

                if (fwrite(buf, 1, to_write, img) != (size_t)to_write) {
                    perror("fwrite save_directory");
                    return;
                }

                remaining -= to_write;
                cur = FAT[cur];
            }
        }
    }

    for (int i = 0; i < dir->subdir_count; i++) {
        save_directory(img, dir->subdirs[i]);
    }
}
int fs_sync_image(void) {
    FILE *img = fopen(FS_IMAGE_PATH, "r+b");
    if (!img) {
        img = fopen(FS_IMAGE_PATH, "w+b");
        if (!img) return -1;
    }

    fseek(img, 0, SEEK_SET);
    fwrite(&superblock, sizeof(Superblock), 1, img);

    fseek(img, superblock.fat_start, SEEK_SET);
    fwrite(FAT, sizeof(int32_t), superblock.total_clusters, img);

    long root_off = (long)superblock.data_start + (long)superblock.dir_start * (long)superblock.cluster_size;
    fseek(img, root_off, SEEK_SET);
    fwrite(&directory_table, sizeof(Files), 1, img);


    fflush(img);
    fclose(img);
    return 0;
}

static void free_chain(int32_t start) {
    if (start < 0) return;
    int32_t cur = start;
    while (cur != EOF_CLUSTER && cur != FREE_CLUSTER) {
        int32_t next = FAT[cur];
        FAT[cur] = FREE_CLUSTER;
        if (next == EOF_CLUSTER) break;
        cur = next;
    }
}

int32_t allocate_cluster_for_bytes(int32_t bytes) {
    if (bytes < 0) return -1;

    int32_t need;
    if (bytes == 0) {
        return FREE_CLUSTER;
    } else {
        need = (bytes + superblock.cluster_size - 1) / superblock.cluster_size;
        if (need <= 0) need = 1;
    }

    int32_t first = -1;
    int32_t prev = -1;
    int found = 0;

    for (int32_t i = 0; i < superblock.total_clusters && found < need; ++i) {
        if (FAT[i] == FREE_CLUSTER) {
            if (first == -1) first = i;
            if (prev != -1) FAT[prev] = i;
            prev = i;
            FAT[i] = FREE_CLUSTER;
            found++;
        }
    }

    if (found < need) {
        int32_t cur = first;
        while (cur != -1) {
            int32_t next = FAT[cur];
            FAT[cur] = FREE_CLUSTER;
            if (next == FREE_CLUSTER || next == EOF_CLUSTER) break;
            cur = next;
        }
        return -1;
    }

    if (prev != -1) FAT[prev] = EOF_CLUSTER;
    return first;
}

int fs_copy_in(RootDirectory* fs, const char *host_path, const char *fs_name) {
    if (!host_path || !fs_name) return -1;

    FILE *hf = fopen(host_path, "rb");
    if (!hf) { perror("fopen host_path"); return -1; }

    fseek(hf, 0, SEEK_END);
    int32_t fsize = (int32_t)ftell(hf);
    fseek(hf, 0, SEEK_SET);

    int32_t start = allocate_cluster_for_bytes(fsize);
    if (start < 0) { fclose(hf); return -3; }

    Directory *cwd = fs->cwd;
    if (!cwd || !cwd->filesArrayReference) {
        fclose(hf);
        free_chain(start);
        return -4;
    }

    Files *files = cwd->filesArrayReference;
    if (files->file_count >= MAX_FILES) {
        fclose(hf);
        free_chain(start);
        return -5;
    }

    int idx = files->file_count++;
    strncpy(files->files[idx].file_name, fs_name, MAX_FILENAME_LEN-1);
    files->files[idx].start_cluster = start;
    files->files[idx].file_size = fsize;

    if (cwd->disk_cluster == superblock.dir_start) {
        memcpy(&directory_table, files, sizeof(Files));
        
    }

    FILE *img = fopen(FS_IMAGE_PATH, "rb+");
    if (!img) { fclose(hf); free_chain(start); return -6; }

    char *buf = malloc(superblock.cluster_size);
    int32_t cur = start, remaining = fsize;
     while (cur != EOF_CLUSTER && remaining > 0) {
        int to_read = remaining > superblock.cluster_size ? superblock.cluster_size : remaining;
        memset(buf, 0, superblock.cluster_size);
        
        size_t bytes_read = fread(buf, 1, to_read, hf);
        if (bytes_read != (size_t)to_read && !feof(hf)) {
            perror("fread host file");
            free(buf);
            fclose(img);
            fclose(hf);
            free_chain(start);
            return -7;
        }

        fseek(img, superblock.data_start + cur * superblock.cluster_size, SEEK_SET);
        fwrite(buf, 1, to_read, img);

        remaining -= to_read;
        cur = FAT[cur];
    }

    free(buf);
    fclose(img);
    fclose(hf);

    if (dir_flush(cwd) != 0) {
        printf("Erro ao flush do diretório\n");
        return -1;
    }
    
    if (fs_sync_image() != 0) {
        printf("Erro ao sincronizar imagem\n");
        return -1;
    }
    
    return 0;
}

int fs_copy_out(Directory *dir, const char *fs_name, const char *host_path_in) {
    if (!dir || !fs_name || !host_path_in) return -1;

    int found = -1;
    if (!dir->filesArrayReference) return -2;

    for (int i = 0; i < dir->filesArrayReference->file_count; ++i) {
        if (strcmp(dir->filesArrayReference->files[i].file_name, fs_name) == 0) {
            found = i;
            break;
        }
    }
    if (found == -1) {
        fprintf(stderr, "fs_copy_out: arquivo '%s' não encontrado no diretório '%s'\n",
                fs_name, dir->directory_name);
        return -3;
    }

    FileEntry *file = &dir->filesArrayReference->files[found];

    struct stat st;
    char *host_path = NULL;
    if (stat(host_path_in, &st) == 0 && S_ISDIR(st.st_mode)) {
        size_t need = strlen(host_path_in) + 1 + strlen(fs_name) + 1;
        host_path = malloc(need);
        if (!host_path) { perror("malloc host_path"); return -4; }
        snprintf(host_path, need, "%s/%s", host_path_in, fs_name);
    } else {
        host_path = my_strdup(host_path_in);
        if (!host_path) { perror("strdup host_path"); return -5; }
    }

    FILE *hf = fopen(host_path, "wb");
    if (!hf) { perror("fopen host"); free(host_path); return -6; }

    FILE *img = fopen(FS_IMAGE_PATH, "rb");
    if (!img) { perror("fopen FS_IMAGE_PATH"); fclose(hf); free(host_path); return -7; }

    int32_t cur = file->start_cluster;
    int32_t remaining = file->file_size;
    int cluster_size = superblock.cluster_size ? superblock.cluster_size : CLUSTER_SIZE;
    char *buf = malloc(cluster_size);
    if (!buf) { perror("malloc buf"); fclose(img); fclose(hf); free(host_path); return -8; }

    if (cur == FREE_CLUSTER || remaining == 0) {
        printf("DEBUG: Arquivo vazio\n");
        free(buf); fclose(img); fclose(hf); free(host_path);
        return 0;
    }

    int cluster_num = 0;
    while (cur != EOF_CLUSTER && cur != FREE_CLUSTER && remaining > 0) {
        long offset = (long)superblock.data_start + ((long)cur * (long)cluster_size);

        if (fseek(img, offset, SEEK_SET) != 0) { perror("fseek img"); break; }

        size_t bytes_read = fread(buf, 1, cluster_size, img);
        if (bytes_read == 0 && ferror(img)) { perror("fread img"); break; }

        int32_t to_write = (remaining > cluster_size) ? cluster_size : remaining;
        if (fwrite(buf, 1, (size_t)to_write, hf) != (size_t)to_write) { perror("fwrite host"); break; }

        remaining -= to_write;
        cur = FAT[cur];
        cluster_num++;
    }

    free(buf);
    fclose(img);
    fclose(hf);
    free(host_path);

    return 0;
}

int fs_remove_file(Directory *dir, const char *fs_name) {
    Files *files = dir->filesArrayReference;
    int found = -1;
    for (int i = 0; i < files->file_count; ++i) {
        if (strcmp(files->files[i].file_name, fs_name) == 0) {
            found = i;
            break;
        }
    }
    if (found == -1) return -1;

    int32_t start = files->files[found].start_cluster;
    if (start != FREE_CLUSTER) free_chain(start);

    for (int i = found; i < files->file_count - 1; ++i) {
        files->files[i] = files->files[i+1];
    }

    FileEntry *last = &files->files[files->file_count - 1];
    last->file_name[0] = '\0';
    last->file_size = 0;
    last->start_cluster = FREE_CLUSTER;

    files->file_count--;

    if (dir->disk_cluster == superblock.dir_start) {
        memcpy(&directory_table, files, sizeof(Files));
    }

    if (dir_flush(dir) != 0) return -1;
    return fs_sync_image();
}

int fs_rename(Directory *dir, const char *old_name, const char *new_name) {
    if (!dir || !old_name || !new_name) return -1;

    Files *files = dir->filesArrayReference;
    int idx = -1;

    for (int i = 0; i < files->file_count; i++) {
        if (strcmp(files->files[i].file_name, old_name) == 0) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        fprintf(stderr, "fs_rename: arquivo '%s' não encontrado\n", old_name);
        return -2;
    }

    for (int i = 0; i < files->file_count; i++) {
        if (strcmp(files->files[i].file_name, new_name) == 0) {
            fprintf(stderr, "fs_rename: já existe arquivo chamado '%s'\n", new_name);
            return -3;
        }
    }

    memset(files->files[idx].file_name, 0, sizeof(files->files[idx].file_name));
    strncpy(files->files[idx].file_name, new_name, sizeof(files->files[idx].file_name) - 1);

    if (dir->disk_cluster == superblock.dir_start) {
        memcpy(&directory_table, files, sizeof(Files));
        printf("DEBUG: directory_table atualizada após renomear '%s' para '%s'\n", old_name, new_name);
    }

    if (dir_flush(dir) !=0) return -1;
    return fs_sync_image();
}

FS_Space fs_list_free_spaces() {
    FS_Space info;
    long total_clusters = superblock.total_clusters;
    long cluster_size   = superblock.cluster_size;

    long free_clusters = 0;
    for (int i = 0; i < total_clusters; i++) {
        if (FAT[i] == FREE_CLUSTER) {
            free_clusters++;
        }
    }

    info.total = total_clusters * cluster_size;
    info.free_space = free_clusters * cluster_size;
    info.used = info.total - info.free_space;

    return info;
}

int fs_mkdir(Directory *parent, const char *name) {
    if (!parent || !name || !name[0]) return -1;

    int cluster = alloc_dir_cluster();
    if (cluster < 0) return -1;

    FAT[cluster] = DIR_CLUSTER;
    
    if (fs_save_fat() != 0) {
        FAT[cluster] = FREE_CLUSTER;
        return -1;
    }

    DirectoryDisk dsk;
    memset(&dsk, 0, sizeof(DirectoryDisk));
    strncpy(dsk.directory_name, name, MAX_FILENAME_LEN - 1);
    dsk.directory_name[MAX_FILENAME_LEN - 1] = '\0';
    dsk.files.file_count = 0;
    dsk.parent_cluster = parent->disk_cluster;

    FILE *f = fopen(FS_IMAGE_PATH, "r+b");
    if (!f) {
        f = fopen(FS_IMAGE_PATH, "w+b");
        if (!f) {
            FAT[cluster] = FREE_CLUSTER; 
            fs_save_fat(); 
            return -1;
        }
    }

    long off = (long)superblock.data_start + (long)cluster * (long)superblock.cluster_size;
    fseek(f, off, SEEK_SET);
    if (fwrite(&dsk, sizeof(DirectoryDisk), 1, f) != 1) {
        fclose(f);
        FAT[cluster] = FREE_CLUSTER;
        fs_save_fat();
        return -1;
    }
    fclose(f);


    Directory *dir = malloc(sizeof(Directory));
    if (!dir) {
        FAT[cluster] = FREE_CLUSTER;
        fs_save_fat();
        return -1;
    }
    memset(dir, 0, sizeof(Directory));
    strncpy(dir->directory_name, name, MAX_FILENAME_LEN - 1);
    dir->directory_name[MAX_FILENAME_LEN - 1] = '\0';

    dir->disk_cluster = cluster;
    dir->parent = parent;
    dir->filesArrayReference = malloc(sizeof(Files));
    if (!dir->filesArrayReference) {
        free(dir);
        FAT[cluster] = FREE_CLUSTER;
        fs_save_fat();
        return -1;
    }
    memcpy(dir->filesArrayReference, &dsk.files, sizeof(Files));

    dir->subdirs = malloc(sizeof(Directory*) * MAX_SUBDIRS);
    if (!dir->subdirs) {
        free(dir->filesArrayReference);
        free(dir);
        FAT[cluster] = FREE_CLUSTER;
        fs_save_fat();
        return -1;
    }
    memset(dir->subdirs, 0, sizeof(Directory*) * MAX_SUBDIRS);
    dir->subdir_count = 0;

    if (parent->subdir_count >= MAX_SUBDIRS) {
        free(dir->subdirs);
        free(dir->filesArrayReference);
        free(dir);
        FAT[cluster] = FREE_CLUSTER;
        fs_save_fat();
        return -1;
    }
    
    parent->subdirs[parent->subdir_count++] = dir;
    
    if (fs_flush_directory(parent) != 0) {
        printf("Erro ao persistir diretório pai após criar '%s'\n", name);
        
    }
    
    return 0;
}




int fs_cd(RootDirectory *fs, const char *name) {
    if (!fs || !name) return -1;

    if (strcmp(name, "..") == 0 && fs->cwd->parent) {
        fs->cwd = fs->cwd->parent;
        if (dir_load(fs->cwd) != 0) {
            printf("Falha ao carregar diretório pai do disco.\n");
        }
        return 0;
    }

    for (int i = 0; i < fs->cwd->subdir_count; i++) {
        if (strcmp(fs->cwd->subdirs[i]->directory_name, name) == 0) {
            fs->cwd = fs->cwd->subdirs[i];
            if (dir_load(fs->cwd) != 0) {
                printf("Falha ao carregar diretório '%s' do disco.\n", name);
            }
            return 0;
        }
    }

    return -1; 
}

void fs_rmdir_recursive(Directory *dir) {
    for (int i=0; i < dir->filesArrayReference->file_count; i++)
        free_chain(dir->filesArrayReference->files[i].start_cluster);
    free(dir->filesArrayReference);

    for (int i=0; i<dir->subdir_count; i++)
        fs_rmdir_recursive(dir->subdirs[i]);
    free(dir->subdirs);
    free(dir);

}

int fs_create_empty_file(RootDirectory *fs, const char *filename) {
    Directory *cwd = fs->cwd;
    if (!cwd || !cwd->filesArrayReference) return -1;

    Files *files = cwd->filesArrayReference;

    if (files->file_count >= MAX_FILES) {
        printf("Diretório cheio!\n");
        return -1;
    }

    for (int i = 0; i < files->file_count; i++) {
        if (strcmp(files->files[i].file_name, filename) == 0) {
            printf("Arquivo '%s' já existe\n", filename);
            return -1;
        }
    }

    int idx = files->file_count++;
    FileEntry *entry = &files->files[idx];

    strncpy(entry->file_name, filename, MAX_FILENAME_LEN);
    entry->file_name[MAX_FILENAME_LEN - 1] = '\0';
    entry->file_size = 0;
    entry->start_cluster = FREE_CLUSTER;

    if (fs_flush_directory(cwd) != 0) {
        printf("Erro ao salvar diretório após criar arquivo\n");
        return -1;
    }

    return 0;
}


void list_dir(RootDirectory* fs) {
    if (!fs || !fs->cwd) return;

    printf("Diretórios em '%s':\n", fs->cwd->directory_name);
    
    if (fs->cwd->subdir_count > 0) {
        for (int i = 0; i < fs->cwd->subdir_count; i++) {
            if (fs->cwd->subdirs[i]) {
                printf("[DIR]  %s\n", fs->cwd->subdirs[i]->directory_name);
            }
        }
    } else {
        printf("Nenhum subdiretório\n");
    }

    printf("Arquivos:\n");
    if (fs->cwd->filesArrayReference) {
        Files *files = fs->cwd->filesArrayReference;
        
        if (files->file_count > 0) {
            for (int i = 0; i < files->file_count; i++) {
                FileEntry *f = &files->files[i];
                if (f->file_name[0] == '\0') continue;

                int clusters_used = 0;
                if (f->start_cluster != FREE_CLUSTER) {
                    int32_t cur = f->start_cluster;
                    while (cur != EOF_CLUSTER && cur != FREE_CLUSTER) {
                        clusters_used++;
                        cur = FAT[cur];
                    }
                }

                int vinicius = (f->file_size + CLUSTER_SIZE - 1) / CLUSTER_SIZE;

                int size_occupied = vinicius * f->file_size;

                if (size_occupied < CLUSTER_SIZE) size_occupied = CLUSTER_SIZE;
                if (vinicius == 0) size_occupied = 0;

                printf("[FILE] %s: %d bytes, %d bytes ocupados (%d cluster)\n",
                       f->file_name,
                       f->file_size,
                       size_occupied,
                       vinicius
                       );
            }
        } else {
            printf("Nenhum arquivo encontrado\n");
        }
    } else {
        printf("Erro: tabela de arquivos não carregada\n");
    }
}

int dir_flush(Directory *dir) {
    if (!dir || !dir->filesArrayReference) return -1;

    FILE *img = fopen(FS_IMAGE_PATH, "r+b");
    if (!img) {
        img = fopen(FS_IMAGE_PATH, "w+b");
        if (!img) return -1;
    }

    DirectoryDisk dsk;
    memset(&dsk, 0, sizeof(DirectoryDisk));
    strncpy(dsk.directory_name, dir->directory_name, MAX_FILENAME_LEN);
    memcpy(&dsk.files, dir->filesArrayReference, sizeof(Files));

    long off = (long)superblock.data_start +
               (long)dir->disk_cluster * (long)superblock.cluster_size;
    fseek(img, off, SEEK_SET);

    int result = (fwrite(&dsk, sizeof(DirectoryDisk), 1, img) == 1) ? 0 : -1;

    fclose(img);
    return result;
}




int dir_load(Directory *dir) {
    if (!dir) return -1;

    FILE *img = fopen(FS_IMAGE_PATH, "rb");
    if (!img) return -1;

    long off = (long)superblock.data_start + (long)dir->disk_cluster * superblock.cluster_size;
    if (fseek(img, off, SEEK_SET) != 0) {
        fclose(img);
        return -1;
    }

    DirectoryDisk dsk;
    if (fread(&dsk, sizeof(DirectoryDisk), 1, img) != 1) {
        fclose(img);
        return -1;
    }
    fclose(img);

    strncpy(dir->directory_name, dsk.directory_name, MAX_FILENAME_LEN);
    memcpy(dir->filesArrayReference, &dsk.files, sizeof(Files));

    return 0;
}



int fs_save_superblock(void) {
    FILE *f = fopen(FS_IMAGE_PATH, "rb+");
    if (!f) return -1;

    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -1; }
    if (fwrite(&superblock, sizeof(superblock), 1, f) != 1) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

int fs_save_fat(void) {
    if (!FAT) return -1;

    FILE *f = fopen(FS_IMAGE_PATH, "rb+");
    if (!f) return -1;

    if (fseek(f, superblock.fat_start, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    if (fwrite(FAT, sizeof(int32_t), superblock.total_clusters, f)
        != (size_t)superblock.total_clusters) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}


int fs_flush_directory(Directory *dir) {
    if (!dir) return -1;

    FILE *f = fopen(FS_IMAGE_PATH, "r+b");
    if (!f) {
        f = fopen(FS_IMAGE_PATH, "w+b");
        if (!f) return -1;
    }

    long off = (long)superblock.data_start +
               (long)dir->disk_cluster * (long)superblock.cluster_size;

    if (dir->disk_cluster == superblock.dir_start) {
        if (!dir->filesArrayReference) { 
            fclose(f); 
            return -1; 
        }
        if (fseek(f, off, SEEK_SET) != 0) { 
            fclose(f); 
            return -1; 
        }
        
        size_t n = fwrite(dir->filesArrayReference, sizeof(Files), 1, f);
        fclose(f);
        return (n == 1) ? 0 : -1;
    }

    DirectoryDisk dsk;
    memset(&dsk, 0, sizeof(dsk));

    strncpy(dsk.directory_name, dir->directory_name, MAX_FILENAME_LEN - 1);
    dsk.directory_name[MAX_FILENAME_LEN - 1] = '\0';

    if (dir->filesArrayReference) {
        memcpy(&dsk.files, dir->filesArrayReference, sizeof(Files));
    } else {
        dsk.files.file_count = 0;
    }

    dsk.parent_cluster = dir->parent ? dir->parent->disk_cluster : -1;

    if (fseek(f, off, SEEK_SET) != 0) { 
        fclose(f); 
        return -1; 
    }
    
    size_t n = fwrite(&dsk, sizeof(DirectoryDisk), 1, f);
    fflush(f);
    fclose(f);
    
    return (n == 1) ? 0 : -1;
}

int fs_save_image(RootDirectory *fs) {
    int rc = 0;
    if (fs_save_superblock() != 0) {
        rc = -1;
    }
    
    if (fs_save_fat() != 0) {
        rc = -1;
    }
    if (fs) {
        fs_flush_all(fs);
    }
    return rc;
}

static void fs_destroy_dir(Directory *dir, Files *root_table_addr) {
    if (!dir) return;

    for (int i = 0; i < dir->subdir_count; i++) {
        fs_destroy_dir(dir->subdirs[i], root_table_addr);
    }

    if (dir->filesArrayReference && dir->filesArrayReference != root_table_addr) {
        free(dir->filesArrayReference);
    }


    if (dir->subdirs) free(dir->subdirs);
    free(dir);
}
void fs_flush_all(RootDirectory *fs) {
    if (!fs || !fs->root) return;

    if (fs_flush_directory(fs->root) != 0) {
        printf("Erro ao salvar root directory\n");
    }
    flush_directory_recursive(fs->root);
}

static void flush_directory_recursive(Directory *dir) {
    if (!dir) return;
    
    for (int i = 0; i < dir->subdir_count; i++) {
        if (fs_flush_directory(dir->subdirs[i]) != 0) {
            printf("Erro ao salvar diretório '%s'\n", dir->subdirs[i]->directory_name);
        }
        flush_directory_recursive(dir->subdirs[i]);
    }
}

void fs_destroy(RootDirectory *fs) {
    if (!fs) return;

    Files *root_table_addr = &directory_table;

    if (fs->root) {
        for (int i = 0; i < fs->root->subdir_count; i++) {
            fs_destroy_dir(fs->root->subdirs[i], root_table_addr);
        }
        if (fs->root->subdirs) free(fs->root->subdirs);
        free(fs->root);
    }
    if (FAT) { free(FAT); FAT = NULL; }
    free(fs);
}

