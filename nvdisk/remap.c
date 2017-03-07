#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/completion.h>

#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_eh.h>

#include "nvdisk.h"

static void nvd_scsi_cdb(uint8 op, uint64 block_address, uint32 xfer_blocks, uint8 *p_cdb)
{   
    uint32 lba_l = LOU64(block_address);
    uint32 lba_h = HIU64(block_address);

    if (lba_h == 0x0){
        p_cdb[5] = (uint8)(lba_l & 0x000000ff);
        p_cdb[4] = (uint8)((lba_l & 0x0000ff00)>>8);
        p_cdb[3] = (uint8)((lba_l & 0x00ff0000)>>16);
        p_cdb[2] = (uint8)((lba_l & 0xff000000)>>24);
    } else {
    
        p_cdb[0] = op;
        
        p_cdb[1] = 0x0;
        p_cdb[9] = (uint8)(lba_l & 0x000000ff);
        p_cdb[8] = (uint8)((lba_l & 0x0000ff00)>>8);
        p_cdb[7] = (uint8)((lba_l & 0x00ff0000)>>16);
        p_cdb[6] = (uint8)((lba_l & 0xff000000)>>24);
        p_cdb[5] = (uint8)(lba_h & 0x000000ff);
        p_cdb[4] = (uint8)((lba_h & 0x0000ff00)>>8);
        p_cdb[3] = (uint8)((lba_h & 0x00ff0000)>>16);
        p_cdb[2] = (uint8)((lba_h & 0xff000000)>>24);
        p_cdb[10] = (uint8)((xfer_blocks & 0xff000000)>>24);
        p_cdb[11] = (uint8)((xfer_blocks & 0x00ff0000)>>16);
        p_cdb[12] = (uint8)((xfer_blocks & 0x0000ff00)>>8);
        p_cdb[13] = (uint8)(xfer_blocks & 0x000000ff);
        p_cdb[14] = 0;
        p_cdb[15] = 0;
    
    }
}   

static int nvd_rw_disk(struct scsi_device *p_sdev, uint64 lba, void *buffer, unsigned len, int dir)
{
    uint8 cdb[16] = {0};
    uint32 ret = NVD_RET_SUCC;

    if (unlikely(!scsi_device_online(p_sdev))) {
        PT_ERR("Remap meta: Disk is offline!");
        return NVD_RET_INT_ERR;
    }

    nvd_scsi_cdb(dir == DMA_TO_DEVICE ? 0x8a : 0x88,lba,len/SECTOR_SIZE, cdb);

    ret = scsi_execute(p_sdev, cdb, dir, //DMA_FROM_DEVICE 
            (uint8 *)buffer,
            len,
            NULL,
            6 * HZ,
            3,
            0,
            NULL);

    if(ret != 0){
        PT_ERR("Remap meta: Disk read write failed!");
        return NVD_RET_INT_ERR;
    }
    return NVD_RET_SUCC;
}

static int nvd_remap_rw_disk(sdev_info_t *p_sdevice, int dir)
{
    uint64 lba = p_sdevice->cap_sectors - CAPACITY_1G_IN_SECTOR;

    if(p_sdevice->p_sdev == NULL){
        ASSERT(FALSE);
        return NVD_RET_INT_ERR;
    }

    return nvd_rw_disk(p_sdevice->p_sdev, lba, (void *)&p_sdevice->remap, sizeof(ddb_remap_result),dir);
}

/* Funtion to find a remap index for an lba. */
int nvd_if_lba_remapped(sdev_info_t *p_sdevice,uint64 error_lba,uint32 sectors,uint64 *p_new_lba)
{
    uint i;
    ddb_remap_result *remap;

    if(p_sdevice == NULL) return FALSE;

    remap = &p_sdevice->remap;
    if(remap == NULL) return FALSE;

    //scan all remapped
    for(i=0;i<remap->no_of_remapped;i++){
          if(error_lba == remap->db.original_lba[i] && sectors == remap->db.sectors[i]){
              //error lba already been mapped
              *p_new_lba = remap->db.new_lba[i];
               PT_DEBUG_LIMIT("%d-%d-%d-%d Found and use remap: index[%d], orig[%llx], new[%llx]",
                        p_sdevice->host_no, p_sdevice->p_sdev->id, p_sdevice->p_sdev->lun, p_sdevice->p_sdev->channel,
                        i, remap->db.original_lba[i],remap->db.new_lba[i]);
              return TRUE;
          }
    }

    return FALSE;
}

