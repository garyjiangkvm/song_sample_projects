#include "nvdisk.h"

#define GET_ARRAY_LEN(array) (sizeof(array) / sizeof(array[0])) 


host_info_t G_host_info[] = {
    {"ahci_platform", 8, 16, 8},
    //{"ata_piix", 8, 16, 8},    
};


sdev_info_t G_dev_pool[DEV_NUM_MAX];
uint32 G_dev_bits = 0;
void * Gp_resv_dev = NULL;

struct task_struct    *Gp_task = NULL;
struct scsi_device *Gp_device = NULL;
int g_wakeup = 0;
int g_restart = 0;
int g_limited = FALSE;
#define HOST_CMD_FAIL_MAX 15
#define DEV_CMD_FAIL_MAX  50
enum{
    HOST_ACTION_CONFIG,
    HOST_ACTION_DESTROY,
    HOST_ACTION_MAX,
};

static uint8 host_type_get_save(char *p_hname, int len, uint8 *p_type)
{
    uint8 index = 0;
    uint8 index_max = GET_ARRAY_LEN(G_host_info);

    if (len > HOST_TYPE_STRLEN_MAX){
        PT_ERR("Name len %d, but expect less than %d.\n", len, HOST_TYPE_STRLEN_MAX);
        return NVD_RET_INT_ERR;
    }

    for (index=0; index<index_max; index++){
        if ((strlen(G_host_info[index].name) != 0) && (strncmp(G_host_info[index].name, p_hname, len) == 0)){
            *p_type = index;
            return NVD_RET_SUCC;
        }        
    }
    
    return NVD_RET_HTYPE_INVALID;
}



static uint8 dev_send_read_cap(struct scsi_device *p_sdev, void *p_rsp)
{    
    uint8 cdb[16] = {0};
    uint8 ret = NVD_RET_SUCC;

    cdb[0] = 0x25; // READ_CAPACITY

    ret = scsi_execute(p_sdev, cdb, DMA_FROM_DEVICE,
            (uint8 *)p_rsp, 
            8,
            NULL,
            6 * HZ,
            3, 
            0, 
            NULL);

    if(ret != 0){
        PT_ERR("ReadCapacity failed!");
        return NVD_RET_INT_ERR;
    }
    return NVD_RET_SUCC;
}

static uint8 dev_send_read_cap_cmd_16(struct scsi_device *p_sdev, void *p_rsp, int len)
{   
	uint8 cdb[16] = {0};
	uint8 ret = NVD_RET_SUCC;    

	cdb[0] = 0x9e; //READ_CAPACITY16;
	cdb[1] = 0x10;
	cdb[13] = len;

	ret = scsi_execute(p_sdev, cdb, DMA_FROM_DEVICE,
			(uint8 *)p_rsp,
			len,
			NULL,
			6 * HZ,
			3, 
			0, 
			NULL);

	if(ret != 0){
		PT_ERR("ReadCapacity failed!");
		return NVD_RET_INT_ERR;
	}    
	return NVD_RET_SUCC;
}

static uint8 dev_send_inq(struct scsi_device *sdev, uint8 *p_rsp, int len)
{
    uint8 cdb[16] = {0};
    uint32 result;

    cdb[0] = 0x12;
    cdb[1] = 0x1;
    cdb[2] = 0x80;
    cdb[4] = len;

    result = scsi_execute(sdev, cdb, DMA_FROM_DEVICE,
            p_rsp, 
            len,
            NULL,
            6 * HZ,
			3, 
			0,
            NULL);

    if(result != 0){
		PT_ERR("Inquiry failed!");
		return NVD_RET_INT_ERR;
	}    
	return NVD_RET_SUCC;
}



/* FIXME: default sector size is 512B. */
static uint8 dev_get_cap(struct scsi_device *p_sdev, uint64 *p_sectors)
{
    uint8 resp[16];
	uint32 sector_size=0;
	uint32 old_sector =0;
    
    uint32 *tmp_32;
	uint32 lba_32;
    uint64 *tmp_64;
	uint64 lba_64;
    
    uint8 ret = NVD_RET_SUCC; 	
    
	/*here we just use READ_CAPACITY_16 can manager it*/
	ret = dev_send_read_cap( p_sdev, (void*)resp);
	if(ret == NVD_RET_SUCC){		
		tmp_32 = (uint32 *)resp;
		old_sector = be32_to_cpu(tmp_32[1]);
		lba_32 =  be32_to_cpu(tmp_32[0]);
		*p_sectors = lba_32;
		PT_DEBUG("lba_32 = 0x%x, capacity_32 =0x%llx",lba_32, *p_sectors);
	}

	ret = dev_send_read_cap_cmd_16( p_sdev, (void*)resp, 16);
	if(ret == NVD_RET_SUCC){		
		tmp_64 = (uint64 *)resp;
		sector_size = be64_to_cpu(tmp_64[1]);
		lba_64 = be64_to_cpu(tmp_64[0]);
		if(sector_size == 0)
			sector_size = old_sector;
		*p_sectors = lba_64;
		PT_DEBUG("lba_64 = 0x%llx, capacity_64 =0x%llx",lba_64, *p_sectors);
	}
	
	return ret;
}


