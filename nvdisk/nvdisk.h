#ifndef __NVDISK_H
#define __NVDISK_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/delay.h>
//#include <linux/dma-direction.h>

#include <asm/atomic.h>

#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_eh.h>

#define HOST_NUM_MAX 2
#define DEV_NUM_MAX_PER_HOST 4
#define DEV_NUM_MAX 8
#define SCAN_HOST_MAX  128
#define HOST_TYPE_STRLEN_MAX 32
#define CAPACITY_1M_IN_SECTOR 2048
#define CAPACITY_1G_IN_SECTOR 0X200000
#define SECTOR_SIZE 512
#define DISK_SN_LEN_MAX     20

#define DEV_REMAP_SYNC_FAIL_MAX   8


#define NTLK_LISTEN_PORT    17
#define NETLINK_GROUPS_MASK 1

#define NVDISK_VERSION  "1.4"



#define LOU64(x) (uint32)((uint64)(x) & 0xffffffff)
#define HIU64(x) (uint32)((uint64)(x) >> 32)

#ifndef TRUE
#define TRUE    1
#endif

#ifndef FALSE
#define FALSE    0
#endif

#ifndef MAX
#define MAX(a,b)  (((a) > (b)) ? (a) : (b))
#define MIN(a,b)  (((a) < (b)) ? (a) : (b))
#endif

typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int        uint32;
typedef unsigned long long  uint64;

typedef signed char     sint8;
typedef signed short    sint16;
typedef signed int      sint32;
typedef signed long     s32_64;
typedef long long       sint64;


typedef struct scsi_capacity {
    uint8       lba_1;
    uint8       lba_2;
    uint8       lba_3;
    uint8       lba_4;
    uint8       size_1;
    uint8       size_2;
    uint8       size_3;
    uint8       size_4;
}scsi_capacity_t;



#define HOST_NAME_LEN_MAX 32
#ifndef SCSI_SENSE_BUFFERSIZE
#define SCSI_SENSE_BUFFERSIZE 	96
#endif

typedef void (* SCSI_FUNC_POINTER_DESTROY)(struct scsi_device *);
typedef int (* SCSI_FUNC_POINTER_CONFIG)(struct scsi_device *);
typedef int (* SCSI_FUNC_POINTER_QCOMMNAD)(struct Scsi_Host *, struct scsi_cmnd *);
typedef void (* SCSI_FUNC_POINTER_DONE)(struct scsi_cmnd *cmd);
typedef void (* SCSI_FUNC_POINTER_ERHANDLER)(struct Scsi_Host *);
typedef enum blk_eh_timer_return (* SCSI_FUNC_POINTER_TIMEOUT)(struct scsi_cmnd *);

#define DDB_REMAP_MAGIC 0xdeadbeef

#define MAX_REMAP_PER_DISK 1024
typedef struct ddb_remap_db_t{
    uint64 original_lba[MAX_REMAP_PER_DISK];
    uint64 new_lba[MAX_REMAP_PER_DISK];
    uint32 sectors[MAX_REMAP_PER_DISK];
}ddb_remap_db; //20K
    
typedef struct ddb_remap_result_t{ //make sure it aligned with SECTOR size
    uint64 magic;
    uint64 unique;
    uint64 base_lba;
    uint16 block_sectors; //how many sectors in one block
    uint16 no_of_remapped; 
    uint16 need_sync;
    uint8 sync_failed;
    uint8 rsvd1;
    ddb_remap_db db;
    uint8 rsvd[64*SECTOR_SIZE - sizeof(ddb_remap_db) - 4*sizeof(uint64)];
}ddb_remap_result; //32K

#define ERR_RESULT_MAX_TYPE 0x20

enum dev_proc_status{
  DEV_GOOD,
  DEV_ERR,
  DEV_FAIL,
  DEV_MAX,
};

typedef struct sdev_info{
    uint8 index;
    uint8 in_used;
    uint8 status;
    uint8 reserved;
    uint64  cap_sectors;  //512B per sector
    struct scsi_device  *p_sdev;
    unsigned char sense[SCSI_SENSE_BUFFERSIZE];
    uint64 err_count[ERR_RESULT_MAX_TYPE];
    ddb_remap_result remap;
	uint64 total_tm_cnt;
    uint64 curr_tm_cnt;
    uint64 curr_prc_cnt;    
    int host_no;
    int channel;
    int id;
    int lun;
    uint8 sn[DISK_SN_LEN_MAX];
}sdev_info_t;

