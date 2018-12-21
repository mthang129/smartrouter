/**
 *  procfs2.c -  create a "file" in /proc
 *
 */

 #include <linux/module.h>   /* Specifically, a module */
 #include <linux/kernel.h>   /* We're doing kernel work */
 #include <linux/proc_fs.h>  /* Necessary because we use the proc fs */
 #include <asm/uaccess.h>    /* for copy_from_user */
 #include <linux/time.h>
 #include <linux/delay.h>
 #include <linux/list.h>
 #include "net/mac80211.h"
 
MODULE_LICENSE("GPL"); 

 #define PROCFS_MAX_SIZE     2048
 #define PROCFS_NAME         "check_client_signal"
 #define MAC_ADDRESS_LENGTH 6
 
 struct timespec ts_start, ts_end, ts_subtime;

 int result_char_count = 1;
 int bytes = 0;
 u8 srcAddr[6];
 u8 disAddr[6];
 u8 recAddr[6];
 char* output;
 char outp[2048];
 int procfile_open(struct inode *inode, struct  file *file);
 ssize_t procfile_read(struct file *filep, char *buffer, size_t len, loff_t *offset);
 ssize_t procfile_write(struct file *filep, const char *buffer, size_t len, loff_t *offset);
//  int __init procfile_init(void);
 
 /**
  * This structure hold information about the /proc file
  *
  */
 struct proc_dir_entry *proc_file_entry;
 
 const struct file_operations proc_file_fops = {
  .owner = THIS_MODULE,
  .open  = procfile_open,
  .read  = procfile_read,
  .write = procfile_write,
 };
 
 int procfile_open(struct inode *inode, struct  file *file) {
     printk(KERN_INFO "Openning sample procfile\n");
     return 0;    
 }
 
 /**
  * The buffer used to store character for this module
  *
  */
 char procfs_buffer[PROCFS_MAX_SIZE];
 
u8 string_to_int(const char * s) {
    u8 result = 0;
    int c ;
    if ('0' == *s && 'x' == *(s+1)) { s+=2;
        while (*s) {
        result = result << 4;
        if (c=(*s-'0'),(c>=0 && c <=9)) result|=c;
        else if (c=(*s-'A'),(c>=0 && c <=5)) result|=(c+10);
        else if (c=(*s-'a'),(c>=0 && c <=5)) result|=(c+10);
        else break;
        ++s;
        }
    }
    return result;
}

void reverse(char* s, int i)
{
   int j;
   for(j = 0; j < i/2; j++) {
       char temp = s[j];
       s[j] = s[i-j-1];
       s[i-j-1] = temp;
   }  
}     

// int to char
char* itoa(int num, char* str, int base)
{
    int i = 0;
    int isNegative = 0;
    int tmp = num;
 
    /* Handle 0 explicitely, otherwise empty string is printed for 0 */
    if (num == 0 || num < -99)
    {
        str[i++] = '0';
        str[i++] = '0';             // 0 -> 00
        str[i] = '\0';
        return str;
    }
 
    // In standard itoa(), negative numbers are handled only with 
    // base 10. Otherwise numbers are considered unsigned.
    if (num < 0 && base == 10)
    {
        isNegative = 1;
        num = -num;
    }
 
    // Process individual digits
    while (num != 0)
    {
        int rem = num % base;
        str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
        num = num/base;
    }
    if (1 < tmp && tmp < base) {       // a ->0a
        str[i++] = '0';
    }
    // If number is negative, append '-'
    if (isNegative)
        str[i++] = '-';
 
    str[i] = '\0'; // Append string terminator
    reverse(str,i);
    return str;
}

char* addchar(char* input, char c) {
    char* output = kmalloc(sizeof(char)*(result_char_count+1), GFP_KERNEL);
    int i = 0;
    for(i = 0; i < result_char_count-1; ++i) {
        output[i] = input[i];
    }
    output[result_char_count-1] = c;
    output[result_char_count] = '\0';
    result_char_count++;
    bytes++;
    kfree(input);
    return output;  
}

