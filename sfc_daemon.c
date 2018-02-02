#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h> 
#include<unistd.h>
#include<stdint.h>
#include<syslog.h>
#include<signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <errno.h>

/*used for ioctl*/
#include <sys/ioctl.h>
#include <net/if.h>

#define port 8888  //Define the port number

/*SFC node status*/
enum {
    OFF,
    FIRST,
    NODE,
    END
};

typedef struct sfc {
	/*VF number*/
    uint8_t vf;
    /*this VM's prev node's ip*/
    uint32_t prev_ip;
    /*this VM's ip*/
    uint32_t ip;
    /*this VM's next node's ip*/
    uint32_t next_ip;
    /*this VM's mac*/
    unsigned char mac[6];
    /*client's ip and mac*/
    uint32_t sa_ip;
    unsigned char sa_mac[6];
    struct sfc *next;
    /*store this is the head, end or not in SFC*/
    uint8_t sfc_mode;
}sfc_t;

typedef struct buffer {
    uint8_t vf;
    uint32_t prev_ip;
    uint32_t ip;
    uint32_t next_ip;
    unsigned char mac[6];
    uint32_t sa_ip;
    unsigned char sa_mac[6];
    /*store total SFC ammount*/
    int total_sfc;
}buffer_t;

int send_netlink(sfc_t* node);
int send_sfc_list(sfc_t* node, int total_sfc);
static void start_daemon();

