#include <linux/list.h>
#include <linux/spinlock.h>

#include <linux/netlink.h>
#include <net/sock.h>

#include "nvdisk.h"

static LIST_HEAD(G_nl_msg_head);
static LIST_HEAD(G_free_msg_head);
DECLARE_COMPLETION(client_ready);

static struct sock *Gp_ntlk_sock = NULL;
spinlock_t   G_nl_lock;
spinlock_t   G_free_lock;
struct task_struct    *Gp_ntlk = NULL;

#define NODE_NUM_MAX   256


static netlink_msg_node_t * netlink_node_get(void)
{
    netlink_msg_node_t *p_node = NULL;
    unsigned long flag; 
    
    if (list_empty(&G_free_msg_head)){
        return NULL;
    }
    
    spin_lock_irqsave(&G_free_lock, flag);  
    p_node = list_entry(G_free_msg_head.next, netlink_msg_node_t, linked);
    list_del(G_free_msg_head.next);
    spin_unlock_irqrestore(&G_free_lock, flag);

    return p_node;
}


static void netlink_node_put(netlink_msg_node_t *p_node)
{
    unsigned long flag;
    spin_lock_irqsave(&G_free_lock, flag);     
    list_add_tail(&(p_node->linked), &G_free_msg_head);
    spin_unlock_irqrestore(&G_free_lock, flag);
}

static void dumpstring(uint8 *p_str, int len, char *name)
{
    int i=0;
    printk("name dump ===%s ===\n", name);
    for(i=0;i<len; i++){
        printk("%2x ", p_str[i]);
    }
    printk("\nname dump finish ===%s ===\n", name);
}

int netlink_enq(sdev_info_t *p_dev, uint32 type, uint8 onoff)
{    
    netlink_msg_node_t *p_node = NULL;
    unsigned long flag;

    if (type >= MSG_TYPE_MAX){
        PT_ERR("invalid type 0x%x\n", type);
        return NVD_RET_INT_ERR;
    }

    /*FIXME: Alloc node. */
    p_node = netlink_node_get();
    if (p_node == NULL){
        PT_ERR("NETLINK msg space is not enough.\n");
        return NVD_RET_INT_ERR;
    }

    p_node->msg.t_index = type;
    p_node->msg.no[0] = (uint8)(p_dev->host_no);
    p_node->msg.no[1] = (uint8)(p_dev->channel);
    p_node->msg.no[2] = (uint8)(p_dev->id);
    p_node->msg.no[3] = (uint8)(p_dev->lun); 
    memcpy(p_node->msg.sn, p_dev->sn, DISK_SN_LEN_MAX);
    if((type == MSG_TYPE_DISK_ERR) ||(type == MSG_TYPE_DISK_STATE)){               
        p_node->msg.details.io_info.status = (uint8)(p_dev->status);
        p_node->msg.details.io_info.tm_cnt = (uint8)(p_dev->total_tm_cnt);
        p_node->msg.details.io_info.err_cnt = (uint8)(p_dev->err_count[DID_BAD_TARGET]);
        p_node->msg.details.io_info.abt_cnt = (uint8)(p_dev->err_count[DID_ABORT]);
    } else if (type == MSG_TYPE_REMAP_EVENT){
        p_node->msg.details.remap.remap_cnt = p_dev->remap.no_of_remapped;
        p_node->msg.details.remap.sectors = p_dev->remap.db.sectors[p_dev->remap.no_of_remapped];
        p_node->msg.details.remap.org_lba = p_dev->remap.db.original_lba[p_dev->remap.no_of_remapped];
        p_node->msg.details.remap.new_lba = p_dev->remap.db.new_lba[p_dev->remap.no_of_remapped];
    } else if (type == MSG_TYPE_DISK_ONOFF){
        p_node->msg.details.onoff.state = onoff;        
    }else {
        PT_ERR("invalid type 0x%x\n", type);
        netlink_node_put(p_node);
        return NVD_RET_INT_ERR;            
    } 

    /*add node to the tail of list*/
    spin_lock_irqsave(&G_nl_lock, flag);        
    list_add_tail(&(p_node->linked), &G_nl_msg_head);
    spin_unlock_irqrestore(&G_nl_lock, flag);
    wake_up_process(Gp_ntlk);

    return NVD_RET_SUCC;
}


static int netlink_send_msg(nltk_msg_t *send_msg)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    uint32 len;

    PT_INFO("SendMsg tp 0x%x no %d-%d-%d-%d.\n", send_msg->t_index, send_msg->no[0], send_msg->no[1], send_msg->no[2], send_msg->no[3]);

    len = sizeof(*send_msg);   

    skb = alloc_skb(NLMSG_SPACE(len), GFP_KERNEL);
    if(skb == NULL){
        PT_ERR("netlink: alloc_skb Error.\n");
        return -1;
    }

    nlh = nlmsg_put(skb, 0, 0, 0, len, 0);
    if (nlh == NULL){
        PT_ERR("netlink: nlmsg_put Error.\n");
        return -1;
    }

    memset(NLMSG_DATA(nlh), 0, len);
    memcpy(NLMSG_DATA(nlh), (char *)send_msg, len);
    NETLINK_CB(skb).pid = 0;
    NETLINK_CB(skb).dst_group = NETLINK_GROUPS_MASK;

    netlink_broadcast(Gp_ntlk_sock, skb, 0, NETLINK_GROUPS_MASK, GFP_KERNEL);

    return 0;
}