static uint8 dev_cap_blocks( struct scsi_device *p_sdev, uint64 *p_sectors)
{
    uint8 retries;    
    uint8 ret = NVD_RET_SUCC; 

    retries = 8;

    while ( retries ) {
		ret = dev_get_cap(p_sdev, p_sectors);
		if(ret == NVD_RET_SUCC)
			break;

		retries--;
    }
    
    return ret;
}

#define INQUIRY_RSP_LEN 74
static uint8 dev_sn(struct scsi_device *p_sdev, uint8 *p_sn, int len)
{
    uint8 resp[INQUIRY_RSP_LEN] = {0};
    uint8 page_len = 0;
    uint8 ret = NVD_RET_SUCC;

    ret = dev_send_inq(p_sdev, resp, INQUIRY_RSP_LEN);
    if(ret != NVD_RET_SUCC){
        return ret;
    }

    page_len = resp[3];
    if (page_len>len){
        PT_ERR("Dev sn is too long %d.\n", page_len);
    }
    memcpy(p_sn, resp+4, page_len);
    return NVD_RET_SUCC;
}

static uint8 scsi_dev_add(struct scsi_device *p_sdev, int type, int host_no)
{
    sdev_info_t *p_sdevice = NULL;
    uint8 sn[DISK_SN_LEN_MAX] = {0};
    uint64 sectors = 0;
    uint64 cap_gb = 0; 
    uint8 index = 0;
    uint8 action = HOST_ACTION_CONFIG;
    uint8 ret = NVD_RET_SUCC;

    /* FIXME: exclude system disk. */
    ret = dev_cap_blocks(p_sdev, &sectors);
    if (ret != NVD_RET_SUCC){
        PT_ERR("Disk %d-%d-%d-%d get capacity failed, ret %d\n", host_no, p_sdev->channel, p_sdev->id, p_sdev->lun,ret);
    }

    ret = dev_sn(p_sdev, sn, DISK_SN_LEN_MAX);    
    if (ret != NVD_RET_SUCC){
        PT_ERR("Disk %d-%d-%d-%d get sn failed, ret %d\n", host_no, p_sdev->channel, p_sdev->id, p_sdev->lun,ret);
        strcpy(sn, "unknown");
    }     
    
    cap_gb = (p_sdev->sector_size)*sectors/(1024*1024*1024);    
    PT_DEBUG("scsi get sdev capacity [%lld G] sectorsize %d vendor [%s], model [%s], rev [%s] type %d min_cap %dG\n",
        cap_gb, p_sdev->sector_size, p_sdev->vendor,	p_sdev->model, p_sdev->rev, type, G_host_info[type].min_cap);

    PT_INFO("Add host %d-%d-%d-%d Get cap %lld G sn %s.\n", host_no, p_sdev->channel, p_sdev->id, p_sdev->lun, cap_gb, sn);    
#if 0 
    if( cap_gb <= G_host_info[type].min_cap){ /* filter the small capcity storage(e.g. u disk) out */
		PT_DEBUG("only %lld G, smaller than %d G. Could be a boot disk. Continue...", cap_gb, G_host_info[type].min_cap);
        Gp_resv_dev = (void*)p_sdev;
		return ret;
	}
#endif
    /* FIXME: need lock ?*/
    for (index=0; index<DEV_NUM_MAX; index++){
        if ((G_dev_pool[index].in_used == 0) &&(G_dev_pool[index].p_sdev == NULL)){
            break;
        }
    }
    if (index==DEV_NUM_MAX){
        PT_ERR("Device space is not enough.");
        return NVD_RET_DEVICE_NOSPACE; 
    }
    PT_INFO("Add host %d-%d-%d-%d index %d\n", host_no, p_sdev->channel, p_sdev->id, p_sdev->lun, index);  
    
    p_sdevice = &(G_dev_pool[index]);
    memset((void*)p_sdevice, 0, sizeof(sdev_info_t));
    p_sdevice->index = index;
	p_sdevice->cap_sectors = (p_sdev->sector_size)*sectors/512;
	p_sdevice->p_sdev = p_sdev;
    p_sdevice->host_no = host_no;
    p_sdevice->channel = p_sdev->channel;
    p_sdevice->id = p_sdev->id;
    p_sdevice->lun = p_sdev->lun;
    p_sdevice->status = DEV_GOOD;
    memcpy(p_sdevice->sn, sn, DISK_SN_LEN_MAX);
    p_sdevice->in_used = 1;
    
    nvd_remap_init_and_load(p_sdevice);
    netlink_enq(p_sdevice, MSG_TYPE_DISK_ONOFF, action);
    return ret;
}


sdev_info_t *scsi_dev_find_by_id(uint8 host, uint8 channel, uint8 id, uint8 lun)
{
    sdev_info_t *p_dev = NULL;
    uint8 i = 0;

    for(i=0; i<DEV_NUM_MAX; i++){
        p_dev = &(G_dev_pool[i]);
        if ((p_dev->cap_sectors) && (p_dev->host_no == host)&& (p_dev->p_sdev->channel== channel)
            && (p_dev->p_sdev->id== id)&& (p_dev->p_sdev->lun== lun)){
            return p_dev;
        }
    }
    return NULL;

}

