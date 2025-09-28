#ifndef FURGFS3_FILE_SYSTEM_H
#define FURGFS3_FILE_SYSTEM_H

#include <stdint.h>

#define MAX_FILES 500
#define MAX_FILENAME_LEN 256

#ifndef MAX_SUBDIRS
#define MAX_SUBDIRS 256
#endif

#define CLUSTER_SIZE 1024
#define NUM_CLUSTERS 128
#define FS_IMAGE_PATH "furgfs.img"

#ifndef FREE_CLUSTER
#define FREE_CLUSTER -1
#endif

#ifndef EOF_CLUSTER
#define EOF_CLUSTER -2
#endif

#ifndef DIR_CLUSTER
#define DIR_CLUSTER -3
#endif

typedef struct {
    char file_name[MAX_FILENAME_LEN];
    int32_t start_cluster;
    int32_t file_size;
} FileEntry;

typedef struct {
    FileEntry files[MAX_FILES];
    int file_count;
} Files;

#pragma pack(push,1)
typedef struct {
    char directory_name[MAX_FILENAME_LEN];
    Files files;
    int32_t parent_cluster;
} DirectoryDisk;
#pragma pack(pop)

typedef struct Directory{
    char directory_name[MAX_FILENAME_LEN];
    struct Directory **subdirs;
    struct Directory *parent;
    int subdir_count;
    Files *filesArrayReference;
    int32_t disk_cluster; 
} Directory;

typedef struct {
    Directory *root;
    Directory *cwd;
} RootDirectory;

typedef struct {
    int32_t total_clusters;
    int32_t cluster_size;
    int32_t fat_start;
    int32_t dir_start;
    int32_t data_start;
    int32_t max_files;
} Superblock;

typedef struct {
    long total;
    long used;
    long free_space;
} FS_Space;

extern Superblock superblock;
extern Files directory_table;
extern int32_t *FAT;


int fs_init_image(const char* path);
int fs_load_image(RootDirectory *fs);
int fs_sync_image(void);
int fs_format_image(void);
int fs_copy_in(RootDirectory* fs, const char *host_path, const char *fs_name);
int fs_copy_out(Directory *dir, const char *fs_name, const char *host_path);
int fs_remove_file(Directory *dir, const char *fs_name);
int fs_rename(Directory *dir, const char *old_name, const char *new_name);
int fs_mkdir(Directory *parent, const char *name);
int dir_load(Directory *dir);
int dir_flush(Directory *dir);
int fs_cd(RootDirectory *fs, const char *name);
int fs_create_empty_file(RootDirectory *fs, const char *fs_name);
void fs_rmdir_recursive(Directory *dir);
void fs_list_files(RootDirectory *fs);
void list_dir(RootDirectory* fs);
void fs_destroy(RootDirectory *fs);

FS_Space fs_list_free_spaces(void);
RootDirectory* init_root_directory(void);

int fs_save_superblock(void);
int fs_save_fat(void);
int fs_flush_directory(Directory *dir);

void fs_flush_all(RootDirectory *fs); 
int fs_save_image(RootDirectory *fs);

#endif