int main(int argc , char *argv[])
{
	if (argc != 2) {
		syslog(LOG_NOTICE,"wrong ammount argument");
		return -1;
	}

    start_daemon();

    int sock, client_sock, read_size, sfc_bool = 1,sfc_count, total_sfc;
    struct sockaddr_in server;
    sfc_t *sfc_head, *cur, *prev;
    buffer_t buf[64] = {{0,0,0,0,"0",0,"0",0}};
    socklen_t client_size;
    struct sockaddr_in client;
    struct ifreq ifr;
    
    //Create socket using IP V4 protocol
    sock = socket(AF_INET,SOCK_STREAM,0);

    //Display error message
	if (sock == -1) {
		syslog(LOG_NOTICE,"Could not create socket.");
		return -1;
    }
    syslog(LOG_NOTICE,"Socket created. %d", sock);

    /*use ioctl to get ip addr*/
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name,argv[1],IFNAMSIZ-1);
    ioctl(sock,SIOCGIFADDR,&ifr);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr));
    server.sin_port = htons(port);
    
    if (bind(sock,(struct sockaddr *)&server,sizeof(server)) < 0) {
        //Display the error message
        syslog(LOG_NOTICE,"Error! Bind failed.");
        return -1;
    }
    syslog(LOG_NOTICE,"Bind done.");

    listen(sock,3);
    
    while(1) {
    
        //Accept and incoming connection
        syslog(LOG_NOTICE,"Waiting for incoming connections...");

        //Accept connection from an incoming client
        client_sock = accept(sock,(struct sockaddr *)&client,&client_size);
        if (client_sock < 0) {
			syslog(LOG_NOTICE,"Accept failed. %d %d", errno, sock);
           	continue;
        }
    
        /*Receive a message from prev node*/
        read_size = recv(client_sock,buf,sizeof(buf),0);
        if (read_size == 0) {
            syslog(LOG_NOTICE,"Client disconnected.");
           	continue;
        }
        else if (read_size == -1) {
            syslog(LOG_NOTICE,"Recv failed.");
       		continue;
        }

        /*set sfc head*/
        sfc_head = (struct sfc*)malloc(sizeof(sfc_t));
        sfc_head->vf = buf[0].vf;
        sfc_head->prev_ip = buf[0].prev_ip;
        sfc_head->ip = buf[0].ip;
        sfc_head->next_ip = buf[0].next_ip;
        memcpy(sfc_head->mac,buf[0].mac,6);
        sfc_head->sa_ip = buf[0].sa_ip;
        memcpy(sfc_head->sa_mac,buf[0].sa_mac,6);
        sfc_head->next = NULL;
        prev = sfc_head;
        //syslog(LOG_NOTICE,"sa_ip %u %u %u\n", sfc_head->sa_ip, buf[0].sa_ip, sfc_head->vf);
        sfc_count = 1;

        for(int i=1; buf[i].vf!=0; i++,prev=prev->next) {
            cur = (struct sfc*)malloc(sizeof(sfc_t));
            cur->prev_ip = buf[i].prev_ip;
            cur->ip = buf[i].ip;
            cur->next_ip = buf[i].next_ip;
            cur->vf = buf[i].vf;
           	memcpy(cur->mac,buf[i].mac,6);
           	cur->sa_ip = buf[i].sa_ip;
            memcpy(cur->sa_mac,buf[i].sa_mac,6);
           	cur->next = NULL;
            //syslog(LOG_NOTICE,"buf ip = %u buf vf = %d\n", buf[i].ip,buf[i].vf);
            prev->next = cur;
            //syslog(LOG_NOTICE,"ip = %u vf = %d\n", prev->next->ip,prev->next->vf);
           	sfc_count++;
        }
        prev->next = NULL;
        /*Send the result to client*/
        write(client_sock,&sfc_bool,sizeof(sfc_bool));
        total_sfc = buf[0].total_sfc;
        //syslog(LOG_NOTICE,"sfc count %d total sfc %d\n", sfc_count, buf[0].total_sfc);
        
        /*if this VM is the first node in sfc, we fill the blank such as sfc next and prev ip and mac*/
        if (sfc_count == buf[0].total_sfc) {
            sfc_head->sfc_mode = FIRST;
            sfc_head->prev_ip = sfc_head->sa_ip;
            sfc_head->next_ip = sfc_head->next->ip;
            unsigned char tmp_next_mac[6];
            memcpy(tmp_next_mac,sfc_head->mac,6);
            memcpy(sfc_head->mac,sfc_head->next->mac,6);
            //syslog(LOG_NOTICE,"next_mac = %pM prev_mac = %pM next->mac = %pM\n", sfc_head->mac, sfc_head->sa_mac, sfc_head->next->mac);
            sfc_t *tmp;
            sfc_t *tmp2;
            //syslog(LOG_NOTICE,"next ip = %d\n", sfc_head->next_ip);
           	for(tmp=sfc_head->next,tmp2=sfc_head; tmp->next; tmp=tmp->next,tmp2=tmp2->next) {
                tmp->prev_ip = tmp2->ip;
                tmp->next_ip = tmp->next->ip;
                //syslog(LOG_NOTICE,"next ip = %u ip = %u prev ip = %u\n", tmp->next_ip,tmp->ip, tmp->prev_ip);
                memcpy(tmp->sa_mac,tmp_next_mac,6);
                memcpy(tmp_next_mac,tmp->mac,6);
                memcpy(tmp->mac,tmp->next->mac,6);
            }
            tmp->prev_ip = tmp2->ip;
            tmp->next_ip = sfc_head->prev_ip;
            memcpy(tmp->sa_mac,tmp_next_mac,6);
            memcpy(tmp->mac,sfc_head->sa_mac,6);

            /*send node to netlink*/
            if (send_netlink(sfc_head) == -1)
                syslog(LOG_NOTICE,"set error");
            else
                syslog(LOG_NOTICE,"set sucess");
        }
        else if (sfc_count < buf[0].total_sfc && sfc_count > 1) {
            /*this VM is not head or end in sfc*/
            sfc_head->sfc_mode = NODE;
            if (send_netlink(sfc_head) == -1)
                syslog(LOG_NOTICE,"set error");
            else
                syslog(LOG_NOTICE,"set sucess");
        }
        else if (sfc_count == 1) {
            sfc_head->sfc_mode = END;
            if (send_netlink(sfc_head) == -1)
                syslog(LOG_NOTICE,"set error");
            else
                syslog(LOG_NOTICE,"set sucess");
            continue;
        }
        /*send list to next service function*/
        if (send_sfc_list(sfc_head->next,total_sfc) < 0)
        syslog(LOG_NOTICE,"send sfc error");
        //printf("sfc head next  = %s %d\n", sfc_head->next->ip, sfc_head->next->vf);
    }
    close(sock);
    return 0;
}