typedef struct host_info_s{
    char name[HOST_NAME_LEN_MAX];   
    int pt_queue_depth;
    int scsi_queue_depth; //set scsi queue depth higer than internal queue depth to give it a bit cusion
    int min_cap;
    int replace_done;
    unsigned int host_no;
    struct Scsi_Host *p_shost;
    SCSI_FUNC_POINTER_DESTROY old_slave_destroy;
    SCSI_FUNC_POINTER_CONFIG old_slave_configure; 
    SCSI_FUNC_POINTER_QCOMMNAD old_queue_command;
    SCSI_FUNC_POINTER_DONE     old_scsi_done;
    SCSI_FUNC_POINTER_ERHANDLER  old_eh_handle;
    SCSI_FUNC_POINTER_TIMEOUT    old_timeout;
}host_info_t;


#define MAGIC_NUM     0xfe45798f
enum{
    MSG_TYPE_DISK_ERR,
    MSG_TYPE_DISK_STATE,
    MSG_TYPE_REMAP_EVENT,
    MSG_TYPE_DISK_ONOFF,
    MSG_TYPE_MAX,
};


typedef struct ntlk_msg{
    uint32 magic_num;          //MAGIC_NUM
	uint32 t_index;            //TYPE 0-Disk error; 1-Disk state change event; 2-Remap event;
	uint8 no[4];               //Disk location, i.e. 0-0-0-0	
	char sn[DISK_SN_LEN_MAX+1];	 //Disk sn
	uint8 reserved[11];
	union {
		struct{				
			uint8 status;         //Disk status 0-Good; 1-Warning; 2-Error;
			uint8 reserved[3]; 
			
			uint32 tm_cnt;         //timeout count
			uint32 err_cnt;        //error count
			uint32 abt_cnt;        //abort count			
		}io_info;
		
		struct{
			uint32 remap_cnt;      //remap cnt
			uint32 sectors;        //current remap IO sectors
			uint64 org_lba;        //current remap IO orginal LBA
			uint64 new_lba;				 //current remap IO new LBA
		}remap;	

        struct{
			uint8 state;      //0-online; 1-offline;
		}onoff;
	}details;
}__attribute__((packed))nltk_msg_t;

typedef struct netlink_msg_node{
    nltk_msg_t      msg;
    struct list_head  linked;
}__attribute__((packed))netlink_msg_node_t;

enum{
    NVD_RET_SUCC,
    NVD_RET_FAIL,
    NVD_RET_INT_ERR,
    NVD_RET_HTYPE_INVALID,
    NVD_RET_DEVICE_NOSPACE,
    NVD_RET_ALLOC_FAILED,
    NVD_RET_MAX,
};

/* Config */
//#define DEV_REMOVE_IN_KERNEL
#define USED_TIME_LIMITED 
#ifdef USED_TIME_LIMITED
#define EXPIRED_TIME_JIFFIES  1464710400  //2016-6-1
#endif
/********************************************
     debug 
*********************************************/
//#define DEBUG_ON
#ifdef DEBUG_ON
#define PT_DEBUG(f, arg...) \
    do { \
        printk(KERN_EMERG  "Debug: " f "\n", ## arg); \
    }while(0)

        
#define PT_DEBUG_LIMIT(f, arg...) \
            do { \
                if (printk_ratelimit()) printk(KERN_EMERG  "Debug: " f "\n", ## arg); \
            }while(0)

#else
#define PT_DEBUG(f, arg...)
#define PT_DEBUG_LIMIT(f, arg...)
#endif

#define PT_INFO(f, arg...) \
    do { \
        printk(KERN_EMERG  "Info: " f "\n", ## arg); \
    }while(0)


    
#define PT_ERR(f, arg...) \
        do { \
            printk(  "Err: " f "\n", ## arg); \
        }while(0)

#define __FILE_NAME__ \
     (strrchr(__FILE__,'/') \
    ? strrchr(__FILE__,'/')+1 \
    : __FILE__ )\

#define ASSERT(x) {\
    if(!(x)) {\
        printk(KERN_EMERG "Debug:  File:%s, FUNC:%s, Line:%d.Condition "#x" failed.\n",\
        __FILE_NAME__, __FUNCTION__, __LINE__);\
    }\
}


//void dStrHex(void* str, int len, long start, int noAddr);
    
int nvd_proc_create(void *data);
void nvd_proc_clean(void);
void nvd_thread_clean(void);
int nvd_scan_device(struct scsi_device *p_sdev);

uint32 nvd_add_remap_request(struct scsi_cmnd *p_cmd, sdev_info_t *p_sdevice);
void nvd_init_thread(void);
void nvd_wakeup_sync(void);
int nvd_remap_init_and_load(sdev_info_t *p_sdevice);
int nvd_find_remapped_lba(sdev_info_t *p_sdevice, struct scsi_cmnd *p_cmd);
sdev_info_t *scsi_dev_find_by_id(uint8 host, uint8 channel, uint8 id, uint8 lun);


int netlink_init(void);
void netlink_exit(void);
int netlink_enq(sdev_info_t *p_dev, uint32 type, uint8 onoff);
void limited_set(void);
int limited(void);


extern sdev_info_t G_dev_pool[];

#endif
