#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mount.h> 
#include <sys/stat.h>

#define MAX_CMD_LEN 100
#define MAX_ARGS 10

int main() {
    char command[MAX_CMD_LEN];
    char *args[MAX_ARGS];

    // --- AUTOMATIC SETUP ---
    if (mount("proc", "/proc", "proc", 0, NULL) != 0) printf("Warning: Failed to mount /proc\n");
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);

    mkdir("/dev/pts", 0755);
    if (mount("devpts", "/dev/pts", "devpts", 0, NULL) != 0) printf("Warning: Failed to mount /dev/pts\n");

    // Wrapper Scripts (The Master Fix)
    setenv("TERM", "linux", 1);
    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin", 1);

    FILE *bash_script = fopen("/bin/bash", "w");
    if (bash_script) {
        fprintf(bash_script, "#!/bin/sh\nexec /bin/sh \"$@\"\n");
        fclose(bash_script);
        chmod("/bin/bash", 0755);
    }
    
    // --- THE NEW BDH SUPER WRAPPER ---
    FILE *bdh_wrapper = fopen("/bin/bdh", "w");
    if (bdh_wrapper) {
        fprintf(bdh_wrapper, "#!/bin/sh\n"
                             "eval $(/bin/resize 2>/dev/null)\n"
                             "stty rows $LINES cols $COLUMNS 2>/dev/null\n"
                             "exec /bin/bdh-engine \"$@\"\n");
        fclose(bdh_wrapper);
        chmod("/bin/bdh", 0755);
    }

    pid_t setup_pid = fork();
    if (setup_pid == 0) {
        char *setup_args[] = {"/bin/busybox", "--install", "-s", "/bin", NULL};
        execv(setup_args[0], setup_args);
        exit(1); 
    } else if (setup_pid > 0) {
        waitpid(setup_pid, NULL, 0); 
    }
    
    // --- AUTOMATIC NETWORK & DISK DRIVERS SETUP ---
    printf("Initializing Network and Drivers...\n");
    system("/bin/modprobe virtio_pci 2>/dev/null"); 
    system("/bin/modprobe virtio_net 2>/dev/null"); 
    
    system("/bin/modprobe virtio_blk 2>/dev/null"); 
    system("/bin/modprobe ext4 2>/dev/null");
    system("/bin/modprobe ext2 2>/dev/null");
    
    system("/bin/ifconfig lo up 2>/dev/null");
    system("/bin/ifconfig eth0 10.0.2.15 netmask 255.255.255.0 up 2>/dev/null");
    system("/bin/route add default gw 10.0.2.2 2>/dev/null");
    system("echo 'nameserver 8.8.8.8' > /etc/resolv.conf"); 

    // --- AUTOMATIC PERSISTENT DISK MOUNT & SYMLINK RESTORE ---
    printf("Initializing Persistent Storage...\n");
    system("mkdir -p /bdh_drive");
    
    if (system("mount -t ext2 /dev/vda /bdh_drive 2>/dev/null") != 0) {
        printf("   -> Formatting new virtual disk...\n");
        system("/bin/mkfs.ext2 /dev/vda 2>/dev/null");
        system("mount -t ext2 /dev/vda /bdh_drive 2>/dev/null");
    }
    
    system("mkdir -p /bdh_drive/apps");
    system("ln -sf /bdh_drive/apps/* /bin/ 2>/dev/null");

    // --- OS NAME CHANGED HERE ---
    printf("\n======================================\n");
    printf("  Welcome to BDH Linux\n");
    printf("======================================\n");

    // --- NEW DYNAMIC LOGIN SYSTEM ---
    char saved_pass[50] = {0};
    FILE *pf = fopen("/bdh_drive/root_pass.txt", "r");
    
    if (pf == NULL) {
        printf("\n[ First Boot: Setup Root Password ]\n");
        while (1) {
            char *new_pass = getpass("Set Root Password: ");
            char temp_pass[50];
            strcpy(temp_pass, new_pass);
            
            char *confirm = getpass("Retype Password: ");
            if (strcmp(temp_pass, confirm) == 0 && strlen(temp_pass) > 0) {
                pf = fopen("/bdh_drive/root_pass.txt", "w");
                if(pf) {
                    fprintf(pf, "%s", temp_pass);
                    fclose(pf);
                    strcpy(saved_pass, temp_pass);
                    printf("Password saved securely!\n\n");
                    break;
                }
            } else {
                printf("Passwords do not match or empty. Try again.\n");
            }
        }
    } else {
        fgets(saved_pass, sizeof(saved_pass), pf);
        fclose(pf);
        saved_pass[strcspn(saved_pass, "\n")] = 0;
    }

    int logged_in = 0;
    char username[50];
    
    while (!logged_in) {
        printf("bdhlinux login: ");
        fflush(stdout);
        
        if (fgets(username, sizeof(username), stdin) == NULL) continue;
        username[strcspn(username, "\n")] = 0; 
        
        if (strlen(username) == 0) continue;

        char *pass = getpass("Password: ");
        
        if (strcmp(username, "root") == 0 && strcmp(pass, saved_pass) == 0) {
            printf("\nLast login: Today on ttyAMA0\n");
            logged_in = 1;
        } else {
            printf("\nLogin incorrect\n\n");
        }
    }
    // -------------------------

    while (1) {
        // --- PROMPT CHANGED HERE ---
        printf("\n[root@bdhlinux ~]# ");
        fflush(stdout);
        if (fgets(command, sizeof(command), stdin) == NULL) { clearerr(stdin); continue; }
        command[strcspn(command, "\n")] = 0;
        if (strlen(command) == 0) continue;
        if (strcmp(command, "exit") == 0) { printf("Init system cannot exit!\n"); continue; }

        int i = 0;
        char *token = strtok(command, " ");
        while (token != NULL && i < MAX_ARGS - 1) { args[i++] = token; token = strtok(NULL, " "); }
        args[i] = NULL;

        if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) printf("cd: missing argument\n");
            else if (chdir(args[1]) != 0) perror("cd failed");
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            if (execvp(args[0], args) == -1) { printf("Command not found: %s\n", args[0]); exit(1); }
        } else if (pid > 0) {
            waitpid(pid, NULL, 0);
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }
    return 0;
}