char* list_to_string(struct packet_mac_signal input) {
    lock_client_info_list();
    output = kmalloc(sizeof(char), GFP_KERNEL);
    output[0] = '\0';
    int count = 0;
    int n = 0, i = 0, j;
	struct list_head *node, *temp;
    struct packet_mac_signal *current_node, *tmpnode;
	list_for_each(node, &(input.list)){
        current_node = list_entry(node,struct packet_mac_signal,list);
        if(count != 0 && current_node == list_first_entry(&(input.list), struct packet_mac_signal, list)) {
            break;
        }
        if(current_node->MACaddr) {
            for(j = 0; j < MAC_ADDRESS_LENGTH; ++j) {
                output = addchar(output, '0');
                output = addchar(output, 'x');
                char temp[3];
                itoa(current_node->MACaddr[j], temp, 16);
                output = addchar(output, temp[0]);
                output = addchar(output, temp[1]);
                if(j < MAC_ADDRESS_LENGTH - 1) {
                    output = addchar(output, ':');
                }
            }
        }
        output = addchar(output, ';');
        char temp[GOOD_SIGNAL_THRESHOLD < -100 ? 5 : 4];
        itoa(current_node->signal, temp, 10);
        for(j = 0; j < 3; j++) {
            output = addchar(output, temp[j]);
        }
        output = addchar(output, '#');
        count++;
    }
    output = addchar(output, '\0');
    unlock_client_info_list();
    return output;
}



 /**
  * The size of the buffer
  *
  */
 unsigned long procfs_buffer_size = 0;
 
 /** 
  * This function is called then the /proc file is read
  *
  */
 //int procfile_read(char *buffer, char **buffer_location, off_t offset, int buffer_length, int *eof, void *data) {
 ssize_t procfile_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
     ssize_t num_left, num_read;
     printk(KERN_INFO "procfile_read (/proc/%s) called %ld %lld\n", PROCFS_NAME, len, *offset);
     num_left = PROCFS_MAX_SIZE - (*offset); 
     num_read = len; 
     if (num_read > num_left)
         num_read = num_left;
     if (num_read > 0) {
         /* fill the buffer, return the buffer size */
        //  copy_to_user(buffer, procfs_buffer + (*offset), num_read);
        char c = (char) procfs_buffer[*(offset)];
        printk("\n Buffer 0: %c", c);
        if (c == '0') {          //0xaa:0xbb:0xcc:0xdd:0xee:0xff
            printk("\n mac");
            int k = 0;
            for(k = 0; k < 6; k++) {
                char tmp[5];
                tmp[0] = '0';
                tmp[1] = 'x';
                tmp[2] = (char)procfs_buffer[*(offset) + 2 + 5*k];
                tmp[3] = (char)procfs_buffer[*(offset) + 3 + 5*k];
                tmp[4] = '\0'; 
                srcAddr[k] = string_to_int(tmp);
            }
            prom_start(srcAddr);
   
            msleep(2000);
            int t = packet_scan();
            printk("%%%%%% %d", t);
            int length = 10;
            int arr[length];
   
            char temp[10];
            itoa(t, temp, 10);
            copy_to_user(buffer, temp, num_read);
            *offset += num_read;
        } else if (c == 'c') {      //checkpacket
            struct packet_mac_signal temp = get_client_signal_info();
            char* output;
            char * out  =  list_to_string(temp);
            int j;
            for(j = 0; j < result_char_count; j++) {
                outp[j] = out[j];
            }
            int cp = copy_to_user(buffer, outp, result_char_count);
            *offset += result_char_count;
            result_char_count = 1;
            reset_client_signal_info();
        }    
     }
     else {
         *offset = 0;
         procfs_buffer_size = 0;
     }
     printk(KERN_INFO "procfile_read copied %ld bytes\n", num_read);
     return num_read;
 }
 
 /**
  * This function is called with the /proc file is written
  *
  */
 ssize_t procfile_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
 
     printk(KERN_INFO "procfile_write (/proc/%s) called %ld\n", PROCFS_NAME, len);
     /* get buffer size */
     procfs_buffer_size = len;
     if (procfs_buffer_size > PROCFS_MAX_SIZE ) {
         procfs_buffer_size = PROCFS_MAX_SIZE;
     }
     
     /* write data to the buffer */
     if ( copy_from_user(procfs_buffer, buffer, procfs_buffer_size) ) {
         return -EFAULT;
     }
     printk("Write buffer: %s", buffer);
     printk("Write procbuffer: %s", procfs_buffer);
     printk(KERN_INFO "procfile_write copied %ld bytes\n", procfs_buffer_size);

    char c = (char)procfs_buffer[0];
    return procfs_buffer_size;
 }
 
 /**
  *This function is called when the module is loaded
  *
  */
int __init init_module()
 {
     /* create the /proc file */
     proc_file_entry = proc_create(PROCFS_NAME, S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH, NULL, &proc_file_fops);
     
     if (proc_file_entry == NULL) {
         remove_proc_entry(PROCFS_NAME, NULL);
         printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",
             PROCFS_NAME);
         return 1;
     }
 
     printk(KERN_INFO "/proc/%s created\n", PROCFS_NAME);    
     return 0;   /* everything is ok */
 }
 
 /**
  *This function is called when the module is unloaded
  *
  */
void __exit cleanup_module(void)
 {
     remove_proc_entry(PROCFS_NAME, proc_file_entry);
     printk(KERN_INFO "/proc/%s removed\n", PROCFS_NAME);
 }
 

 
