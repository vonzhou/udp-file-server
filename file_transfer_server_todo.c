/* UDP file server FIXME


		--vonzhou20140904
*/


#include "global.h"

#define SERV_PORT 2500
#define MAX 1428
typedef struct{
	unsigned char fp[20];
	int chunk_id;
	short flags;
	short chunk_len;
	char data[MAX_CHUNK_SIZE];
//	char data[0];
}TransferUnit;  

// some flags for this chunk transfered.
enum{
        UNIT_NEED_DEDU = 0x0001,   // need deduplication
        UNIT_FILE_START = 0x0002,   // first gram , file metadata
        UNIT_FILE_END = 0x0004,
        CHUNK_OTHER = 0x0008,      // other for extension
};

void recvFile(char name[20],int sockfd){
	
    int ret,fd;
	mode_t fdmode = (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    unsigned char mesg[MAX];
    fd_set rdset;
    struct timeval tv;
    int rlen, wlen, chunklen;
    int i;
	TransferUnit *header = NULL;// = (TransferUnit *)malloc(sizeof(TransferUnit));

    fd = open(name,O_RDWR|O_CREAT|O_APPEND,fdmode);
    if(fd == -1)        {
     	  printf("open file %s error:%s \n",name,strerror(errno));
    	  exit(-1);
    }
	while(1){
    	memset(mesg, 0, sizeof(mesg));
		// read the gram header
      	rlen = recvfrom(sockfd, mesg, sizeof(mesg), 0, NULL, NULL);
		header = (TransferUnit *)mesg;
		//printf("%d\n", rlen);
       	if(rlen != sizeof(TransferUnit) ){
       		printf("read sockfd error %s\n",strerror(errno));
            exit(-1);
       	}
		// recv file end 
        if((header->flags ^ UNIT_FILE_END) == 0){
        	printf("recv end.\n");
           	break;
        }
		// recv file metadata 
        if((header->flags ^ UNIT_FILE_START) == 0){
        	int len = header->chunk_len;  // metadata length...
			char metadata[100] = {0};
			printf("==== begin file recv ====,metadata len = %d\n", len);
			strcpy(metadata, header->data, len);
			if((rlen = recvfrom(sockfd, mesg, len, 0, NULL, NULL)) == len){
				mesg[len] = '\0';
				printf("File name : %s\n", mesg);
			}
           	continue;
        }
		
		chunklen = header->chunk_len;
		// read the actual chunk data 
      	rlen = recvfrom(sockfd, mesg, chunklen, 0, NULL, NULL);
		printf("rlen = %d, chunklen= %d\n", rlen, chunklen);
		printf("chunk_id:%d,chunk_len:%d \n", header->chunk_id, header->chunk_len);
       	if(rlen != chunklen ){
       		printf("read chunk data error %s\n",strerror(errno));
            exit(-1);
       	}

		printf("chunk_id:%d,chunk_len:%d \n", header->chunk_id, header->chunk_len);
		
		// write the chunk data to this file
        wlen = write(fd,mesg,rlen);
        if(wlen != rlen ){
        	printf("write error %s\n",strerror(errno));
            exit(-1);
        }        
                
        printf("The %d times write\n",i);
	}
    close(fd);
}

int main(int argc, char *argv[])
{
        int sockfd;
        int r;
        struct sockaddr_in servaddr;

        sockfd = socket(AF_INET,SOCK_DGRAM,0); /*create a socket*/
	
        /*init servaddr*/
        bzero(&servaddr,sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(SERV_PORT);

        //bind address and port to socket*
        if(bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) == -1)
        {
                perror("bind error");
                exit(-1);
        }

        //r = fcntl(sockfd, F_GETFL, 0);
        //fcntl(sockfd, F_SETFL, r & ~O_NONBLOCK);
        
        recvFile(argv[1],sockfd);
        return 0;

}
