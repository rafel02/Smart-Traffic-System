#include "dbclient.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include <jansson.h>

#define host "localhost"
//#define port 8080
//#define buff_sz 1024

char r_buff[500];

void jsonfrmt_leddar(char *msg, double timestamp, int sensortype, int sensorid, int objectid, int objecttype, float dist, float amp,\ 
	float x, float y, float x_vel, float y_vel, float x_next, float y_next)
{
	//printf("start jsonfrmt_leddar....\n");
	json_t *obj = json_pack("{sisisisisfsfsfsfsfsf}",\
	 "sensortype", sensortype,\
	 "sensorid", sensorid,\
	 "objectid", objectid,\
	 "objecttype", objecttype,\
	 "x", x,\
	 "y", y,\
	 "x_vel", x_vel,\
	 "y_vel", y_vel,\
	 "x_next", x_next,\
	 "y_next", y_next);

	strcpy(msg,json_dumps(obj,JSON_COMPACT));
	//json_dump_file(obj, "out.json", JSON_COMPACT);
	json_decref(obj);
	//printf("end jsonfrmt_leddar....msg=%s\n",msg);
}

int senddata( int fd, char* msg )
{ 
	char page[] = "/submitjson";
	
	memset(r_buff,'\0',sizeof(r_buff));
	snprintf(r_buff, sizeof(r_buff),
		 "POST %s HTTP/1.0\r\n"
		 "Content-type: application/json\r\n"
		 "Content-length: %d\r\n"
		 "Connection: Keep-Alive\r\n\r\n"
		 "%s", page, (int)strlen(msg), msg);
	
	printf(">> r_buff = %s\n",r_buff);	 
	write(fd, r_buff, strlen(r_buff));
}

void closesocket(int fd)
{
	shutdown(fd, SHUT_RDWR); 
	close(fd); 
}

int create_socket(int port)
{
	struct hostent *hp;
	struct sockaddr_in addr;
	int on = 1, sockfd;     

	if((hp = gethostbyname(host)) == NULL){
		herror("gethostbyname");
		exit(1);
	}
	bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;
	sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	//setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));

	if(sockfd == -1){
		perror("setsockopt");
		exit(1);
	}
	
	if(connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1){
		perror("connect");
		exit(1);

	}
	return sockfd;
}
