#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#define NL_CONT_SIZE      92
#define NL_PAYLOAD_MAX    256
#define NL_EVENT_PORT     17 // can't be changed.
#define NL_CLIENT_ADDR    111 // can be configure, need be greater than 110  

#define DISK_SN_LEN_MAX     20 // can't be changed.


enum{
    MSG_TYPE_DISK_ERR,
    MSG_TYPE_DISK_STATE,
    MSG_TYPE_REMAP_EVENT,
    MSG_TYPE_DISK_ONOFF,
    MSG_TYPE_MAX,
};

enum{
    DISK_STATE_ON,
    DISK_STATE_OFF,
    DISK_STATE_MAX,
};

typedef unsigned char       uint8;
typedef unsigned int        uint32;
typedef unsigned long long  uint64;


typedef struct ntlk_recv{
    int u_sock;   
    char recv_buf[NLMSG_SPACE(NL_PAYLOAD_MAX)];
}ntlk_recv_t;


#define MAGIC_NUM     0xfe45798f
typedef struct ntlk_msg{
    uint32 magic_num;          //magic_num
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


ntlk_recv_t g_ntlk_recv;

/*
    nltk_msg_t list:    
    msg_index     details    
    0    
    1
    2
*/
static void msg_parse(nltk_msg_t *p_ntlk_msg)
{
    if (p_ntlk_msg->magic_num != MAGIC_NUM){
        printf("Error!!Get wrong msg. magic 0x%x expect 0x%x\n", p_ntlk_msg->magic_num, MAGIC_NUM);
        return;
    }
    switch(p_ntlk_msg->t_index){
        case MSG_TYPE_DISK_ERR:
        case MSG_TYPE_DISK_STATE:  
            // add process code here.
            printf("%d-%d-%d-%d %s %s status %d tm %d err %d abt %d\n",
                p_ntlk_msg->no[0], p_ntlk_msg->no[1],p_ntlk_msg->no[2],p_ntlk_msg->no[3], p_ntlk_msg->sn, 
                (p_ntlk_msg->t_index==MSG_TYPE_DISK_ERR)?"DiskErr":"DiskState",
                p_ntlk_msg->details.io_info.status, p_ntlk_msg->details.io_info.tm_cnt, p_ntlk_msg->details.io_info.err_cnt, p_ntlk_msg->details.io_info.abt_cnt);   //.i.e  DISK[0-0-0-0]: disk error or state change
            break;
        case MSG_TYPE_REMAP_EVENT:  
            // add process code here.
            printf("%d-%d-%d-%d %s Remap cnt %d sectors 0x%lx orgLBA 0x%llx newLBA 0x%llx\n",
                p_ntlk_msg->no[0], p_ntlk_msg->no[1],p_ntlk_msg->no[2],p_ntlk_msg->no[3], p_ntlk_msg->sn,
                p_ntlk_msg->details.remap.remap_cnt, p_ntlk_msg->details.remap.sectors, p_ntlk_msg->details.remap.org_lba, p_ntlk_msg->details.remap.new_lba);   //.i.e  DISK[0-0-0-0]: remap .
            break;  
        case MSG_TYPE_DISK_ONOFF:
            switch(p_ntlk_msg->details.onoff.state){
                case DISK_STATE_ON:
                    printf("%d-%d-%d-%d %s online.\n", p_ntlk_msg->no[0], p_ntlk_msg->no[1],p_ntlk_msg->no[2],p_ntlk_msg->no[3], p_ntlk_msg->sn);
                    break;
                case DISK_STATE_OFF:
                    printf("%d-%d-%d-%d %s offline.\n", p_ntlk_msg->no[0], p_ntlk_msg->no[1],p_ntlk_msg->no[2],p_ntlk_msg->no[3], p_ntlk_msg->sn);
                    break;
                default:
                    printf("Get wrong disk state %d.\n", p_ntlk_msg->details.onoff.state);
            }           
            break;
        default:
            printf("!!!!Invliad type 0x%x.\n", p_ntlk_msg->t_index);
            break;
    }
}

static int ntlk_sinit(void)
{
    struct sockaddr_nl addr; 

    g_ntlk_recv.u_sock= socket(PF_NETLINK, SOCK_DGRAM, NL_EVENT_PORT);
    if(g_ntlk_recv.u_sock < 0)
    {  
        printf("ERROR: create netlink socket error.\n");
        return -1;
    }  
    
    addr.nl_family = PF_NETLINK;  
    addr.nl_pad    = 0;  
    addr.nl_pid    = NL_CLIENT_ADDR;  
    addr.nl_groups = 1;  //group
      
    if(bind(g_ntlk_recv.u_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)  
    {  
        printf("ERROR: bind socket error.\n");  
        return -1;  
    }  

    return 0;
}

static void ntlk_recv(void)
{
    
    struct iovec iov;
    struct msghdr msg;
    struct sockaddr_nl dst_addr;
    struct nlmsghdr *p_nlh = NULL;
    int len;

    //memset(&dst_addr, 0, sizeof(dst_addr));  
    dst_addr.nl_family = AF_NETLINK;  
    dst_addr.nl_pid = 0;   /* For Linux Kernel */  

    /* Fill the netlink message header */ 
    p_nlh = (struct nlmsghdr*)g_ntlk_recv.recv_buf;
    p_nlh->nlmsg_len = NLMSG_LENGTH(NL_PAYLOAD_MAX);  
    p_nlh->nlmsg_pid = getpid();  /* self pid */  
    p_nlh->nlmsg_flags = 1;  

    iov.iov_base = (void *)p_nlh;  
    iov.iov_len = p_nlh->nlmsg_len;  

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void *)&dst_addr;  
    msg.msg_namelen = sizeof(struct sockaddr_nl);  
    msg.msg_iov = &iov;  
    msg.msg_iovlen = 1;
      
    while(1) {        
         /* send */ 
        dst_addr.nl_groups = 0;
        memset(p_nlh, 0, NLMSG_LENGTH(NL_PAYLOAD_MAX));
        len = sendmsg(g_ntlk_recv.u_sock, &msg, 0);
        if (len == 0){                        
             printf("ERROR: netlink recv sendmsg error.\n" );
             goto out;
        }

        dst_addr.nl_groups = 1; //group
        memset(p_nlh, 0, NLMSG_SPACE(NL_PAYLOAD_MAX));
        len = recvmsg(g_ntlk_recv.u_sock, &msg, 0); 
        if (len == 0){                        
            printf("ERROR: recv err.");
            break;
        }

        msg_parse((nltk_msg_t *)NLMSG_DATA(p_nlh));       
    }  
    
out:    
    close(g_ntlk_recv.u_sock);
}

int ntlk_process(void)
{  
    if (ntlk_sinit() != 0){
        printf("ERROR: Initialize ntlk failed.\n");
        return -1;
    }
    
    ntlk_recv();
    return 0;
}


int main(void)
{
    return ntlk_process();
}