static sdev_info_t *scsi_dev_find(struct scsi_device *p_sdev)
{
    sdev_info_t *p_dev = NULL;
    uint8 i = 0;

    for(i=0; i<DEV_NUM_MAX; i++){
        p_dev = &(G_dev_pool[i]);
        if ((p_dev->in_used) && (p_dev->p_sdev == p_sdev)){
            return p_dev;
        }
    }
    return NULL;
}


int host_slave_common(struct scsi_device *p_sdev, uint8 action)
{
    host_info_t *p_hinfo;
    sdev_info_t *p_dev = NULL;
	struct Scsi_Host *p_shost = p_sdev->host;
	uint8 type = 0;
    uint8 ret = NVD_RET_SUCC;

    ret = host_type_get_save((char *)p_shost->hostt->name, strlen((char *)p_shost->hostt->name), &type);
    if(ret != NVD_RET_SUCC){            
		PT_ERR("Can't be here %s\n",(char *)p_shost->hostt->name);
        return 0;
    }

    p_hinfo = &(G_host_info[type]);
    if (action == HOST_ACTION_CONFIG){
        PT_INFO("Device Online %d-%d-%d-%d.\n", p_shost->host_no, p_sdev->channel, p_sdev->id, p_sdev->lun);       
        if (p_hinfo->old_slave_configure){
            ret = p_hinfo->old_slave_configure(p_sdev);
        }

        PT_DEBUG("Device index %d.\n", index);
        ret = scsi_dev_add(p_sdev, type, p_shost->host_no);
        if(ret != NVD_RET_SUCC){            
    		PT_DEBUG("add %s device failed",(char *)p_shost->hostt->name);
        } 
         
    } else {
        PT_INFO(" Device Offline.\n");   
          
        p_dev = scsi_dev_find(p_sdev);        
        if (p_dev){
            PT_INFO("Device Offline %d-%d-%d-%d sn %s.\n", p_dev->host_no, p_dev->channel, p_dev->id, p_dev->lun, p_dev->sn);
            netlink_enq(p_dev, MSG_TYPE_DISK_ONOFF, action);   
            p_dev->p_sdev = NULL;
            p_dev->in_used = 0;            
            PT_INFO("Put host %d-%d-%d-%d index %d\n", p_dev->host_no, p_dev->channel, p_dev->id, p_dev->lun, p_dev->index);  
        }
        if (p_hinfo->old_slave_destroy){
            p_hinfo->old_slave_destroy(p_sdev);
        }

    } 

    return ret;
}


static void host_slave_destroy(struct scsi_device *p_sdev)
{
    host_slave_common(p_sdev, HOST_ACTION_DESTROY);
}

static int host_slave_configure(struct scsi_device *p_sdev)
{
    return host_slave_common(p_sdev, HOST_ACTION_CONFIG);
}

static void cmd_change_cap(struct scsi_cmnd *p_cmd)
{
        struct sg_mapping_iter miter;
        unsigned long flags;
        uint32 *p_u32, cap32;
        uint64 *p_u64, cap64;
        struct scsi_data_buffer *sdb = scsi_in(p_cmd);
        unsigned int sg_flags = SG_MITER_ATOMIC;


        sg_flags |= SG_MITER_TO_SG;
        sg_miter_start(&miter, sdb->table.sgl, sdb->table.nents, sg_flags);
        
        local_irq_save(flags);  
	if(sg_miter_next(&miter)){ 
		//PT_DEBUG("nents %d, addr %p, len %d\n",sdb->table.nents, miter.addr,(int)miter.length);
		//dStrHex(miter.addr, miter.length, 0, 0);

		if(p_cmd->cmnd[0] == 0x25){
			p_u32 = (uint32 *)miter.addr;
			cap32 = be32_to_cpu(*p_u32);
			cap32 -= (CAPACITY_1G_IN_SECTOR + 8);
			*p_u32 = cpu_to_be32(cap32);
		}

		if(p_cmd->cmnd[0] == 0x9e){
			p_u64 = (uint64 *)miter.addr;
			cap64 = be64_to_cpu(*p_u64);
			cap64 -= (CAPACITY_1G_IN_SECTOR + 8);
			*p_u64 = cpu_to_be64(cap64);
		}
		//dStrHex(miter.addr, miter.length, 0, 0);

	}
        sg_miter_stop(&miter);

        local_irq_restore(flags);

        return;
}

