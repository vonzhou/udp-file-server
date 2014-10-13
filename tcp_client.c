/*--vonzhou
 use tcp for fun
*/

#include "global.h"

#define SERV_PORT 2500

typedef struct {
        unsigned char fp[20];
        int chunk_id;
        short flags;
        short chunk_len; // The max is 32767 enough
        char data[MAX_CHUNK_SIZE];
}TransferUnit;

// some flags for this chunk transfered.
enum{
        UNIT_NEED_DEDU = 0x0001,   // need deduplication
        UNIT_FILE_START = 0x0002,   // first gram , file metadata
		UNIT_FILE_END = 0x0004,
        CHUNK_OTHER = 0x0008,      // other for extension
};

FileInfo *fi;

int sendTCP(FileInfo *fi,int sockfd,struct sockaddr *pservaddr,socklen_t servlen){
	char buf[sizeof(TransferUnit)];
    struct timeval tv;
    int rlen,wlen, len = 1;
    int fd;
    int ret;
    int i,j;
	FingerChunk *p;
	TransferUnit unit;
	
	//connect to UDP server
    if(connect(sockfd,(struct sockaddr *)pservaddr,servlen) == -1)
    	err_quit("connet error");
    else
       	printf("connect server ok!\n");

    fd = open(fi->file_path, O_RDONLY);
    if(fd==-1)
		err_quit("fopen error %s\n",strerror(errno));
    
	// begin to transfer the file to the server;            
    i=0;
    p = fi->first;
               
    while(p){        
       	tv.tv_sec = 1;
        tv.tv_usec = 0;
                        
		//prepare for writing the socket        
        memset(&unit, 0, sizeof(unit));
		/* we first transfer some medadata,like filename,super feature, feature vector,
		*etc, to make a packetin, here just let the fp = 20 1s	*/
		
		// use SF-2, maybe not good

		// construct the first unit to send
		if(i == 0){
			for(j=0;j<20;j++)
				unit.fp[j] = fi->file_hash[j];
			unit.chunk_id = i++;
			//need not dedu
			unit.flags = 0 | UNIT_FILE_START;
			unit.chunk_len = strlen(fi->file_path);
			memcpy(unit.data, fi->file_path, unit.chunk_len);
			printf("file name : %s \n", unit.data);
			wlen = write(sockfd, &unit, 28 + strlen(fi->file_path));
            if(wlen != (28 + strlen(fi->file_path)))
            	err_quit("write data to sockfd error:%s\n",strerror(errno));
			//memset(&unit, 0, sizeof(unit));
			continue;
		}

		if(p != NULL)
			len = p->chunklen;
		// the last read , p is null, so cannnot use p->chu
        rlen = read(fd, unit.data, len);
        if(rlen < 0)
            err_quit("file read data error %s\n",strerror(errno));
		
		// construct this tranfer unit we cannot before read
		//bcos the following p pointer used.
		for(j=0; j< 20;j++)
           	unit.fp[j] = p->chunk_hash[j];// 20B fingerprint;
        unit.chunk_id = i++; // 4B chunk ID
        unit.chunk_len = p->chunklen;
		unit.flags = UNIT_NEED_DEDU;
		//write to socket
        wlen = write(sockfd, &unit, 28 + p->chunklen);
        if(wlen != (rlen + 28))
           	err_quit("write data to sockfd error:%s\n",strerror(errno));
                                
        p = p->next;
		memset(&unit, 0, sizeof(unit));
        usleep(500);
        printf("The %d times read\n",i);
	
	}// end while(p)
	
	//indicate the transfer completed, just a unit header
	close(fd);
	unit.flags = UNIT_FILE_END ;
    wlen = write(sockfd, &unit, 28);
    if(wlen != 28)
    	err_quit("write end unit error:%s\n",strerror(errno));
			
 	printf("File %s Transfer Success!\n", fi->file_path);
	
}


int main(int argc ,char *argv[])
{        
        char *fh;
        struct sysinfo s_info;
        long time1,time2;
        int error1,error2;
        int sockfd;
		int res;
        struct stat fsize;
        struct sockaddr_in servaddr;
        error1= sysinfo(&s_info);
        time1 = s_info.uptime;
        int r;
		//	FileInfo *fi;
		fi = file_new();

        if(argc != 3)
		err_quit("useage:udpclient<IPaddress>;\n");
        bzero(&servaddr,sizeof(servaddr));
        servaddr.sin_family= AF_INET;
        servaddr.sin_port = htons(SERV_PORT);
        
        if(inet_pton(AF_INET,argv[1],&servaddr.sin_addr) <= 0)
        	err_quit("[%s]is not a valid IPaddress\n",argv[1]);

        sockfd =socket(AF_INET, SOCK_STREAM, 0);
        
		strcpy(fi->file_path, argv[2]);
		
		// file's sha1 
		res = SHA1File(fi->file_path, fi->file_hash);
		if(res){
			printf("Get file sha1 hash failed.\n");
			exit(-1);
		}
		// chunk the file
        chunk_file(fi);
        printf("File size : %lld\n",fi->file_size);
        printf("Chunk Num : %d\n",fi->chunknum);

        sendTCP(fi, sockfd, (struct sockaddr *)&servaddr,sizeof(servaddr));        
        close(sockfd);
	
		fprintf(stderr,"ServerIP:\t%s\n",argv[1]);        
        if(stat(argv[2],&fsize) == -1)
                perror("failed to get fiel statusi\n");
        else        
                fprintf(stderr,"file name:\t%s\nfile size:\t%dK\n",argv[2],fsize.st_size/1024);
        error2=sysinfo(&s_info);
        time2 = s_info.uptime;
        printf("tranfice file time =%ld seconds\n",(time2-time1));
}                
