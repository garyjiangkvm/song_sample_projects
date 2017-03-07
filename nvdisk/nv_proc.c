/*
* 
*/
#include <linux/version.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsicam.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_dbg.h>
#include <linux/proc_fs.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
#include <linux/proc_ns.h>
#endif
#include <linux/seq_file.h>
#include <linux/stat.h>

#include "nvdisk.h"

#undef MODULE_NAME
#define MODULE_NAME "NVD"
#define NVD_PROC_NAME "nvdisk"


typedef struct proc_dir_entry pt_procdir_t;
typedef struct seq_file       pt_seqfile_t;
typedef sint32 (*pt_seqshow_t)(pt_seqfile_t *s, void *v);
typedef uint32 (*pt_seqwrite_t)(const char __user * buf,  uint32 len, void *data);
typedef struct pt_fops{
    struct file_operations seq_operations;
    pt_seqshow_t show;
    pt_seqwrite_t write;
}pt_fops_t;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
struct proc_dir_entry {
        unsigned int low_ino;
        umode_t mode;
        nlink_t nlink;
        kuid_t uid;
        kgid_t gid;
        loff_t size;
        const struct inode_operations *proc_iops;
        const struct file_operations *proc_fops;
        struct proc_dir_entry *next, *parent, *subdir;
        void *data;
        atomic_t count;         /* use count */
        atomic_t in_use;        /* number of callers into module in progress; */
                        /* negative -> it's going away RSN */
        struct completion *pde_unload_completion;
        struct list_head pde_openers;   /* who did ->open, but not ->release */
        spinlock_t pde_unload_lock; /* proc_fops checks and pde_users bumps */
        u8 namelen;
        char name[];
};

union proc_op {
        int (*proc_get_link)(struct dentry *, struct path *);
        int (*proc_read)(struct task_struct *task, char *page);
        int (*proc_show)(struct seq_file *m,
                struct pid_namespace *ns, struct pid *pid,
                struct task_struct *task);
};

struct proc_inode {
        struct pid *pid;
        int fd;
        union proc_op op;
        struct proc_dir_entry *pde;
        struct ctl_table_header *sysctl;
        struct ctl_table *sysctl_entry;
        struct proc_ns ns;
        struct inode vfs_inode;
};

/*
 *  * General functions
 *   */
static inline struct proc_inode *PROC_I(const struct inode *inode)
{
        return container_of(inode, struct proc_inode, vfs_inode);
}

static inline struct proc_dir_entry *PDE(const struct inode *inode)
{
        return PROC_I(inode)->pde;
}
#endif
static inline void *__PDE_DATA(const struct inode *inode)
{
        return PDE(inode)->data;
}

static inline struct pid *proc_pid(struct inode *inode)
{
        return PROC_I(inode)->pid;
}

static inline struct task_struct *get_proc_task(struct inode *inode)
{
        return get_pid_task(proc_pid(inode), PIDTYPE_PID);
}