static void nvd_cmd_done_check_result(struct scsi_cmnd *p_cmd)
{
    struct scsi_sense_hdr sshdr;
    sdev_info_t *p_sdevice;

    if(p_cmd->request == NULL || p_cmd == NULL){
        PT_DEBUG("rq or cmd is null");
        return;
    }
    p_sdevice = scsi_dev_find(p_cmd->device);
    if (!p_sdevice){
        return;
    }

    //check result first
    switch (host_byte(p_cmd->result)){
        case DID_BAD_TARGET : 
            break;

        case DID_NO_CONNECT :
            break;
            
        case DID_BUS_BUSY:
            break;

        case DID_TIME_OUT:
            break;

        case DID_RESET:
            break;

        case DID_TRANSPORT_DISRUPTED:
            break;

        case DID_OK:
    	    if (scsi_command_normalize_sense(p_cmd, &sshdr)){
                PT_INFO("disk request done with err [%x] key[%x] asc[%x] ascq[%x]",
    			    sshdr.response_code,
    			    sshdr.sense_key,
    			    sshdr.asc,
    			    sshdr.ascq);
               
                if(sshdr.sense_key == 0x03 && sshdr.asc == 0x11 && sshdr.ascq == 0x04){
                    nvd_add_remap_request(p_cmd, p_sdevice);
                    netlink_enq(p_sdevice, MSG_TYPE_REMAP_EVENT, HOST_ACTION_MAX);
                }

    	    }
    	    break;

        default:
            break;
        }
    
        
        if(p_sdevice != NULL){
            p_sdevice->err_count[host_byte(p_cmd->result)] ++;
        }

        PT_DEBUG("disk [%s] request done with cmnd %p cdb 0x%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            p_cmd->request->rq_disk->disk_name,p_cmd, 
            p_cmd->request->__cmd[0], p_cmd->request->__cmd[1],p_cmd->request->__cmd[2],p_cmd->request->__cmd[3],
            p_cmd->request->__cmd[4],p_cmd->request->__cmd[5],p_cmd->request->__cmd[6],p_cmd->request->__cmd[7],
            p_cmd->request->__cmd[8],p_cmd->request->__cmd[9],p_cmd->request->__cmd[10],p_cmd->request->__cmd[11],
            p_cmd->request->__cmd[12],p_cmd->request->__cmd[13],p_cmd->request->__cmd[14],p_cmd->request->__cmd[15]);
}

void nvd_bio_end_io(struct bio *bio, int error)
{
    struct scsi_cmnd *p_cmd; 
    struct request *rq = (struct request *) bio->bi_private;

    if(rq == NULL) return;
    p_cmd = (struct scsi_cmnd *)rq->special;
    if(p_cmd == NULL) return;

    bio->bi_end_io = (bio_end_io_t*)p_cmd->SCp.ptr;
    bio->bi_private = (void *)(p_cmd->SCp.buffer);

    nvd_cmd_done_check_result(p_cmd);
    PT_DEBUG("Done: Device %d-%d-%d bi_end_io %p private %p cmnd 0x%p cdb 0x%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
        p_cmd->device->id, p_cmd->device->lun, p_cmd->device->channel, 
        bio->bi_end_io, bio->bi_private, p_cmd,
        p_cmd->request->__cmd[0], p_cmd->request->__cmd[1],p_cmd->request->__cmd[2],p_cmd->request->__cmd[3],
        p_cmd->request->__cmd[4],p_cmd->request->__cmd[5],p_cmd->request->__cmd[6],p_cmd->request->__cmd[7],
        p_cmd->request->__cmd[8],p_cmd->request->__cmd[9],p_cmd->request->__cmd[10],p_cmd->request->__cmd[11],
        p_cmd->request->__cmd[12],p_cmd->request->__cmd[13],p_cmd->request->__cmd[14],p_cmd->request->__cmd[15]);

    if (bio->bi_end_io)
          bio->bi_end_io(bio, error);
}

static void host_cmd_scsi_done(struct scsi_cmnd *p_cmd)
{
    host_info_t *p_hinfo;
    uint8 type = 0;
    uint8 ret = NVD_RET_SUCC; 

    ret = host_type_get_save((char *)p_cmd->device->host->hostt->name, strlen((char *)p_cmd->device->host->hostt->name), &type);
    if(ret != NVD_RET_SUCC){            
		PT_ERR("Can't be here %s\n",(char *)p_cmd->device->host->hostt->name);
        return;
    }    

    p_hinfo = &(G_host_info[type]);
    if (p_cmd->device != Gp_resv_dev){
        PT_DEBUG("%ld Device %d-%d-%d-%d Done IO %d-%d-%d Cmnd 0x%p Result 0x%x CDB 0x%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x done.", 
            jiffies, p_cmd->device->host->host_no, p_cmd->device->channel, p_cmd->device->id, p_cmd->device->lun, 
            p_cmd->device->iorequest_cnt.counter, p_cmd->device->iodone_cnt.counter, p_cmd->device->ioerr_cnt.counter, p_cmd, p_cmd->result,
            p_cmd->request->__cmd[0], p_cmd->request->__cmd[1],p_cmd->request->__cmd[2],p_cmd->request->__cmd[3],
            p_cmd->request->__cmd[4],p_cmd->request->__cmd[5],p_cmd->request->__cmd[6],p_cmd->request->__cmd[7],
            p_cmd->request->__cmd[8],p_cmd->request->__cmd[9],p_cmd->request->__cmd[10],p_cmd->request->__cmd[11],
            p_cmd->request->__cmd[12],p_cmd->request->__cmd[13],p_cmd->request->__cmd[14],p_cmd->request->__cmd[15]); 

        if(p_cmd->cmnd[0] == 0x25 || p_cmd->cmnd[0] == 0x9e) cmd_change_cap(p_cmd);
    }
    
    p_hinfo->old_scsi_done(p_cmd);    
}

