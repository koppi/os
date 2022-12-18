#include <lib/string.h>
#include <mm.h>
#include <kheap.h>
#include <printf.h>
#include <sched.h>
#include <vfs.h>

char senddir[64];
char dir[64];

void print_meminfo() {
    printf("Total mem: %lu MB\nFree mem: %lu MB\n",
           get_mem_size() / 1024, (get_max_blocks() - get_used_blocks()) * 4 / 1024);
    printf("Heap size: %d KB Free heap: %d KB\n",
           get_heap_size() / 1024, (get_heap_size() - get_used_heap()) / 1024);
    printf("cr0: %x cr2: %x cr3: %lx\n", get_cr0(), get_cr2(), get_pdbr());
}

char *get_argument(char *command, int n) {
    for(int i = 0; i < n; i++) {
        command = strchr(command, ' ');
        if(command) {
            command++;
        } else {
            return command;
        }
    }
    return command;
}

void console_cd(char *dir, char *command) {
    memset(senddir, 0, 64);
    strcpy(senddir, dir);
    strcat(senddir, "/");
    char *arg = get_argument(command, 1);
    if(arg) {
        strcat(senddir, arg);
        if(vfs_cd(senddir)) {
            strcpy(dir, senddir);
        } else {
            printf("cd %s: directory not found\n", senddir);
        }
    } else {
        memset(dir, 0, 64);
    }
}

void console_start(char *dir, char *command) {
    memset(senddir, 0, 64);
    strcpy(senddir, dir);
    strcat(senddir, "/");
    strcat(senddir, get_argument(command, 1));
    // Cut the arguments from the path
    if(get_argument(senddir, 1)) {
        strncpy(senddir, senddir, strlen(senddir) - strlen(get_argument(senddir, 1)) - 1);
    }

    char *arguments = kmalloc(128);
    memset(arguments, 0, 128);
    strcpy(arguments, get_argument(command, 2));

    int procn = start_proc(senddir, arguments);
    if(procn != PROC_STOPPED) {
        while(proc_state(procn) != PROC_STOPPED);
        remove_proc(procn);
        printf("\n");
    }
}

void print_file(file *f) {
    if(f->type == FS_NULL) {
        printf("Cannot open file\n");
        return;
    }

    if((f->type & FS_DIR) == FS_DIR) {
        printf("Cannot display content of directory.\n");
        return;
    }

    while(f->eof != 1) {
        char buf[512];
        vfs_file_read(f, buf);

        for(int i = 0; i < 512; i++) {
            printf("%c", buf[i]);
            if (i % 56 == 0) printf("\n");
        }
    }
}

void console_read(char *dir, char *command) {
    memset(senddir, 0, 64);
    strcpy(senddir, dir);
    strcat(senddir, "/");
    strcat(senddir, get_argument(command, 1));
    file *f = vfs_file_open(senddir, "r");
    if(f->type != FS_FILE) {
        printf("read: file %s not found\n", senddir);
    } else {
        print_file(f);
    }
    vfs_file_close(f);
}

void console_exec(char *buf) {
    if (strcmp(buf, "help") == 0) {
        printf("help - shows help\n"
               "mem  - prints RAM info\n"
               "ps   - prints process information\n");
    } else if(strcmp(buf, "mem") == 0) {
        print_meminfo();
    } else if(strcmp(buf, "ps") == 0) {
        print_procs();
    } else if(strcmp(buf, "ls") == 0) {
        if(dir[0] == 0) {
            vfs_ls();
        } else {
            vfs_ls_dir(dir);
        }
    } else if(strncmp(buf, "cd", 2) == 0) {
        console_cd(dir, buf);
    } else if(strncmp(buf, "start", 5) == 0) {
        console_start(dir, buf);
    } else if(strncmp(buf, "read", 4) == 0) {
        console_read(dir, buf);
    } else {
        printf("Command '%s' not found.\n", buf);
    }
}

