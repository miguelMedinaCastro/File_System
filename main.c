/**
 * @file      main.c
 * @author    Miguel Medina; Milena Bueno
 * @brief     FS FURGfs3
 * @version   0.1
 * @date      04-09-2025
 * @copyright 2025, Universidade Federal do Rio Grande
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "include/init_file_system.h"
#include "include/file_system.h"

#define MAX_INPUT 512

static void print_prompt(RootDirectory *fs) {
    Directory *cwd = fs->cwd;
    if (cwd == fs->root) {
        printf("FURGfs3:/ > ");
        fflush(stdout);
        return;
    }

    Directory *stack[256];
    int top = 0;
    Directory *d = cwd;
    while (d && d != fs->root) {
        stack[top++] = d;
        d = d->parent;
    }

    printf("FURGfs3:/");
    for (int i = top - 1; i >= 0; i--) {
        printf("%s", stack[i]->directory_name);
        if (i != 0) printf("/");
    }
    printf(" > ");
    fflush(stdout);
}

static void persist_all_changes(RootDirectory *fs) {
    fs_save_fat();
    fs_flush_all(fs);
    fs_save_image(fs);
}

int main(void) {
    RootDirectory *fs = init_file_system();
    if (!fs) {
        fprintf(stderr, "Falha em init_file_system()\n");
        return 1;
    }
    fs->cwd = fs->root;

    if (access(FS_IMAGE_PATH, F_OK) != 0) {
        printf("Criando novo sistema de arquivos...\n");
        if (fs_format_image() != 0) {
            fprintf(stderr, "fs_format_image() falhou\n");
            return 1;
        }
    }

    if (fs_load_image(fs) != 0) {
        fprintf(stderr, "Erro ao carregar imagem do disco\n");
        return 1;
    }

    printf("Sistema carregado.\n");

    char input[MAX_INPUT];
    while (1) {
        print_prompt(fs);

        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = '\0';

        char *cmd = strtok(input, " ");
        if (!cmd) continue;

        if (strcmp(cmd, "exit") == 0) {
            break;

        } else if (strcmp(cmd, "ls") == 0) {
            list_dir(fs);

        } else if (strcmp(cmd, "mkdir") == 0) {
            char *name = strtok(NULL, " ");
            if (name) {
                if (fs_mkdir(fs->cwd, name) == 0) {
                    // printf("Diretório '%s' criado com sucesso\n", name);
                    persist_all_changes(fs);
                } else {
                    printf("Erro ao criar diretório '%s'\n", name);
                }
            } else {
                printf("Uso: mkdir <nome>\n");
            }

        } else if (strcmp(cmd, "cd") == 0) {
            char *name = strtok(NULL, " ");
            if (!name) {
                printf("Uso: cd <nome>\n");
            } else {
                if (fs_cd(fs, name) != 0) {
                    printf("Diretório não encontrado: %s\n", name);
                } else {
                    dir_load(fs->cwd);
                }
            }

        } else if (strcmp(cmd, "copy_in") == 0) {
            char *host = strtok(NULL, " ");
            char *fs_name = strtok(NULL, " ");
            if (host && fs_name) {
                if (fs_copy_in(fs, host, fs_name) == 0) {
                    fs_flush_directory(fs->cwd);
                } else {
                    printf("Erro no copy_in\n");
                }
            } else {
                printf("Uso: copy_in <host_path> <fs_name>\n");
            }

        } else if (strcmp(cmd, "copy_out") == 0) {
            char *fs_name = strtok(NULL, " ");
            char *host = strtok(NULL, " ");
            if (fs_name && host) {
                if (fs_copy_out(fs->cwd, fs_name, host) != 0) {
                    printf("Erro no copy_out\n");
                }
            } else {
                printf("Uso: copy_out <fs_name> <host_path>\n");
            }

        } else if (strcmp(cmd, "rm") == 0) {
            char *name = strtok(NULL, " ");
            if (!name) {
                printf("Uso: rm <nome>\n");
            } else {
                int found_dir = -1;
                for (int i = 0; i < fs->cwd->subdir_count; i++) {
                    if (strcmp(fs->cwd->subdirs[i]->directory_name, name) == 0) {
                        found_dir = i; 
                        break;
                    }
                }
                if (found_dir != -1) {
                    fs_rmdir_recursive(fs->cwd->subdirs[found_dir]);

                    const int target_cluster = fs->cwd->subdirs[found_dir]->disk_cluster;
                    if (target_cluster >= 0 && target_cluster < superblock.total_clusters) {
                        FAT[target_cluster] = FREE_CLUSTER;
                        fs_save_fat();
                    }

                    FILE *img = fopen(FS_IMAGE_PATH, "r+b");
                    if (img) {
                        DirectoryDisk empty;
                        memset(&empty, 0, sizeof(empty));
                        long off = (long)superblock.data_start + (long)target_cluster * (long)superblock.cluster_size;
                        fseek(img, off, SEEK_SET);
                        fwrite(&empty, sizeof(DirectoryDisk), 1, img);
                        fflush(img);
                        fclose(img);
                    }

                    for (int i = found_dir; i < fs->cwd->subdir_count - 1; i++) {
                        fs->cwd->subdirs[i] = fs->cwd->subdirs[i + 1];
                    }

                    fs->cwd->subdir_count--;
                    fs_flush_directory(fs->cwd);

                } else if (fs_remove_file(fs->cwd, name) == 0) {
                    fs_flush_directory(fs->cwd);
                } else {
                    printf("Arquivo ou diretório '%s' não encontrado.\n", name);
                }
            }

        } else if (strcmp(cmd, "mv") == 0) {
            char *old_name = strtok(NULL, " ");
            char *new_name = strtok(NULL, " ");
            if (!old_name || !new_name) {
                printf("Uso: mv <old_name> <new_name>\n");
            } else {
                if (fs_rename(fs->cwd, old_name, new_name) == 0) {
                    fs_flush_directory(fs->cwd);
                } else {
                    printf("Erro ao renomear '%s'\n", old_name);
                }
            }

        } else if (strcmp(cmd, "space") == 0) {
            FS_Space s = fs_list_free_spaces();
            printf("Total: %ld, Usado: %ld, Livre: %ld\n", s.total, s.used, s.free_space);

        } else if (strcmp(cmd, "touch") == 0) {
            char *name = strtok(NULL, " ");
            if (!name) {
                printf("Uso: touch <nome>\n");
            } else {
                if (fs_create_empty_file(fs, name) == 0) {
                    // printf("Arquivo '%s' criado com sucesso\n", name);
                    persist_all_changes(fs);
                } else {
                    printf("Erro ao criar arquivo '%s'\n", name);
                }
            }

        } else {
            printf("Comando desconhecido: %s\n", cmd);
        }
    }

    printf("Saindo...\n");
    fs_flush_all(fs);
    fs_save_image(fs);
    fs_destroy(fs);
    
    return 0;
}