static int host_queuecommand(struct Scsi_Host *p_shost, struct scsi_cmnd *p_cmd)
{
    sdev_info_t *p_sdevice;
    uint8 type = 0;
    uint8 ret = NVD_RET_SUCC;  

    ret = host_type_get_save((char *)p_shost->hostt->name, strlen((char *)p_shost->hostt->name), &type);
    if(ret != NVD_RET_SUCC){            
		PT_ERR("Can't be here %s\n",(char *)p_shost->hostt->name);
        return 0;
    }    

    if (p_cmd->device != Gp_resv_dev){
        if(p_cmd->cmnd[0] == 0x25 || p_cmd->cmnd[0] == 0x9e){
            if ((G_host_info[type].old_scsi_done)&& (G_host_info[type].old_scsi_done != p_cmd->scsi_done)){
                PT_ERR("func scsi done changed!");
            } else {
                G_host_info[type].old_scsi_done = p_cmd->scsi_done;
                p_cmd->scsi_done = host_cmd_scsi_done;
            }
        } else if (p_cmd->cmnd[0] == 0x28 || p_cmd->cmnd[0] == 0x2a || p_cmd->cmnd[0] == 0x88  || p_cmd->cmnd[0] == 0x8a){

            PT_DEBUG("%ld Device %d-%d-%d-%d Queue Hfailed %d Hbusy %d Allowed %d Cmnd 0x%p CDB 0x%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x timeout %d retry %d.\n", 
                jiffies, p_shost->host_no, p_cmd->device->channel, p_cmd->device->id, p_cmd->device->lun,  
                p_shost->host_failed, p_shost->host_busy, p_cmd->allowed, p_cmd,
                p_cmd->request->__cmd[0], p_cmd->request->__cmd[1],p_cmd->request->__cmd[2],p_cmd->request->__cmd[3],
                p_cmd->request->__cmd[4],p_cmd->request->__cmd[5],p_cmd->request->__cmd[6],p_cmd->request->__cmd[7],
                p_cmd->request->__cmd[8],p_cmd->request->__cmd[9],p_cmd->request->__cmd[10],p_cmd->request->__cmd[11],
                p_cmd->request->__cmd[12],p_cmd->request->__cmd[13],p_cmd->request->__cmd[14],p_cmd->request->__cmd[15],
                p_cmd->request->timeout, p_cmd->request->retries);

            if(p_cmd->request && p_cmd->request->bio){
                
                PT_DEBUG("Queue: Device %d-%d-%d-%d bi_end_io %p, private %p Cmnd 0x%p CDB 0x%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                    p_shost->host_no, p_cmd->device->channel, p_cmd->device->id, p_cmd->device->lun,  
                    p_cmd->request->bio->bi_end_io, p_cmd->request->bio->bi_private, p_cmd,
                    p_cmd->request->__cmd[0], p_cmd->request->__cmd[1],p_cmd->request->__cmd[2],p_cmd->request->__cmd[3],
                    p_cmd->request->__cmd[4],p_cmd->request->__cmd[5],p_cmd->request->__cmd[6],p_cmd->request->__cmd[7],
                    p_cmd->request->__cmd[8],p_cmd->request->__cmd[9],p_cmd->request->__cmd[10],p_cmd->request->__cmd[11],
                    p_cmd->request->__cmd[12],p_cmd->request->__cmd[13],p_cmd->request->__cmd[14],p_cmd->request->__cmd[15]);

                // this command not be replaced.
                if (p_cmd->request->bio->bi_end_io != nvd_bio_end_io){
                    p_cmd->allowed = 3;
                    p_cmd->request->retries = 3;
                    p_cmd->request->timeout = (p_cmd->request->timeout)/2;
                    p_cmd->SCp.ptr = (void *)p_cmd->request->bio->bi_end_io;
                    p_cmd->SCp.buffer = ( struct scatterlist *)p_cmd->request->bio->bi_private;
                    p_cmd->request->bio->bi_end_io = nvd_bio_end_io;
                    p_cmd->request->bio->bi_private = (void*)p_cmd->request;
                    PT_DEBUG("New bi_end_io %p, private %p\n", p_cmd->request->bio->bi_end_io, p_cmd->request->bio->bi_private);
                    //find remap lba
                	p_sdevice = scsi_dev_find(p_cmd->device);
                	if(p_sdevice != NULL){
                        nvd_find_remapped_lba(p_sdevice,p_cmd);
                	} 
                }
                               
            }            
        }
    }
  
    ret = G_host_info[type].old_queue_command(p_shost, p_cmd);
    if (ret != NVD_RET_SUCC){
        PT_DEBUG("\n!!!!!!!!!!!!!!Qcmnd ret 0x%x.\n", ret);
    }
    return ret;
}