#ifdef USED_TIME_LIMITED
static int time_check(void)
{
    struct timeval tv;
    uint32 curr_secs = 0;
    
    do_gettimeofday(&tv);
    curr_secs = (uint32)tv.tv_sec;

    PT_DEBUG("Curr secs %d limits secs %d.\n", curr_secs, EXPIRED_TIME_JIFFIES); 
    if (curr_secs > EXPIRED_TIME_JIFFIES){
        PT_ERR("Limits here.");
        return TRUE;
    } else {
        return FALSE;
    }
}
#endif


static int netlink_thread( void * arg)
{
    struct list_head *pos, *next;
    int ret;
    netlink_msg_node_t *p_node = NULL;
	unsigned long flag;
    
    
    while(!kthread_should_stop()){
        __set_current_state(TASK_INTERRUPTIBLE);
        if (list_empty(&G_nl_msg_head)){
            schedule();
        }

        __set_current_state(TASK_RUNNING);
        if (!list_empty(&G_nl_msg_head)){   
            
            spin_lock_irqsave(&G_nl_lock, flag);
            list_for_each_safe(pos, next, &G_nl_msg_head){
                p_node = list_entry(pos, netlink_msg_node_t, linked);
                spin_unlock_irqrestore(&G_nl_lock, flag);            
                wait_for_completion(&client_ready);
#ifdef USED_TIME_LIMITED
                if (time_check()){
                    limited_set();
                    netlink_node_put(p_node);
                    goto out;
                }
#endif
                
                ret = netlink_send_msg(&p_node->msg);
                if (ret == 0){
                    spin_lock_irqsave(&G_nl_lock, flag);
                    list_del(pos);
                    spin_unlock_irqrestore(&G_nl_lock, flag);
                    netlink_node_put(p_node);
                    p_node = NULL;
                } else {
                    PT_ERR("failed to send message.\n");
                    return -1;
                }
out:
                spin_lock_irqsave(&G_nl_lock, flag);
            }
            spin_unlock_irqrestore(&G_nl_lock, flag);
        }
    }

    return 0;
}



static void netlink_data_reply(struct sk_buff *__skb)
{
    complete(&client_ready);  
    PT_DEBUG("!!got data reply..\n");    
}


static int netlink_node_init(void)
{
    netlink_msg_node_t *p_node = NULL;
    unsigned long flag;
    int i = 0;

    for (i=0; i<NODE_NUM_MAX; i++){
        p_node = (netlink_msg_node_t *)kmalloc(sizeof(netlink_msg_node_t),GFP_ATOMIC);
        if (p_node == NULL){
            PT_ERR("Alloc memory for ntlk failed.\n");
            return NVD_RET_ALLOC_FAILED;                
        }

        p_node->msg.magic_num = MAGIC_NUM;
        p_node->msg.sn[DISK_SN_LEN_MAX] = '\0';
        PT_DEBUG("Alloc memory for ntlk node %d.\n", i);
        spin_lock_irqsave(&G_free_lock, flag);      
        list_add_tail(&(p_node->linked), &G_free_msg_head);
        spin_unlock_irqrestore(&G_free_lock, flag);
    }

    return NVD_RET_SUCC;
}

static void netlink_node_free(void)
{
    struct list_head *pos, *next;
    netlink_msg_node_t *p_node = NULL;
    unsigned long flag;

    spin_lock_irqsave(&G_free_lock, flag);
    list_for_each_safe(pos, next, &G_free_msg_head){
        p_node = list_entry(pos, netlink_msg_node_t, linked);
        spin_unlock_irqrestore(&G_free_lock, flag); 
        list_del(pos);
            
        kfree(p_node);           
        spin_lock_irqsave(&G_free_lock, flag);
    }
    spin_unlock_irqrestore(&G_free_lock, flag);
}


int netlink_init(void)
{
    int ret = NVD_RET_SUCC;
    
    PT_INFO("netlink init.");
    
#ifdef USED_TIME_LIMITED
    if (time_check()){
        limited_set();
    }
#endif
    
    /* create netlink socket */
    Gp_ntlk_sock = netlink_kernel_create(&init_net, NTLK_LISTEN_PORT, 0, netlink_data_reply, NULL, THIS_MODULE);
    if (Gp_ntlk_sock == NULL){
        PT_ERR("Failed to create netlink socket.\n");
        return NVD_RET_INT_ERR;
    }  

    
    /* init lock */ 
    spin_lock_init(&G_nl_lock);
    spin_lock_init(&G_free_lock);

    /* init msg*/
    ret = netlink_node_init();
    if (ret != NVD_RET_SUCC){
        PT_ERR("Failed to init netlink resource.\n");
        return ret;
    }
    
    /*create thread*/
    Gp_ntlk = kthread_run(netlink_thread, NULL,"netlink_k");
    if (Gp_ntlk == NULL){
        return NVD_RET_INT_ERR;
    } 
    return NVD_RET_SUCC;
}


void netlink_exit(void)
{
    if (Gp_ntlk != NULL){
        kthread_stop(Gp_ntlk);
    }

    netlink_node_free();

    if (Gp_ntlk_sock != NULL){
        sock_release(Gp_ntlk_sock->sk_socket);
    }

}