#define SEQ_PRINT(seq, f, arg...) \
    do{seq_printf(seq, f, ##arg);}while(0)

#define pt_GetFops(fops)   container_of(fops, pt_fops_t, seq_operations)
#define pt_GetSeqPrivate(seq) ((seq)->private)

#define pt_ProcMkDir(name, parent) proc_mkdir(name, parent)
#define pt_ProcRemove(name, parent) remove_proc_entry(name, parent)

static pt_fops_t seq_nvd_info_ops;
static pt_fops_t seq_nvd_remap_ops;

static inline int default_open_fs(struct inode *inode, struct file *file)
{
    pt_fops_t *r_fops;
        
    r_fops = pt_GetFops(PDE(inode)->proc_fops);

    return single_open(file, r_fops->show, PDE(inode)->data);
}

static inline ssize_t default_seq_write(struct file *file, 
                       const char __user *buf, size_t len, loff_t *ppos)
{
    pt_fops_t *r_fops;

    r_fops = pt_GetFops(PDE(file->f_mapping->host)->proc_fops);
    return (ssize_t)(r_fops->write(buf, (uint32)len, ((pt_seqfile_t *)file->private_data)->private));
}

static pt_procdir_t *pt_ProcCreate(const char *name,
                        pt_procdir_t *parent,
                        pt_seqshow_t show,
                        pt_seqwrite_t write,
                        pt_fops_t *r_fops,
                        void *data)
{
    struct file_operations *f_ops;
    mode_t mode = S_IRUGO;

    r_fops->show = show;
    
    f_ops = &r_fops->seq_operations;
    f_ops->open = default_open_fs;
    f_ops->read = seq_read;
    f_ops->llseek = seq_lseek;
    f_ops->release = single_release;
    if(write != NULL) {
        r_fops->write = write;
        f_ops->write = default_seq_write;
        mode |= S_IWUGO;
    }

    return  proc_create_data(name, mode, parent, f_ops, data);
}


/***************************************************************************************/
char *G_dev_status[] = {"Good", "Warning", "Error", "Unknown"};

static int nvd_control_SeqShow(pt_seqfile_t *seq, void *v)
{
    int i;
    sdev_info_t *p_sdevice;
    SEQ_PRINT(seq, "version:\t%s\n", NVDISK_VERSION);

    if (limited()){
        SEQ_PRINT(seq, "limited!!\n");
    } else {
        SEQ_PRINT(seq, "%-10s %-12s %-12s %-s\n", "Id", "size(G)", "status", "Error");
        
        for (i=0;i<DEV_NUM_MAX;i++){
            p_sdevice = &G_dev_pool[i];            
            if((p_sdevice->in_used == 0)||(p_sdevice->p_sdev == NULL)) continue;
            SEQ_PRINT(seq, "%d-%d-%d-%d\t %-12lld %-12s %lld-%lld-%lld\n",
                p_sdevice->host_no, p_sdevice->p_sdev->channel, p_sdevice->p_sdev->id, p_sdevice->p_sdev->lun,   
                p_sdevice->cap_sectors/CAPACITY_1G_IN_SECTOR, G_dev_status[p_sdevice->status], 
                p_sdevice->total_tm_cnt, p_sdevice->err_count[DID_BAD_TARGET], p_sdevice->err_count[DID_ABORT]);
        }
    }
    return 0;
}

static uint32 nvd_control_SeqSet(const char __user *buf, uint32 len, void *data)
{
    uint8  k_buf[160];
    char action[32] = "";
    uint32 len_for_cp;
    uint32 config = 0;
    sdev_info_t *p_sdevice;
        
    len_for_cp = min(len, (uint32)(sizeof(k_buf) - 1));
    if(copy_from_user(k_buf, buf, len_for_cp)) {
        return -EFAULT;
    }

    sscanf(k_buf, "%s %d", action, &config);
    PT_DEBUG("Proc nvd write : action[%s] config[%d]",action,config);

    if(!strcmp(action,"recap")){
        if(config >= DEV_NUM_MAX) return len;

        p_sdevice = &G_dev_pool[config];
        if(p_sdevice->cap_sectors == 0) return len;

        nvd_scan_device(p_sdevice->p_sdev);
    }
    
    return len;
}

static sint32 nvd_remap_SeqShow(pt_seqfile_t *seq, void *v)
{
    int i,j;
    ddb_remap_result *remap;
    sdev_info_t *p_sdevice;

    SEQ_PRINT(seq ,"Max remap %d\n", MAX_REMAP_PER_DISK);
    for (i=0;i<DEV_NUM_MAX;i++){
        p_sdevice = &G_dev_pool[i];
        if(p_sdevice->cap_sectors == 0) continue;

        remap = &p_sdevice->remap;

        SEQ_PRINT(seq,  "%d-%d-%d-%d [%d] [%llx]:\n================================================\n",
            p_sdevice->host_no, p_sdevice->p_sdev->channel, p_sdevice->p_sdev->id, p_sdevice->p_sdev->lun,
            remap->no_of_remapped,remap->base_lba);
        SEQ_PRINT(seq, "%-4s %-12s %-12s %-s\n", "Index", "Sectors", "ErrLBA", "RemapLBA");
        for (j=0;j<remap->no_of_remapped;j++){
               SEQ_PRINT(seq,  "%-4d %-12d %-12llx %-llx\n", 
                j,remap->db.sectors[j],remap->db.original_lba[j],remap->db.new_lba[j]);
        }
        SEQ_PRINT(seq,  "================================================\n\n");
    }

    return 0;
}

static uint32 nvd_remap_SeqSet(const char __user * buf,  uint32 len, void *data)
{
    uint8  k_buf[32];
    uint8 *p_tmp = NULL;
    uint32 len_for_cp;
    sdev_info_t *p_sdevice;
    ddb_remap_result *remap;
    uint8 host, channel, id, lun = 0;
    uint64 base_lba;
    
    len_for_cp = min(len, (uint32)(sizeof(k_buf) - 1));
    if(copy_from_user(k_buf, buf, len_for_cp)) {
        return -EFAULT;
    }

    //sscanf(k_buf, "%s %s %d", action, name, &config);
    //PT_DEBUG("Proc remap write : action[%s] Name[%s] config[%d]",action,name,config);

    /*
	 * Usage: echo "clear 0 1 2 3" >/proc/nvdisk/remap
	 * with  "0 1 2 3" replaced by your "Host Channel Id Lun".
	 */
	p_tmp = k_buf;
	if (!strncmp("clear", p_tmp, 5)) {
		p_tmp = p_tmp + 5;

        p_tmp++;        
		host = *p_tmp-'0';
        p_tmp = p_tmp+2;
		channel = *p_tmp-'0';
        p_tmp = p_tmp+2;
		id = *p_tmp-'0';
        p_tmp = p_tmp+2;
		lun = *p_tmp-'0';
	
        printk("Get %d-%d-%d-%d\n", host, channel, id, lun);

        p_sdevice = scsi_dev_find_by_id(host, channel, id, lun);
        if(p_sdevice == NULL) return len;
        remap = &p_sdevice->remap;

        base_lba = remap->base_lba;
        memset((void *)remap,0,sizeof(ddb_remap_result));
        remap->base_lba = base_lba;
        remap->need_sync = 1;
        nvd_wakeup_sync();
    }
    return len;
}

struct proc_dir_entry *root_dir = NULL;
int nvd_proc_create(void *data)
{
    pt_procdir_t *nvd_proc = NULL;

    root_dir = pt_ProcMkDir(NVD_PROC_NAME, NULL);
    if(root_dir == NULL) {
        PT_ERR("root proc create failed.");
        return -1;
    }
    
    nvd_proc = pt_ProcCreate("info",
                        root_dir, 
                        nvd_control_SeqShow, 
                        nvd_control_SeqSet, 
                        &seq_nvd_info_ops, 
                        data);

    nvd_proc = pt_ProcCreate("remap",
                        root_dir, 
                        nvd_remap_SeqShow,
                        nvd_remap_SeqSet, 
                        &seq_nvd_remap_ops, 
                        data);
    if(nvd_proc == NULL){
        PT_ERR("info proc create failed.");
        return -1;
    }
    return 0;
}

void nvd_proc_clean(void)
{
    pt_ProcRemove("info",root_dir);
    pt_ProcRemove("remap",root_dir);
    pt_ProcRemove(NVD_PROC_NAME, NULL);
}