static void host_eh_strategy_handler(struct Scsi_Host *p_shost)
{
    host_info_t *p_hinfo;
    struct scsi_cmnd *p_cmd = NULL;
    uint8 type = 0;
    uint8 ret = NVD_RET_SUCC;

    ret = host_type_get_save((char *)p_shost->hostt->name, strlen((char *)p_shost->hostt->name), &type);
    if(ret != NVD_RET_SUCC){            
		PT_ERR("Can't be here %s\n",(char *)p_shost->hostt->name);
        return;
    }    
    
    PT_DEBUG("%ld H %d EH Hstate %d Hfailed %d Hbusy %d eh strategy handler.\n", 
        jiffies, p_shost->host_no, p_shost->shost_state, p_shost->host_failed, p_shost->host_busy);
    list_for_each_entry(p_cmd, &(p_shost->eh_cmd_q), eh_entry){
        PT_DEBUG("EhHandler DIO %d-%d-%d Cmnd 0x%p Result 0x%x CDB 0x%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n", 
            p_cmd->device->iorequest_cnt.counter, p_cmd->device->iodone_cnt.counter, p_cmd->device->ioerr_cnt.counter, p_cmd, p_cmd->result,
            p_cmd->request->__cmd[0], p_cmd->request->__cmd[1],p_cmd->request->__cmd[2],p_cmd->request->__cmd[3],
            p_cmd->request->__cmd[4],p_cmd->request->__cmd[5],p_cmd->request->__cmd[6],p_cmd->request->__cmd[7],
            p_cmd->request->__cmd[8],p_cmd->request->__cmd[9],p_cmd->request->__cmd[10],p_cmd->request->__cmd[11],
            p_cmd->request->__cmd[12],p_cmd->request->__cmd[13],p_cmd->request->__cmd[14],p_cmd->request->__cmd[15]);
    }
    
    p_hinfo = &(G_host_info[type]);
    p_hinfo->old_eh_handle(p_shost);
}

void host_recovery(struct Scsi_Host *shost)
{
    unsigned long flags;
    PT_DEBUG("host recovery...\n");
    spin_lock_irqsave(shost->host_lock, flags);
	if (shost->host_eh_scheduled)
		if (scsi_host_set_state(shost, SHOST_RECOVERY))
			WARN_ON(scsi_host_set_state(shost, SHOST_CANCEL_RECOVERY));
	spin_unlock_irqrestore(shost->host_lock, flags);
}

#if 0
enum blk_eh_timer_return host_scsi_timed_out(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *host = cmd->device->host;
	struct ata_port *ap = ata_shost_to_port(host);
	struct scsi_sense_hdr sshdr;
	struct ata_queued_cmd *qc;
	enum blk_eh_timer_return ret;

    int sense_valid = 0, sense_deferred = 0;
    unsigned long flags;
    
	PT_DEBUG("ENTER\n");

    //cmd->result = (DRIVER_SENSE << 24) | SAM_STAT_CHECK_CONDITION;
    //scsi_build_sense_buffer(0, cmd->sense_buffer, HARDWARE_ERROR, 0x5d, 0x13);

    sense_valid = scsi_command_normalize_sense(cmd, &sshdr);
	if (sense_valid)
		sense_deferred = scsi_sense_is_deferred(&sshdr);

    PT_DEBUG("sense valid %d sense_defered 0x%x key 0x%x\n", sense_valid, sense_deferred, sshdr.sense_key);

	if (ap->ops->error_handler) {
        PT_DEBUG("Eh not handled change to eh_handled.\n");
		ret = BLK_EH_HANDLED;
		goto out;
	}

	ret = BLK_EH_HANDLED;
	spin_lock_irqsave(ap->lock, flags);
	qc = ata_qc_from_tag(ap, ap->link.active_tag);
	if (qc) {
		WARN_ON(qc->scsicmd != cmd);
		qc->flags |= ATA_QCFLAG_EH_SCHEDULED;
		//qc->err_mask |= AC_ERR_TIMEOUT;
		ret = BLK_EH_HANDLED;       
	}
	spin_unlock_irqrestore(ap->lock, flags);
   
 out:    
	PT_DEBUG("EXIT, ret=%d\n", ret);
	return ret;
}
#endif

static enum blk_eh_timer_return host_eh_timeout(struct scsi_cmnd *p_cmd)
{
    host_info_t *p_hinfo;
    struct Scsi_Host *p_shost = p_cmd->device->host;
    sdev_info_t *p_dev = NULL;
    uint8 type = 0;
    uint64 curr_prc_cnt = 0;
    uint8 ret = NVD_RET_SUCC;    

    ret = host_type_get_save((char *)p_shost->hostt->name, strlen((char *)p_shost->hostt->name), &type);
    if(ret != NVD_RET_SUCC){            
        PT_ERR("Can't be here %s\n",(char *)p_shost->hostt->name);
        return 0;
    }    

    p_hinfo = &(G_host_info[type]);

