#include "display.h"
#include "dbclient.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include <jansson.h>

#define RESET   "\033[0m"
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */

#define BUFFER_SIZE 10*1024
#define host "localhost"
#define port 8081
#define N 5

#define read_interval 0.5
#define leddar01 "L01"

int fd;
char buffer[BUFFER_SIZE];
char json_buff[BUFFER_SIZE];
char poststr[450];
char req_buff[500];
int objectIDs[N]={0};

int obj_index=0,sockfd=-1;
float X[N],Y[N];

/**
 * Preparing query command in json formate
 **/
void jsonfrmt_leddarquery(char *msg, char *select_cmd)
{
	json_t *obj = json_pack("{ss}",\
	 "cmd", select_cmd);
	
	strcpy(msg,json_dumps(obj,JSON_COMPACT));
	json_decref(obj);
}

/**
 * Don't show duplicated object information
 **/
static int leddar_obj_duplication(int objID) {
	int i;
	
	for(i=0;i<N && objectIDs[i]!='\0';i++)
		if( objID == objectIDs[i] )
			return -1;
	
	return 0;
}

/**
 * Prepare json formate string for leddar data
 **/
int getimageval()
{
	int i;
	int obj_index=0;
	json_t *obj, *data, *objectid, *x, *y, *x_next, *y_next, *sensortype, *sensorid;
	json_error_t error;
	
	memset(objectIDs, '\0', sizeof(objectIDs));
	printf("\nJSON_BUFFER = %s\n",json_buff);
	obj= json_loads(json_buff,0,&error);
	
	if(!obj)
	{
		fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
		return 1;
	}
	
	if(!json_is_array(obj))
	{
		fprintf(stderr, "error: obj is not an array\n");
		json_decref(obj);
		return 1;
	}
	
	for(i = 0; i < json_array_size(obj); i++)
	{
		data = json_array_get(obj, i);
		if(!json_is_object(data))
		{
			fprintf(stderr, "error: commit data %d is not an object\n",  i);
			json_decref(obj);
			return 1;
		}
			
		sensortype = json_object_get(data, "sensortype");
		sensorid = json_object_get(data, "sensorid");
		objectid = json_object_get(data, "objectid");
		
		/*if(!json_is_integer(sensortype) || !json_is_integer(sensorid) || !json_is_integer(objectid))
		{
			fprintf(stderr, "error: commit %d: sha is not a string\n",  i);
			json_decref(obj);
			return 1;
		}*/

		if( json_integer_value(sensortype) == 1 && json_integer_value(sensorid) == 1 && leddar_obj_duplication(json_integer_value(objectid)) == 0)
		{
			objectIDs[obj_index]=json_integer_value(objectid);
			obj_index++;
		
			x = json_object_get(data, "x");
			y = json_object_get(data, "y");
			x_next = json_object_get(data, "x_next");
			y_next = json_object_get(data, "y_next");
			if(!json_is_real(x) || !json_is_real(y) || !json_is_real(x_next) || !json_is_real(y_next))
			{
				fprintf(stderr, "error: commit %d: sha is not a string\n",  i);
				json_decref(obj);
				return 1;
			}

			printf(GREEN "\nobjectid = %d sensorid = %d sensortype = %d x=%f y=%f x_next=%f y_next=%f\n",(int)json_integer_value(objectid), (int)json_integer_value(sensorid),\
			 (int)json_integer_value(sensortype), json_real_value(x), json_real_value(y),json_real_value(x_next), json_real_value(y_next));
			printf(RESET); 
			put_image(json_integer_value(sensorid), json_real_value(x), json_real_value(y), json_real_value(x_next), json_real_value(y_next)); 

		}
	}
	
	//printf("getimageval....\n");
	return 0;
}

/**
 * Prepare HTTP POST query for data server 
 **/
int http_post_query(int fd)
{
	char *start, *end;
	char cmd[]= "SELECT * FROM LEDDAR_DATA WHERE sensortype=1 ORDER BY id DESC LIMIT 5";//"SELECT (sensortype, sensorid, objectid, objecttype, x, y, x_vel, y_vel, x_next, y_next) FROM SENSOR_DATA WHERE sensortype=1 ORDER BY id DESC LIMIT 5";//"SELECT * FROM sensor_data LIMIT 2 OFFSET (SELECT COUNT(*) FROM sensor_data)-2;";
	
	memset(poststr,'\0',sizeof(poststr));
	jsonfrmt_leddarquery(poststr, cmd);
	
	memset(req_buff,'\0',sizeof(req_buff));
	snprintf(req_buff, sizeof(req_buff),
		 "POST %s HTTP/1.0\r\n"
		 "Content-type: application/json\r\n"
		 "Content-length: %d\r\n\r\n"
		 "%s", "/query", (int)strlen(poststr), poststr);
	
	printf("req_buff = %s\n",req_buff);	 
	write(fd, req_buff, strlen(req_buff));
	
	bzero(buffer, BUFFER_SIZE);
	while(read(fd, buffer, BUFFER_SIZE - 1) != 0){
		fprintf(stderr, "\nBUFFER = %s\n", buffer);
		//bzero(buffer, BUFFER_SIZE);
	}
	
	start = strchr(buffer, '[');
	end = strchr(buffer, ']');
	memset(json_buff,'\0',sizeof(json_buff));
	if (end != 0){
		strncpy(json_buff, start, (int)(end-start)+1);
		//printf("\nJSON_BUFFER = >%s<\n",json_buff);
		//getimageval(json_buff);
		return 0;
	} else {
		printf(RED "buffer doesn't have proper data\n");
		printf(RESET); 
		return 1;
	}
}

/**
 * Read leddar DB and show up on image
 **/
int read_table(int fd)
{
	if( http_post_query(fd) == 0 && json_buff[0] != '\0' )
		getimageval();
}

int main(int argc, char* argv[]) {
	char table1_name[]= "data_leddar";
	time_t start_t, end_t;
		
	init_visualization();
		
	while(1){

		sockfd = create_socket(8081);
		if ( 1 == sockfd ){
			printf( RED"check port number is correct\n");
			printf(RESET);
			return -1;
		}
		
		init_image();
		if(read_table(sockfd) == 0)
			get_image();
		//else
		//	get_prev_image();
		time(&start_t); time(&end_t);
		while( difftime(end_t, start_t) < read_interval)
			time(&end_t);
		
		closesocket(sockfd);		
	}

	printf(".....END GAME....\n");
	return 0;
}
