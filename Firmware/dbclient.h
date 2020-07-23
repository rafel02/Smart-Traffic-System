#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<jsoncpp/json/json.h> 

using namespace std;
//std::string msg; // msg.c_str()

void jsonfrmt_leddar(char *msg, double timestamp, int sensortype, int sensorid, int objectid, int objecttype, float dist, float amp,float x, float y, float x_vel, float y_vel, float x_next, float y_next);
int senddata( int sockfd, char* msg );
void closesocket(int fd);
int create_socket(int port);