int nvd_find_remapped_lba(sdev_info_t *p_sdevice, struct scsi_cmnd *p_cmd)
{
    uint64 new_lba;
    uint64 error_lba = blk_rq_pos(p_cmd->request);
    uint32 sectors = blk_rq_sectors(p_cmd->request);

    if (nvd_if_lba_remapped(p_sdevice, error_lba,sectors, &new_lba)){
        //replace cdb
        PT_DEBUG("\n==========Before Replace IO here CDB 0x%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x .\n",
            p_cmd->request->__cmd[0], p_cmd->request->__cmd[1],p_cmd->request->__cmd[2],p_cmd->request->__cmd[3],
            p_cmd->request->__cmd[4],p_cmd->request->__cmd[5],p_cmd->request->__cmd[6],p_cmd->request->__cmd[7],
            p_cmd->request->__cmd[8],p_cmd->request->__cmd[9],p_cmd->request->__cmd[10],p_cmd->request->__cmd[11],
            p_cmd->request->__cmd[12],p_cmd->request->__cmd[13],p_cmd->request->__cmd[14],p_cmd->request->__cmd[15]);
        nvd_scsi_cdb(p_cmd->sc_data_direction == DMA_TO_DEVICE ? 0x8a : 0x88,new_lba,sectors, p_cmd->cmnd);
        PT_DEBUG("\n==========After Replace IO here CDB 0x%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x .\n",
            p_cmd->request->__cmd[0], p_cmd->request->__cmd[1],p_cmd->request->__cmd[2],p_cmd->request->__cmd[3],
            p_cmd->request->__cmd[4],p_cmd->request->__cmd[5],p_cmd->request->__cmd[6],p_cmd->request->__cmd[7],
            p_cmd->request->__cmd[8],p_cmd->request->__cmd[9],p_cmd->request->__cmd[10],p_cmd->request->__cmd[11],
            p_cmd->request->__cmd[12],p_cmd->request->__cmd[13],p_cmd->request->__cmd[14],p_cmd->request->__cmd[15]);
        return TRUE;
    }

    return FALSE;
}

uint32 nvd_add_remap_request(struct scsi_cmnd *p_cmd, sdev_info_t *p_sdevice)
{
    ddb_remap_result *remap;
    uint64 max_lba = (uint64)((uint64)CAPACITY_1G_IN_SECTOR * 1000 * 10); //bigger than 10T
    uint64 new_lba;
    uint64 error_lba = blk_rq_pos(p_cmd->request);
    uint32 sectors = blk_rq_sectors(p_cmd->request);
        PT_INFO("Add remap %ld Device %d-%d-%d-%d Cmnd 0x%p CDB 0x%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x done.", 
            jiffies, p_cmd->device->host->host_no, p_cmd->device->id, p_cmd->device->lun, p_cmd->device->channel, p_cmd,
            p_cmd->request->__cmd[0], p_cmd->request->__cmd[1],p_cmd->request->__cmd[2],p_cmd->request->__cmd[3],
            p_cmd->request->__cmd[4],p_cmd->request->__cmd[5],p_cmd->request->__cmd[6],p_cmd->request->__cmd[7],
            p_cmd->request->__cmd[8],p_cmd->request->__cmd[9],p_cmd->request->__cmd[10],p_cmd->request->__cmd[11],
            p_cmd->request->__cmd[12],p_cmd->request->__cmd[13],p_cmd->request->__cmd[14],p_cmd->request->__cmd[15]); 

    if(error_lba > max_lba){ //bigger than 10T
        return 0;
    }

    if(p_sdevice == NULL) return 0;

    remap = &p_sdevice->remap;
    if(remap->no_of_remapped >= MAX_REMAP_PER_DISK){ //something wrong, cannot remap anymore
         PT_DEBUG_LIMIT("Remap full. Cannot take anymore. skip");
         return 0;
    }
    
    if(remap->no_of_remapped == 0){
        new_lba = remap->base_lba;
    }else{
        new_lba = remap->db.new_lba[remap->no_of_remapped - 1] + remap->db.sectors[remap->no_of_remapped -1];
    }
    remap->db.original_lba[remap->no_of_remapped] = error_lba;
    remap->db.new_lba[remap->no_of_remapped] = new_lba;
    remap->db.sectors[remap->no_of_remapped] = sectors;
    PT_DEBUG_LIMIT("%d-%d-%d-%d Remap bad sector: index[%d], sectors[%d], orig[%llx], new[%llx]",
              p_sdevice->host_no, p_sdevice->p_sdev->id, p_sdevice->p_sdev->lun, p_sdevice->p_sdev->channel,
              remap->no_of_remapped, sectors, error_lba, new_lba);

    remap->no_of_remapped ++;
    remap->need_sync = 1;
    nvd_wakeup_sync();

    return remap->no_of_remapped;
}