static void start_daemon()
{
    pid_t pid = fork();

    if (pid < 0)
    exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_FAILURE);
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    signal(SIGCHLD,SIG_IGN);
    signal(SIGHUP,SIG_IGN);
    
    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_FAILURE);
    umask(0);
    chdir("/");
	for(int i=sysconf(_SC_OPEN_MAX); i>=0; i--)
        close(i);
    /*log file is at /var/log/messages */
    openlog("sfc_daemon", LOG_PID, LOG_DAEMON);
}

int send_netlink(sfc_t* node)
{
    #define NETLINK_TEST 17
    #define MAX_PAYLOAD 1024
    struct sockaddr_nl src_addr, dest_addr;
    struct msghdr msg;
    struct nlmsghdr *nlh = NULL;
    struct iovec iov;
    int sock_fd;
    int ret;

    //printf("node = %s %d\n", node->ip, node->vf);
    //printf("mac = %pM\n", node->mac);
    memset(&msg,0,sizeof(msg));
    sock_fd = socket(PF_NETLINK,SOCK_RAW,NETLINK_TEST);
    memset(&src_addr,0,sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();
    src_addr.nl_groups = 0;
    ret = bind(sock_fd,(struct sockaddr*)&src_addr,sizeof(src_addr));
    if (ret != 0) {
      	syslog(LOG_NOTICE,"bind to kernel error");
		return -1;
	}
    
    memset(&dest_addr,0,sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;
    dest_addr.nl_groups = 0;
    
    nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;
   
    memcpy(NLMSG_DATA(nlh),node,sizeof(sfc_t));
    //syslog(LOG_NOTICE,"nlh sa_ip = %d",NLMSG_DATA(nlh));
    iov.iov_base = (void*)nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void*)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    ret = sendmsg(sock_fd, &msg,0);
    if (ret < 0) {
        syslog(LOG_NOTICE,"send to kernel error");
        return -1;
    }
    close(sock_fd);
    return 0;
}

int send_sfc_list(sfc_t* node, int total_sfc)
{
    int sock;
    struct sockaddr_in server;
    buffer_t new_sfc_list[64]  = {{0,0,0,0,"0",0,"0",0}};
    sfc_t *cur;
    int i=0, sfc_bool;

    /*transfer SFC from link list to array*/
    for(cur=node; cur; cur=cur->next) {
        new_sfc_list[i].vf = cur->vf;
        new_sfc_list[i].prev_ip = cur->prev_ip;
       	new_sfc_list[i].ip = cur->ip;
        new_sfc_list[i].next_ip = cur->next_ip;
       	memcpy(new_sfc_list[i].mac,cur->mac,sizeof(new_sfc_list[i].mac));
       	new_sfc_list[i].sa_ip = cur->sa_ip;
        memcpy(new_sfc_list[i].sa_mac,cur->sa_mac,sizeof(new_sfc_list[i].sa_mac));
        new_sfc_list[i].total_sfc = total_sfc;
       	//syslog(LOG_NOTICE,"next vf = %d next ip = %u",new_sfc_list[i].vf, new_sfc_list[i].ip);
       	i++;
    }
    if ((sock = socket(AF_INET,SOCK_STREAM,0)) == -1) {
        syslog(LOG_NOTICE,"Could not create socket next. ");
       	return -1;
    }
    syslog(LOG_NOTICE,"Socket next created.");
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = new_sfc_list[0].ip;
    server.sin_port = htons(port);

    if (connect(sock,(struct sockaddr*)&server,sizeof(server)) < 0) {
       	syslog(LOG_NOTICE,"connect next failed.%d",errno);
       	return -1;
    }
    if (send(sock,new_sfc_list,i*sizeof(sfc_t),0) < 0) {
       	syslog(LOG_NOTICE,"Send next failed");
       	return -1;
    }
    if (recv(sock,&sfc_bool,sizeof(int),0) < 0) {
       	syslog(LOG_NOTICE,"recv next failed");
       	return -1;
    }
    syslog(LOG_NOTICE,"%s", sfc_bool == 1 ? "set next sucessfully" : "set next failed");
    close(sock);
}
