#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h> 
#include<unistd.h>
#include<stdint.h>

/*used for ioctl*/
#include <sys/ioctl.h>
#include <net/if.h>

int main()
{
    int sock, client_sock, read_size;
    struct sockaddr_in server,client;
    char buf[256], new_buf[256];;
    int length = sizeof(client);
    struct ifreq ifr;

    sock = socket(AF_INET,SOCK_DGRAM,0);
    if (sock == -1) {
        puts("Could not create socket.");
        return -1;
    }
    puts("Socket created.");
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name,argv[1],IFNAMSIZ-1);
    ioctl(sock,SIOCGIFADDR,&ifr);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr));
    server.sin_port = htons(20000);

    if (bind(sock,(struct sockaddr *)&server,sizeof(server)) < 0) {
        //Display the error message
        puts("Error! Bind failed.");
        return -1;
    }
    puts("Bind done.");

    while(1) {

        //listen(sock,3);
        puts("Waiting for incoming connections...");
    	   memset(buf,0,sizeof(buf));
        //Accept connection from an incoming client
            /*client_sock = accept(sock,(struct sockaddr *)&client,&client_size);
            if (client_sock < 0) {
                puts("Accept failed.");
            continue;
            }
            puts("Connection accepted.");*/
    
            //Receive a message from client
            read_size = recvfrom(sock,buf,1024,0,(struct sockaddr*)&client,(socklen_t *)&length);

            /*if (read_size == 0) {
            puts("Client disconnected.");
            continue;
            //fflush(stdout);
            }*/
            if (read_size == -1) {
                puts("Recv failed.");
                continue;
            }
            //printf("buf = %s recv bytes = %d\n", buf, recv);
#if 0            
            char *loc = strstr(buf, "pineapple.");
            if (loc) {
                memset(new_buf, 0, strlen(new_buf));
                strncpy(new_buf,buf,(loc-buf));
                char tmp[256] = {0};
                //strncpy(tmp,);
                strncpy(tmp,(buf + (strlen("pineapple.") + (loc - buf))),strlen(buf + strlen("pineapple.") + (loc - buf)));
                strncat(new_buf,tmp,strlen(tmp));
                printf("new_buf = %s\n", new_buf);
            }
            else 
                printf("No match.\n");
            //free(loc);
            
            if (sendto(sock, new_buf, read_size, 0, (struct sockaddr*)&client, length) < 0) {
                perror("send fail");
                continue;
            }
#else
	    if (sendto(sock, buf, read_size, 0, (struct sockaddr*)&client, length) < 0) {
                perror("send fail");
                continue;
            }
#endif
            //write(client_sock,buf,strlen(buf));
    }
    close(sock);
    return 0;
}