//must be called from a thread
int nvd_remap_init_and_load(sdev_info_t *p_sdevice)
{
        ddb_remap_result *remap;

        //memset((void *)&p_sdevice->remap,0,sizeof(ddb_remap_result));
         
        remap = &p_sdevice->remap;
        nvd_remap_rw_disk(p_sdevice, DMA_FROM_DEVICE);

        if(remap->magic == DDB_REMAP_MAGIC){
            PT_DEBUG("%d-%d-%d-%d no_of_remapped %d. base_lba[%llx].use this one.",
                p_sdevice->host_no, p_sdevice->p_sdev->id, p_sdevice->p_sdev->lun, p_sdevice->p_sdev->channel,
                remap->no_of_remapped, remap->base_lba);
        }else{
            memset((void *)&p_sdevice->remap,0,sizeof(ddb_remap_result)); //we need to zero it again.
            remap->magic = DDB_REMAP_MAGIC;
            remap->base_lba = p_sdevice->cap_sectors - CAPACITY_1G_IN_SECTOR + CAPACITY_1M_IN_SECTOR;
    	    remap->need_sync = 1;
    	    nvd_wakeup_sync();
            PT_DEBUG("Init remap meta info for %d-%d-%d-%d, base_lba[%llx]",
                p_sdevice->host_no, p_sdevice->p_sdev->id, p_sdevice->p_sdev->lun, p_sdevice->p_sdev->channel, remap->base_lba);
        }

        if(remap->no_of_remapped > MAX_REMAP_PER_DISK){ //something wrong, cannot remap anymore
            ASSERT(FALSE);
            remap->no_of_remapped = MAX_REMAP_PER_DISK; 
        }

        return remap->no_of_remapped;

}

void nvd_remap_check_and_sync(void)
{
	uint32 i;
        uint32 ret;
        sdev_info_t *p_sdevice;
        ddb_remap_result *remap;

	for(i = 0;i <DEV_NUM_MAX ;i++){
		p_sdevice = &G_dev_pool[i];
		if(p_sdevice->cap_sectors == 0) continue;
        
        remap = &p_sdevice->remap;
		if((remap->need_sync == 0) || (remap->sync_failed >= DEV_REMAP_SYNC_FAIL_MAX)){
			continue;
        }

        ret = nvd_remap_rw_disk(p_sdevice,DMA_TO_DEVICE);
        if(ret == NVD_RET_SUCC){
            PT_DEBUG_LIMIT("%d-%d-%d-%d remap db sync done. no_of_remapped %d.",
                p_sdevice->host_no, p_sdevice->p_sdev->id, p_sdevice->p_sdev->lun, p_sdevice->p_sdev->channel,remap->no_of_remapped);
            remap->need_sync = 0;
        }else{
            remap->sync_failed++;
            PT_DEBUG_LIMIT("%d-%d-%d-%d remap db sync failed.no_of_mapped %d",
                p_sdevice->host_no, p_sdevice->p_sdev->id, p_sdevice->p_sdev->lun, p_sdevice->p_sdev->channel,remap->no_of_remapped);
        }

	}

}

static struct task_struct *remap_thread = NULL;

int nvd_remap_handler(void *arg)
{
    do{
            set_current_state(TASK_UNINTERRUPTIBLE); 

            nvd_remap_check_and_sync();
            schedule_timeout(msecs_to_jiffies(60*1000)); 
            PT_DEBUG("remap handler run.");

    }while(!kthread_should_stop());
    PT_DEBUG("remap handler exit.");
    return 0;
}

void nvd_wakeup_sync(void)
{
    if(remap_thread) wake_up_process(remap_thread);
}

void nvd_init_thread(void)
{
    remap_thread = kthread_run(nvd_remap_handler,NULL,"nvd_remap");
}

void nvd_thread_clean(void)
{
    if(remap_thread) kthread_stop(remap_thread);
}