    if (p_cmd->device != Gp_resv_dev){
        PT_DEBUG("%ld Timeout Device %d-%d-%d-%d io %d-%d-%d Dbusy %d Qdpt %d Hfailed %d Hbusy %d Cmnd 0x%p Result 0x%x allowed %d CDB 0x%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x timeout %d retry %d.\n", 
            jiffies, p_shost->host_no, p_cmd->device->channel, p_cmd->device->id, p_cmd->device->lun, 
            atomic_read(&p_cmd->device->iorequest_cnt), atomic_read(&p_cmd->device->iodone_cnt), atomic_read(&p_cmd->device->ioerr_cnt), p_cmd->device->device_busy, p_cmd->device->queue_depth,
            p_shost->host_failed, p_shost->host_busy, p_cmd, p_cmd->result, p_cmd->allowed,  
            p_cmd->request->__cmd[0], p_cmd->request->__cmd[1],p_cmd->request->__cmd[2],p_cmd->request->__cmd[3],
            p_cmd->request->__cmd[4],p_cmd->request->__cmd[5],p_cmd->request->__cmd[6],p_cmd->request->__cmd[7],
            p_cmd->request->__cmd[8],p_cmd->request->__cmd[9],p_cmd->request->__cmd[10],p_cmd->request->__cmd[11],
            p_cmd->request->__cmd[12],p_cmd->request->__cmd[13],p_cmd->request->__cmd[14],p_cmd->request->__cmd[15],
            p_cmd->request->timeout, p_cmd->request->retries);
    
    
        //PT_DEBUG("======eh timeout %d\n", p_hinfo->old_timeout(p_cmd));
        //ret = BLK_EH_HANDLED; //host_scsi_timed_out(p_cmd);        
        PT_DEBUG("RESET timeout ret %d.\n", ret);

        p_dev = scsi_dev_find(p_cmd->device);
        if (p_dev){
            if (p_dev->status != DEV_FAIL){
                p_dev->status = DEV_ERR;
            }
            p_dev->total_tm_cnt++;   
            printk("Device %d-%d-%d-%d io %d-%d-%d timeout %lld.\n", 
                p_shost->host_no, p_cmd->device->channel, p_cmd->device->id, p_cmd->device->lun, 
                atomic_read(&p_cmd->device->iorequest_cnt), atomic_read(&p_cmd->device->iodone_cnt), atomic_read(&p_cmd->device->ioerr_cnt),
                p_dev->total_tm_cnt);
        

 #ifdef DEV_REMOVE_IN_KERNEL      
            if ((p_dev->total_tm_cnt >= DEV_CMD_FAIL_MAX) && (g_wakeup == 0)){
                PT_DEBUG("\n************Remove device and wakeup process.\n");          
                Gp_device = p_cmd->device;   
                scsi_device_set_state(Gp_device, SDEV_OFFLINE); 
                wake_up_process(Gp_task);
                g_wakeup++;        
            }
 #else            
            curr_prc_cnt = atomic_read(&p_cmd->device->iorequest_cnt)-atomic_read(&p_cmd->device->iodone_cnt);
            if (curr_prc_cnt != p_dev->curr_prc_cnt){
                p_dev->curr_prc_cnt = curr_prc_cnt;
                p_dev->curr_tm_cnt = 0;
            } else {
                p_dev->curr_tm_cnt++;
            }

 
            if((p_dev->curr_tm_cnt >= HOST_CMD_FAIL_MAX)||(p_dev->total_tm_cnt >= DEV_CMD_FAIL_MAX)){                
                p_dev->status = DEV_FAIL;
                netlink_enq(p_dev, MSG_TYPE_DISK_STATE, HOST_ACTION_MAX);
            } else {
                netlink_enq(p_dev, MSG_TYPE_DISK_ERR, HOST_ACTION_MAX);
            }
 #endif
        }
    }

    return p_hinfo->old_timeout(p_cmd);
}


static int host_replace_funcs(struct Scsi_Host *p_shost, int type, int h_index)
{
    host_info_t *p_hinfo;
    int ret = NVD_RET_SUCC;    

    p_hinfo = &(G_host_info[type]);
    if (p_hinfo->replace_done){
        return ret;
    }

    p_hinfo->p_shost = p_shost;
    p_hinfo->old_slave_configure = p_shost->hostt->slave_configure;
    p_hinfo->old_slave_destroy = p_shost->hostt->slave_destroy;   
    p_hinfo->old_queue_command = p_shost->hostt->queuecommand;   
    p_hinfo->old_eh_handle = p_shost->transportt->eh_strategy_handler;
    p_hinfo->old_timeout = p_shost->transportt->eh_timed_out;   

    p_shost->hostt->slave_configure = host_slave_configure;
    p_shost->hostt->slave_destroy = host_slave_destroy;
    p_shost->hostt->queuecommand = host_queuecommand;
    //p_shost->transportt->eh_strategy_handler = host_eh_strategy_handler;
    p_shost->transportt->eh_timed_out = host_eh_timeout;
    p_hinfo->replace_done = 1;

    PT_DEBUG("htAb %p\n", p_shost->hostt->eh_abort_handler);
    PT_DEBUG("htBr %p\n",p_shost->hostt->eh_bus_reset_handler); 
    PT_DEBUG("htDr %p\n",p_shost->hostt->eh_device_reset_handler);
    PT_DEBUG("htHr %p\n",p_shost->hostt->eh_host_reset_handler);
    PT_DEBUG("htTr %p\n",p_shost->hostt->eh_target_reset_handler);
    PT_DEBUG("htTm %p\n",p_shost->hostt->eh_timed_out); 
    PT_DEBUG("hEh 0x%p\n",p_hinfo->old_eh_handle); 
    PT_DEBUG("ttTm 0x%p\n",p_shost->transportt->eh_timed_out);  
    PT_DEBUG("ttUs %p\n",p_shost->transportt->user_scan); 
    PT_DEBUG("cmd1L %d \n",p_shost->cmd_per_lun); 

    return ret;
}

