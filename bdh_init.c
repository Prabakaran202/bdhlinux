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
    // இது ஒவ்வொரு முறையும் எஞ்சினைத் திறக்கும் முன் அளவைச் சரியாக செட் செய்துவிடும்!
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
    
    // --- AUTOMATIC NETWORK SETUP WITH STATIC IP ---
    printf("🌐 Initializing Network...\n");
    system("/bin/modprobe virtio_pci 2>/dev/null"); 
    system("/bin/modprobe virtio_net 2>/dev/null"); 
    system("/bin/ifconfig lo up 2>/dev/null");
    
    // DHCP-க்குக் காத்திருக்காமல் நேரடியாக IP செட் செய்கிறோம்!
    system("/bin/ifconfig eth0 10.0.2.15 netmask 255.255.255.0 up 2>/dev/null");
    system("/bin/route add default gw 10.0.2.2 2>/dev/null");
    system("echo 'nameserver 8.8.8.8' > /etc/resolv.conf"); 

    // --- OS NAME CHANGED HERE ---
    printf("======================================\n");
    printf("  Welcome to BDH Linux\n");
    printf("======================================\n");

    while (1) {
        // --- PROMPT CHANGED HERE ---
        printf("bdhlinux # ");
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