static void host_recovery_funcs(void)
{
    host_info_t *p_hinfo;
    int index = 0;
    int index_max = GET_ARRAY_LEN(G_host_info);

    for (index=0; index<index_max; index++){
        p_hinfo = &(G_host_info[index]);
        if (p_hinfo->replace_done){
            p_hinfo->p_shost->hostt->slave_configure = p_hinfo->old_slave_configure;
            p_hinfo->p_shost->hostt->slave_destroy = p_hinfo->old_slave_destroy;
            p_hinfo->p_shost->hostt->queuecommand = p_hinfo->old_queue_command;
            p_hinfo->p_shost->transportt->eh_strategy_handler = p_hinfo->old_eh_handle;
            p_hinfo->p_shost->transportt->eh_timed_out = p_hinfo->old_timeout;
        }
    }
}

static uint8 scsi_dev_scan(void)
{
	struct scsi_device *p_sdev;  
	struct Scsi_Host *p_shost; 
	uint8 h_index = 0, type;
	uint8 ret = NVD_RET_SUCC;

	/* FIXME: replace with func next_device. */
	for(h_index = 0; h_index < SCAN_HOST_MAX; h_index++){
		p_shost = scsi_host_lookup(h_index);
		if(!p_shost){
			continue;
		}            

		ret = host_type_get_save((char *)p_shost->hostt->name, strlen((char *)p_shost->hostt->name), &type);
		if(ret != NVD_RET_SUCC){            
			PT_ERR("Shost %s is not supported",(char *)p_shost->hostt->name);
			ret = NVD_RET_SUCC;
			continue;
		}

        ret = host_replace_funcs(p_shost, type, h_index);
        if(ret != NVD_RET_SUCC){            
    		PT_DEBUG("Shost %s place failed",(char *)p_shost->hostt->name);
    	    break;
        }   
		PT_DEBUG("disk host %s type %d [ id %d, unique_id %d]...", 
				p_shost->hostt->name, type, h_index, p_shost->unique_id);

		shost_for_each_device(p_sdev, p_shost){
			if(p_sdev && (p_sdev->type == 0)){ //type is disk
				ret = scsi_dev_add(p_sdev, type, h_index);
				if (ret!= NVD_RET_SUCC){
                    PT_ERR("add host %s failed.",(char *)p_shost->hostt->name);
    	            return ret;
                }
			} /* sdev */
		} /* shost_for_each_device(sdev, shost) */        
	}

	return ret;
}

int nvd_scan_device(struct scsi_device *p_sdev)
{
    struct Scsi_Host *p_shost;

    if(p_sdev == NULL){
         return -1;
    }

    printk("rescan device.\n");

    p_shost = p_sdev->host;
    scsi_rescan_device(&p_sdev->sdev_gendev);
    //scsi_scan_target(&p_shost->shost_gendev,p_sdev->channel,p_sdev->id,p_sdev->lun,0);// channel&target will be 0. it's new device,so rescan is disabled
    return 0;
}

int scsi_monitor(void *data)
{    
    struct Scsi_Host *p_host = NULL;    
    uint8 channel = 0;
    uint8 target = 0;
    uint8 lun = 0;
    
	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		if ((Gp_device == NULL) || (g_restart == g_wakeup)) {
			schedule();
			continue;
		}

		__set_current_state(TASK_RUNNING);
		printk("%ld scsi monitor waking up\n", jiffies);

        p_host = Gp_device->host;
        channel = Gp_device->channel;
        target = Gp_device->id;
        lun = Gp_device->lun;
        printk("%ld ##########################################remove device state %d visible %d.############################\n", jiffies, Gp_device->sdev_state, Gp_device->is_visible);            
        scsi_remove_device(Gp_device);

        msleep(60*1000);
        printk("%ld ##########################################remove device status %d finish.############################\n", jiffies, Gp_device->sdev_state);
        printk("add device %d-%d-%d-%d.\n", p_host->host_no, channel, target, lun);
        if (p_host->transportt->user_scan)
            printk("add device return %d.\n", p_host->transportt->user_scan(p_host, channel, target, lun));
        Gp_device = NULL;
        g_restart++;
	}
	__set_current_state(TASK_RUNNING);

	return 0;
}

void limited_set(void)
{
    g_limited = TRUE;
    host_recovery_funcs();
    return;
}

int limited(void)
{
    return g_limited;
}

int nvdisk_init(void)
{
	uint8 ret = NVD_RET_SUCC;

    memset((void *)G_dev_pool,0,sizeof(sdev_info_t)*DEV_NUM_MAX);	

#ifdef DEV_REMOVE_IN_KERNEL
    PT_DEBUG("run thread scsi_monitor.\n");
    Gp_task = kthread_run(scsi_monitor, NULL,"scsi_monitor");
    if (Gp_task == NULL){
        return NVD_RET_INT_ERR;
    } 
#endif
    
    PT_DEBUG("init netlink.\n");
    if (netlink_init()!= 0){
        return NVD_RET_INT_ERR;
    }
    nvd_init_thread();
    nvd_proc_create(NULL);

    if (!limited()){
        ret = scsi_dev_scan();
    }
    return 0;
}

void nvdisk_exit(void)
{
    netlink_exit();
    nvd_thread_clean();
    nvd_proc_clean();
    host_recovery_funcs();
#ifdef DEV_REMOVE_IN_KERNEL
    if (Gp_task){
        kthread_stop(Gp_task);
    }
#endif
}
module_init(nvdisk_init);
module_exit(nvdisk_exit);

MODULE_LICENSE("GPL");
