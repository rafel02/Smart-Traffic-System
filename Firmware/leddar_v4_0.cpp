#include <math.h>
#include <time.h>
#include <sys/time.h>
#include "leddar4.h"
#include "dbclient.h"
#include "display.h"
//#include <string.h>
//#include <stdio.h>
//#include <stdlib.h>
#include <jsoncpp/json/json.h> 
#include <fstream>

// ***********************************************************************************************
// MACROS
// ***********************************************************************************************

#define testwithfile 0
#define DEBUGLEVEL1 1
#define DEBUGLEVEL2 0
#define DEBUGLEVEL3 0

#if DEBUGLEVEL1
#  define debug1_print printf
#else
#  define debug1_print(format, args...) ((void)0)
#endif

#if DEBUGLEVEL2
#  define debug2_print printf
#else
#  define debug2_print(format, args...) ((void)0)
#endif

#if DEBUGLEVEL3
#  define debug3_print printf
#else
#  define debug3_print(format, args...) ((void)0)
#endif

#define sect_col 1
#define amp_col 2
#define dist_col 3

#if testwithfile
	#define FREQ 2								// This value is for testing, no need to modify
	#define OBJ_DISAPPEAR_SCANLIMIT 2*FREQ
#else
	#define FREQ 7								// range is between 6-50Hz, possible frequencies(1.5625,3.125,6.25,12.5,25,50 Hz)
	#define OBJ_DISAPPEAR_SCANLIMIT 5*FREQ		// if object didn’t detected for 5*freq scans, then it’s went out of range or it’s wrong detection
#endif

#define SECCNT 16
#define SECT_OBJ_DECT_CNT 3	   					// max can detect 3 object in a sector
#define MIN_DECT_SCANLIMIT (1*FREQ)				// detected item considered as object if it appeared minimum for 1sec
#define N 10									// number of past value to be use for prediction
#define M 12									// Mth step will be predicted	

#define sensortype 1
#define sensorid 1					
// ***********************************************************************************************
// GLOBAL VARIABLES
// ***********************************************************************************************

char msg[500];
int BEAMWIDTH, LIDR_RANG;
int localvisualizationenable=0, obj_update_cnt =-1, sockfd=-1, predictionenable=0;
float HUMAN_SECTORDIFF, CAR_SECTORDIFF, HUMAN_SCANDDIFF, CAR_SCANDIFF, FIX_OBJ_UPDATE_INTERVAL, FIX_OBJ_DIS_VAR, FIX_OBJ_AMP_VAR;
float sectangle, offsetangle;
double objectID=0;         

// ***********************************************************************************************
// TYPES
// ***********************************************************************************************

struct OBJECT{
	double objectID;
	double timestamp;
	float dist;
	float amp;
	int sect_list[SECCNT];
	float x;
	float y;	
//	OBJ_DIR dir;
	OBJ_TYPE type;
	double x_vel;
	double y_vel;
//	double vel;
//	float ang;
	float X[N];
	float Y[N];
	float nxt_x;
    float nxt_y;
//  float angle;
	char update_flag;			// Enabled every time object information is updated. Disable every time at the begging for object search
	char inprogress_flag;		// Get disable after MIN_DECT_SCANLIMIT period. Object with inprogress_flag set, can't be deleted during inprogress period
	char live_flag;				// Get disable if object is not detected for OBJ_DISAPPEAR_SCANLIMIT period. After disable respective object can be used for saving other object information.
	char reuse_flag;			// Enabled after object is not live anymore, and can be resused
	long scan_cnt;				// Always increassing for live object on detectection
	long max_disappercnt;		// Increase every time live object information not updated
};

struct NEW_OBJECT{
	double timestamp;
	float dist;
	float amp;
	int sect_list[SECCNT];
	float x;
	float y;
	OBJ_TYPE type;
	int new_det;
};

struct SECT_OBJECT{ 
	double timestamp;
	float dist;
	float amp;
	int sect;
	float x;
	float y;
	int new_det;
	int fix_obj;
};

// ***********************************************************************************************
// TYPES GLOBAL VARIABLES
// ***********************************************************************************************

struct SECT_OBJECT sect_obj[SECT_OBJ_DECT_CNT*SECCNT];
struct SECT_OBJECT temp_sect_obj[SECT_OBJ_DECT_CNT*SECCNT];
struct SECT_OBJECT nonfix_sect_obj[SECT_OBJ_DECT_CNT*SECCNT];
struct SECT_OBJECT fix_obj[SECCNT];
struct NEW_OBJECT new_objs[SECT_OBJ_DECT_CNT*SECCNT];
struct OBJECT objs[SECCNT*FREQ* OBJ_DISAPPEAR_SCANLIMIT];

// ***********************************************************************************************
// FUNCTIONS DECELARATION
// ***********************************************************************************************

static void configuration();
static double get_micro();
static void get_coordinate(int sect, float dist, float *x, float *y);
static void velocity_cal( struct OBJECT &dest, struct NEW_OBJECT src );
static void print_objs();
static void send_obj();
static void update_live_flag();
static void update_inprogress_flag();
static void update_scan_and_disapper_cnt();
static void reset_obj(char *resetprop);
static void add_obj( struct OBJECT &dest, struct NEW_OBJECT src );
static void append_obj( struct NEW_OBJECT src );
static void copy_obj( struct OBJECT &dest, struct NEW_OBJECT src );
static int find_matched_sectcnt(int *newsectlist, int* storedsectlist);
static void find_update_obj( struct NEW_OBJECT new_obj );
static void object_update();
static void print_new_obj();
static void reset_new_obj(struct NEW_OBJECT new_obj[], char *resetprop);
static void new_obj_detection();
static void print_sect_obj(struct SECT_OBJECT sect_obj[]);
static void fill_coordinates();	
static void reset_sect_obj(struct SECT_OBJECT sect_obj[], char *resetprop);
static void cpy_sect_obj(struct SECT_OBJECT *dest, struct SECT_OBJECT *src);
static void print_fix_obj();
static void reset_fix_obj(char *resetprop);
static void update_nonfixsectobj();
static void update_fixobj();
static float predcit_value(float *a);
static void append_atbeginning(float val, float *a);

// ***********************************************************************************************
// IMPLEMENTATION
// ***********************************************************************************************

/**
 * Intilization of algorithum paramters from json config file
 **/
static void configuration()
{
	Json::Value root;   // starts as "null"; will contain the root value after parsing
	Json::Reader reader;
	
	std::ifstream devconfigfile("dev_config.json", std::ifstream::binary);
	devconfigfile >> root;
	
	BEAMWIDTH= root["leddar_fix_parameters"]["BEAMWIDTH"].asInt();
	LIDR_RANG= root["leddar_fix_parameters"]["LIDR_RANG"].asInt();
	
	HUMAN_SECTORDIFF= root["leddar_tunnig_parameters"]["HUMAN_SECTORDIFF"].asFloat();
	CAR_SECTORDIFF= root["leddar_tunnig_parameters"]["CAR_SECTORDIFF"].asFloat();
	HUMAN_SCANDDIFF= root["leddar_tunnig_parameters"]["HUMAN_SCANDDIFF"].asFloat();
	CAR_SCANDIFF= root["leddar_tunnig_parameters"]["CAR_SCANDIFF"].asFloat();
	FIX_OBJ_UPDATE_INTERVAL= root["leddar_tunnig_parameters"]["FIX_OBJ_UPDATE_INTERVAL"].asFloat();
	FIX_OBJ_DIS_VAR= root["leddar_tunnig_parameters"]["FIX_OBJ_DIS_VAR"].asFloat();
	FIX_OBJ_AMP_VAR= root["leddar_tunnig_parameters"]["FIX_OBJ_AMP_VAR"].asFloat();			// while new object is entering to liddar range, there can be reflection and fixed object amplitude can vary by 4 
	
	localvisualizationenable = root["leddar_test_parameters"]["localvisualizationenable"].asInt();
	predictionenable = root["leddar_test_parameters"]["predictionenable"].asInt();
	
	debug3_print("localvisualizationenable = %d predictionactivate=%d \n",localvisualizationenable,predictionenable);
	offsetangle = 90-(BEAMWIDTH/2);
	sectangle = BEAMWIDTH/SECCNT;
}

/**
 * Get time in micro second
 **/
static double get_micro() 
{
	struct timeval tv;

	gettimeofday(&tv,NULL);
	return (long)(1000000*tv.tv_sec) + tv.tv_usec;
}

/**
 * Get coordinates
 **/
static void get_coordinate(int sect, float dist, float *x, float *y)
{

	float ang_d,ang_r;
	//ang_d= 90 - ( offsetangle + ((float)(sect-1)*sectangle) + sectangle/2 );
	ang_d= 21.09375 - ((sect-1)*2.8125) ;
	ang_r = ((PI/180) * ang_d);
	*y= dist* sin(ang_r);
	*x= dist* cos(ang_r);
	debug3_print("ARGU: sect = %d dist = %f ang_d = %f ang_r= %f X = %f Y = %f\n",sect,dist,ang_d,ang_r,*x,*y);	
}

/**************************************************** ALGO LEVEL 3 : START **************************************************/

static void velocity_cal( struct OBJECT &dest, struct NEW_OBJECT src )
{
    double delt_t, delt_d,vel;
	float new_x,new_y,old_x,old_y, base, opp, ang;

	delt_t = ((double)src.timestamp - (double)dest.timestamp)/1000000;
	
	dest.x_vel = (src.x - dest.x)/delt_t;
	dest.y_vel = (src.y - dest.y)/delt_t;
	
	debug2_print("delt_t=%lf src.x=%f dest.x=%f dest.x_vel=%lf src.y=%f dest.y=%f dest.y_vel=%lf\n",\
		delt_t, src.x, dest.x, dest.x_vel, src.y, dest.y, dest.y_vel);
}

/**
 * Print final object list
 **/
static void print_objs()
{
	int i,j;

	debug2_print("------------------ DATA IN LOCAL DB --------------------\n");
	debug1_print("SectList\tTimeStamp\tObjectId Amplitude Distance\nX_vel Y_vel  X Y Next_X Next_Y \t\tscancnt - maxdisapcnt - update - inprogress - live - reuse\n");
	for(i=0; objs[i].sect_list[0] != 0; i++){

		for(j=0;objs[i].sect_list[j];j++)
			debug2_print("%d ",objs[i].sect_list[j]);
		debug1_print(GREEN "[%lf]\t<%lf> %f %f\t%f %f %f %f %f %f\t%li %li %d %d %d %d\n" RESET,\
				objs[i].timestamp, objs[i].objectID, objs[i].amp, objs[i].dist, objs[i].x_vel, objs[i].y_vel,\
				objs[i].x, objs[i].y, objs[i].nxt_x, objs[i].nxt_y, objs[i].scan_cnt, objs[i].max_disappercnt,\
				objs[i].update_flag, objs[i].inprogress_flag, objs[i].live_flag, objs[i].reuse_flag );

	}
	debug2_print("---------------------------------------------------------\n");
	
}

static float predcit_value(float *a){
	int i; double d=0,p;
	
	for(i=0; (i<N-1 && a[i+1]!='\0') ;i++)
		d += a[i]-a[i+1];
	
	p = (((M/(float)i)*d)+a[0]);
	
	debug3_print("Prediction_val=%f a[0]=%d M=%d i=%d\n",p,a[0],M,i);
	return p;
}

/**
 * Send final object list to DB. Send final object list for visualization if localvisualizationenable flag enabled
 **/
static void send_obj()
{
	int i,j;

	if(localvisualizationenable) init_image();
	
	debug1_print("================ DATA FOR DB =====================\n");
	debug1_print("SectList\tTimeStamp\tObjectId Amplitude Distance\nX_vel Y_vel  X Y Next_X Next_Y \t\tscancnt - maxdisapcnt - update - inprogress - live - reuse\n");
	for(i=0; objs[i].sect_list[0] != 0; i++){
		if( objs[i].live_flag == 1 && objs[i].update_flag == 1 )
		{
			if (predictionenable) {
				objs[i].nxt_x=predcit_value(objs[i].X);
				objs[i].nxt_y=predcit_value(objs[i].Y);
			} else {
				objs[i].nxt_x=objs[i].x;
				objs[i].nxt_y=objs[i].y;
			}
			
			for(j=0;objs[i].sect_list[j];j++)
				debug1_print(GREEN "%d " RESET,objs[i].sect_list[j]);
			debug1_print(GREEN "[%lf]\t<%lf> %f %f\t%f %f %f %f %f %f\t%li %li %d %d %d %d\n" RESET,\
				objs[i].timestamp, objs[i].objectID, objs[i].amp, objs[i].dist, objs[i].x_vel, objs[i].y_vel,\
				objs[i].x, objs[i].y, objs[i].nxt_x, objs[i].nxt_y, objs[i].scan_cnt, objs[i].max_disappercnt,\
				objs[i].update_flag, objs[i].inprogress_flag, objs[i].live_flag, objs[i].reuse_flag );
			
			objs[i].max_disappercnt = 0;
			
			memset(msg,'\0',sizeof(msg));
			jsonfrmt_leddar(msg, objs[i].timestamp, sensortype, sensorid, objs[i].objectID, objs[i].type, objs[i].dist, objs[i].amp,\
				objs[i].x, objs[i].y, objs[i].x_vel, objs[i].y_vel, objs[i].nxt_x, objs[i].nxt_y);
			sockfd = create_socket(8080);
			if ( -1 != sockfd ){
				senddata(sockfd, msg);
				closesocket(sockfd);
				sockfd = -1;
			}
			if(localvisualizationenable) put_image(leddar01_id,objs[i].x, objs[i].y, objs[i].nxt_x, objs[i].nxt_y);
		}
	}
	if(localvisualizationenable) get_image();
	debug1_print("==================================================\n");
}

/**
 * Declare object as not-live, if object didn't appear for OBJ_DISAPPEAR_SCANLIMIT period
 **/
static void update_live_flag()
{
	int i;

	for(i=0; objs[i].sect_list[0] != 0; i++){
		if( objs[i].max_disappercnt > OBJ_DISAPPEAR_SCANLIMIT ){		// object didn't appear for OBJ_DISAPPEAR_SCANLIMIT period
			objs[i].live_flag =0;
			objs[i].reuse_flag =1;
		} else if ( objs[i].reuse_flag == 0 && objs[i].live_flag ==0 && objs[i].inprogress_flag == 0 ){	// object appear min for inprogress period and it's reusablity of off
			objs[i].objectID = objectID;  objectID++;
			objs[i].live_flag =1;
		}
	}
}

/**
 * Confirm object as valid if it's been detected for MIN_DECT_SCANLIMIT period
 **/
static void update_inprogress_flag()
{
	int i;

	for(i=0; objs[i].sect_list[0] != 0; i++){
		debug3_print("amp = %f scan_cnt= %li limit=%d....\n",objs[i].amp,objs[i].scan_cnt,MIN_DECT_SCANLIMIT);
		if( objs[i].scan_cnt < MIN_DECT_SCANLIMIT )					// object in progress
			objs[i].inprogress_flag =1;
		else
			objs[i].inprogress_flag =0;								// surelly it's a object
	}
}

/**
 * Increase scan count and max disappear cnt. This will help to invalidate object after it disappear
 **/
static void update_scan_and_disapper_cnt()
{
	int i;

	for(i=0; objs[i].sect_list[0] != 0 && !objs[i].reuse_flag ; i++){
		objs[i].scan_cnt+=1;
		objs[i].max_disappercnt+=1;									//reset to 0 after sending
	}
}

/**
 * Reset update flag. Update flag tell wheater specific object information is updated or not
 **/
static void reset_obj(char *resetprop)
{
	int i;

	if(!strcmp(resetprop, "update")){
		for(i=0; objs[i].sect_list[0] != 0; i++)
			objs[i].update_flag=0;
	}
}

/**
 * Adding object at the beggning
 **/
static void add_obj( struct OBJECT &dest, struct NEW_OBJECT src )			/// adding new node
{
	int k,j;
	
	for(j=0;j<SECCNT;j++)
		dest.sect_list[j] = '\0';	// clean old sector list
	
	dest.timestamp = src.timestamp;
	dest.dist = src.dist;
	dest.amp = src.amp;
	dest.type = src.type;
	dest.x = src.x;
	dest.y = src.y;
	if (predictionenable){
		dest.X[0]= src.x;
		dest.Y[0] = src.y;
	}
	dest.type = src.type;
	for(k=0;src.sect_list[k];k++)
		dest.sect_list[k]= src.sect_list[k];

	dest.reuse_flag =0;
	dest.update_flag=1;
	dest.inprogress_flag =0;
	dest.live_flag=0;
	dest.scan_cnt=0;
	dest.max_disappercnt=0;
}

/**
 * Overwriting invalid object memory. If no invalid object present, then append new object
 **/
static void append_obj( struct NEW_OBJECT src )
{
	int i,j,k;
	
	for(i=0; objs[i].sect_list[0] != 0; i++){
		//if( objs[i].live_flag != 1 && objs[i].inprogress != 1){		// object is not live and not in-progress 
		if( objs[i].reuse_flag ==1 ){
			for(j=0;j<SECCNT;j++)
				objs[i].sect_list[j] = '\0';	// clean old sector list
			
			for(j=0;j<N;j++){
				objs[i].X[j] = '\0';			// clean old X and Y list, used for prediction
				objs[i].Y[j] = '\0';
			}
			
			objs[i].timestamp = src.timestamp;
			objs[i].dist = src.dist;
			objs[i].amp = src.amp;
			objs[i].type = src.type;
			objs[i].x = src.x;
			objs[i].y = src.y;
			if (predictionenable){
				objs[i].X[0]= src.x;
				objs[i].Y[0] = src.y;
			}
			objs[i].type = src.type;
			for(k=0;src.sect_list[k];k++)
				objs[i].sect_list[k]= src.sect_list[k];
			
			objs[i].reuse_flag =0;	
			objs[i].update_flag=1;
			objs[i].inprogress_flag =0;
			objs[i].live_flag=0;
			objs[i].scan_cnt=0;
			objs[i].max_disappercnt=0;
			
			
			i++; break;
		}
	}
	
	if( objs[i].sect_list[0] == 0 )				// no node for reuse, so add new object at the end
	{	
		objs[i].timestamp = src.timestamp;
		objs[i].dist = src.dist;
		objs[i].amp = src.amp;
		objs[i].type = src.type;
		objs[i].x = src.x;
		objs[i].y = src.y;
		if (predictionenable){
			objs[i].X[0]= src.x;
			objs[i].Y[0] = src.y;
		}
		objs[i].type = src.type;
		for(k=0;src.sect_list[k];k++)
			objs[i].sect_list[k]= src.sect_list[k];
			
		objs[i].reuse_flag =0;	
		objs[i].update_flag=1;
		objs[i].inprogress_flag =0;
		objs[i].live_flag=0;
		objs[i].scan_cnt=0;
		objs[i].max_disappercnt=0;
	}
}

static void append_atbeginning(float val, float *a)
{
	int j,i;
	
	for(j=N-2; j>=0; j--)
		a[j+1]=a[j];
	a[0]=val;
}
/**
 *  Updating information of object
 **/
static void copy_obj( struct OBJECT &dest, struct NEW_OBJECT src )
{
	int k,j,i;
	debug3_print("\nCopying objectID = %f\n",dest.objectID);
	for(j=0;j<SECCNT;j++)
		dest.sect_list[j] = '\0';								// clean old sector list

	dest.dist = src.dist;
	dest.amp = src.amp;
	dest.type = src.type;
	dest.type = src.type;
	for(k=0;src.sect_list[k];k++)
		dest.sect_list[k]= src.sect_list[k];
		
	dest.update_flag=1;
	dest.reuse_flag =0;
	
	velocity_cal(dest,  src); 									// This has to be in send_data() function. It will be called if update_flag ==1
	dest.x = src.x;
	dest.y = src.y;
	dest.timestamp = src.timestamp;
	if (predictionenable){
		append_atbeginning(src.x, dest.X);
		append_atbeginning(src.y, dest.Y);
	}
}

/**
 *  Find number of sector match between new object and already stored object
 **/
static int find_matched_sectcnt(int *newsectlist, int* storedsectlist)
{
	int i,j,succnt=0;
	
	for(i=0;newsectlist[i];i++){
		for(j=0;storedsectlist[j];j++){
			if(newsectlist[i]==storedsectlist[j])
				succnt++;
		}
	}
	return succnt;
}

/**
 *  Find new detected object in alredy stored object list
 **/
static void find_update_obj( struct NEW_OBJECT new_obj )				// search for obj (new_obj) in old object list
{
	int i, matchednode_index=-1, current_match_sectcnt=0, curret_distdiff=0, current_ampdiff=0, prv_match_sectcnt=0, prv_distdiff=0, prv_ampdiff=0;
	
	for(i=0; objs[i].sect_list[0] != 0; i++){
		if( !objs[i].update_flag && !objs[i].reuse_flag ){										// if object is not updated before
			current_match_sectcnt = find_matched_sectcnt(new_obj.sect_list, objs[i].sect_list);
			
			if(current_match_sectcnt >=1){
				if( current_match_sectcnt > prv_match_sectcnt ){		// new matched node have more matched sector
					matchednode_index = i;
					prv_match_sectcnt = current_match_sectcnt;
						
				} else if( current_match_sectcnt == prv_match_sectcnt ) {
					curret_distdiff = objs[i].dist - new_obj.dist;					if (curret_distdiff < 0) curret_distdiff*=-1;
					prv_distdiff = objs[matchednode_index].dist - new_obj.dist; 	if (prv_distdiff < 0) prv_distdiff*=-1;
					
					if ( curret_distdiff < prv_distdiff ){ 				// new matched node distance difference from detected object is less
						matchednode_index = i;
						prv_match_sectcnt = current_match_sectcnt;
						
					} else if ( curret_distdiff == prv_distdiff ){
						
						current_ampdiff = objs[i].amp - new_obj.amp;				if (current_ampdiff < 0) current_ampdiff*=-1;
						prv_ampdiff = objs[matchednode_index].amp - new_obj.amp; 	if (prv_ampdiff < 0) prv_ampdiff*=-1;
						
						if ( current_ampdiff < prv_ampdiff ){			// new matched node amplitude difference from detected object is less
							matchednode_index = i;
							prv_match_sectcnt = current_match_sectcnt;
						}
					}
				}
			}
			
		}
	}
	
	if( matchednode_index >=0 ) 										// found matched object- update values
		copy_obj(objs[matchednode_index],new_obj);
	else
		append_obj(new_obj); 											// add new object
}

/**
 *  Compare new detected list object with alredy stored object list to mind matches
 **/
static void object_update()
{
	int i;
	
	if( objs[0].sect_list[0] == 0 ) 		// very first time call
	{
		for(i=0;new_objs[i].new_det;i++)
			add_obj(objs[i],new_objs[i]);	
		
		update_scan_and_disapper_cnt();		// increase scan_cnt and max_disappercnt
		update_inprogress_flag();			// reset inprogress_flag if object detected for MIN_DECT_SCANLIMIT
		update_live_flag();					// set to live if object detected for MIN_DECT_SCANLIMIT and didn't disapper for max OBJ_DISAPPEAR_SCANLIMIT
		send_obj();							// send data to DB
	} else {
		
		reset_obj("update");
		for(i=0;new_objs[i].new_det;i++)	// find new node in old list and update it
			find_update_obj(new_objs[i]);
			
		update_scan_and_disapper_cnt();		// increase scan_cnt and max_disappercnt
		update_inprogress_flag();			// reset inprogress_flag if object detected for MIN_DECT_SCANLIMIT
		update_live_flag();					// set to live if object detected for MIN_DECT_SCANLIMIT and didn't disapper for max OBJ_DISAPPEAR_SCANLIMIT
		send_obj();							// send data to DB
	}
	
	#if DEBUGLEVEL2
		print_objs();
	#endif
}

/********************************************************* ALGO LEVEL 3 : END ************************************************/

/********************************************************* ALGO LEVEL 2 : START ************************************************/

/**
 * Printing member of array of objects of NEW_OBJECT structure
 **/
static void print_new_obj()
{
		debug2_print("********* New Object List *********\n");
		int i,j;
		debug2_print("\tSectList\tTimeStamp\tAmplitude\tDistance\tX\t\tY\tNewDetFlag\n\t");
		for(i=0;new_objs[i].new_det;i++){
			for(j=0;new_objs[i].sect_list[j];j++)
				debug2_print("%d ",new_objs[i].sect_list[j]);
			debug2_print("\t[%lf]\t%f\t%f\t%f\t%f\t%d\n", new_objs[i].timestamp,\
			 new_objs[i].amp, new_objs[i].dist, new_objs[i].x, new_objs[i].y, new_objs[i].new_det);
		}
}

/**
 * Reset member flag of "new_object" array of object of structure NEW_OBJECT
 **/
static void reset_new_obj(struct NEW_OBJECT new_obj[], char *resetprop)
{
		int i,j;
		if(!strcmp(resetprop, "newdet")){
			for(i=0;new_obj[i].new_det;i++)
				new_obj[i].new_det=0;
		}
		
		if(!strcmp(resetprop, "sectlist")){				// reset sectlist while ned_det flag is set
			for(i=0;new_obj[i].new_det;i++)
				for(j=0;j<SECCNT;j++)
					new_objs[i].sect_list[j] = '\0';
				//memset(new_obj[i].sect_list, '\0', SECCNT);
		}
		
		if(!strcmp(resetprop, "impval")){
			for(i=0;new_obj[i].new_det;i++){
				new_objs[i].dist= 0;
				new_objs[i].amp= 0;
				new_objs[i].x= 0;
				new_objs[i].y= 0;
			}
		}
		
}

/**
 * Combine detections of multiple sector in order to detect valid objects
 **/
static void new_obj_detection()
{
	int a,i,j,k,m,nwo_index=0;
	float dist_d;
	OBJ_TYPE type=Unknown;
	
	reset_new_obj(new_objs,"impval");										// reset dist, amp, x,y
	reset_new_obj(new_objs,"sectlist");										// clean array list, before resetting new_det
	reset_new_obj(new_objs,"newdet");										// reset newdet flag for old objects
	
	for(i=0;nonfix_sect_obj[i].new_det;i++)									// current object is new object
	{
		if( nonfix_sect_obj[i].fix_obj == 0 )								// current object is not fix object
		{			
			for(j=0; ( (nonfix_sect_obj[i+1].sect - nonfix_sect_obj[i].sect) ==1 && nonfix_sect_obj[i+1].fix_obj == 0 && nonfix_sect_obj[i+1].new_det == 1) ;j++,i++){  // next object is new object and not a fix object

				dist_d = nonfix_sect_obj[i+1].dist - nonfix_sect_obj[i].dist;	if(dist_d <0) dist_d*=-1;
				debug3_print("i = %d j=%d nwo_index=%d dist_d=%f\n",i,j,nwo_index,dist_d);
				if ( HUMAN_SECTORDIFF >=  dist_d ){	 						// dis less then 0.3
					type = Human;
					debug3_print("Human Detecected\n");
				}else 	 													// next object is not same
					break;
			}
		
			if(j==0){														//j+1:number of sector for same object
				new_objs[nwo_index].dist= nonfix_sect_obj[i].dist;
				new_objs[nwo_index].amp= nonfix_sect_obj[i].amp;
				new_objs[nwo_index].x= nonfix_sect_obj[i].x;
				new_objs[nwo_index].y= nonfix_sect_obj[i].y;
				new_objs[nwo_index].sect_list[0]= nonfix_sect_obj[i].sect;
				new_objs[nwo_index].timestamp=nonfix_sect_obj[i].timestamp;
				new_objs[nwo_index].new_det=nonfix_sect_obj[i].new_det;
				if(nonfix_sect_obj[i].dist <10 && nonfix_sect_obj[i].amp >100) ///////temp : change when get better logic///////
					new_objs[nwo_index].type = Human;
				else
					new_objs[nwo_index].type = type;
			} else {
				for(k=0;k<j+1;k++){
					new_objs[nwo_index].type = type;
					new_objs[nwo_index].dist+= nonfix_sect_obj[i-k].dist;
					new_objs[nwo_index].amp+= nonfix_sect_obj[i-k].amp;
					new_objs[nwo_index].x+= nonfix_sect_obj[i-k].x;
					new_objs[nwo_index].y+= nonfix_sect_obj[i-k].y;
					new_objs[nwo_index].new_det=nonfix_sect_obj[i].new_det;
					new_objs[nwo_index].timestamp=nonfix_sect_obj[i].timestamp;					// timestamp of the last point when it's detectected in last sector of sector list
					new_objs[nwo_index].sect_list[k]= nonfix_sect_obj[i-k].sect;
				}
				debug3_print("i=%d j=%d, k=%d, nwo_index=%d \n",i,j, k, nwo_index);
				new_objs[nwo_index].dist=(new_objs[nwo_index].dist/k);							//averaging multiple sector value for single object
				new_objs[nwo_index].amp=(new_objs[nwo_index].amp/k);
				new_objs[nwo_index].x=(new_objs[nwo_index].x/k);
				new_objs[nwo_index].y=(new_objs[nwo_index].y/k);
			}	
			
			#if DEBUGLEVEl3
			for(m=0;new_objs[nwo_index].sect_list[m];m++)
				debug3_print("%d ",new_objs[nwo_index].sect_list[m]);
			debug3_print("\t%lf\t%f\t%f\t%f\t%f\t%d\n", new_objs[nwo_index].timestamp,\
				new_objs[nwo_index].amp, new_objs[nwo_index].dist, new_objs[nwo_index].x, new_objs[nwo_index].y, new_objs[nwo_index].new_det);
			#endif
			
			nwo_index++;
		}
	}
}
/********************************************************* ALGO LEVEL 2 : STOP ************************************************/

/********************************************************* ALGO LEVEL 1 : START ************************************************/

/**
 * Printing member  of array of objects of SECT_OBJECT structure
 **/
static void print_sect_obj(struct SECT_OBJECT sect_obj[])
{
		int i;
		debug2_print("TimeStamp\t\tSect\tAmplitude\tDistance\tX\t\tY\tNewDetFlag\tFixObjFlag\n");
		for(i=0;sect_obj[i].new_det;i++)
			debug2_print("%lf\t%d\t%f\t%f\t%f\t%f\t%d\t%d\n", sect_obj[i].timestamp, sect_obj[i].sect, sect_obj[i].amp, sect_obj[i].dist, \
				sect_obj[i].x, sect_obj[i].y, sect_obj[i].new_det, sect_obj[i].fix_obj );

}

/**
 * Find coordinates for each object detected in each sector.
 * Modifing member of "sect_obj" object of SECT_OBJECT structure
 **/
static void fill_coordinates()
{
		int i;
		for(i=0;sect_obj[i].new_det;i++)
			get_coordinate(sect_obj[i].sect, sect_obj[i].dist, &sect_obj[i].x, &sect_obj[i].y);
}

/**
 * Reset newdet and fixobj member of objects of SECT_OBJECT structure
 **/
static void reset_sect_obj(struct SECT_OBJECT sect_obj[], char *resetprop)
{
		int i;
		if(!strcmp(resetprop, "newdet")){
			for(i=0;sect_obj[i].new_det;i++)
				sect_obj[i].new_det=0;
		}
		
		if(!strcmp(resetprop, "fixobj")){
			for(i=0;sect_obj[i].new_det;i++)
				sect_obj[i].fix_obj=0;
		}		
}

/**
 * Clonning SECT_OBJECT structure object
 **/
static void cpy_sect_obj(struct SECT_OBJECT *dest, struct SECT_OBJECT *src)
{
		int i;
		for(i=0;src[i].new_det;i++){
				dest[i].sect = src[i].sect;		// 1 to 16
				dest[i].amp = src[i].amp;			//printf("<%f> ", amp[index]);
				dest[i].dist = src[i].dist;		//printf("<<%f>> ", dist[index]);	
				dest[i].new_det	= src[i].new_det;
				dest[i].timestamp = src[i].timestamp;	// in microsecond
				dest[i].x = src[i].x;
				dest[i].y = src[i].y;
		}	
}

/**
 * Printing member of "fix_obj" array of object of SECT_OBJECT structure
 **/
static void print_fix_obj()
{
		int i;
		debug2_print("\tTimeStamp\tSect\tAmplitude\tDistance\tX\t\tY\tNewDetFlag\tFixObjFlag\n");
		for(i=0;i<SECCNT;i++)
			debug2_print("\t%lf\t%d\t%f\t%f\t%f\t%f\t%d\t%d\n", fix_obj[i].timestamp, fix_obj[i].sect, fix_obj[i].amp, fix_obj[i].dist, fix_obj[i].x, fix_obj[i].y, fix_obj[i].new_det, fix_obj[i].fix_obj );

}

/**
 * Reset fix_object flag 
 **/
static void reset_fix_obj(char *resetprop)
{
		int i;
		if(!strcmp(resetprop, "newdet")){
			for(i=0;i<SECCNT;i++)
				fix_obj[i].new_det=0;
		}
}

/**
 * Create non fix object list.
 * Copy non fix object from "sect_obj" object list to "nonfix_sect_obj" object list on bases of "fix_obj" flag
 **/
static void update_nonfixsectobj()
{
	
	int i,j,k;
	reset_sect_obj(nonfix_sect_obj,"newdet");
	
	for(i=0,j=0;sect_obj[i].new_det;i++){
		
		for( k = 0; nonfix_sect_obj[k].new_det; k++){				// to avoid 2nd onward detectected object in sector 
			if( sect_obj[i].sect == nonfix_sect_obj[k].sect )
				break;
		}
		
		if(sect_obj[i].fix_obj == 0 && k == j){						// to only copy first detectected object in sector
			
			nonfix_sect_obj[j].sect = sect_obj[i].sect;				// 1 to 16
			nonfix_sect_obj[j].amp = sect_obj[i].amp;				//printf("<%f> ", amp[index]);
			nonfix_sect_obj[j].dist = sect_obj[i].dist;				//printf("<<%f>> ", dist[index]);	
			nonfix_sect_obj[j].x = sect_obj[i].x;
			nonfix_sect_obj[j].y = sect_obj[i].y;
			nonfix_sect_obj[j].fix_obj	= sect_obj[i].fix_obj;
			nonfix_sect_obj[j].new_det	= sect_obj[i].new_det;
			nonfix_sect_obj[j].timestamp = sect_obj[i].timestamp;	// in microsecond
			j++;
		}
	}
}

/**
 * Update "fix_obj" list after FIX_OBJ_UPDATE_INTERVAL period.
 * "temp_sect_obj" store pervious "sect_obj". After FIX_OBJ_UPDATE_INTERVAL period, new "sect_obj" is compared with "temp_sect_obj".
 * The detections which didn't move in FIX_OBJ_UPDATE_INTERVAL period, will be stored in "fix_obj" array of object.
 **/
static void update_fixobj()
{
		int i=0,j=0;
		
		if( obj_update_cnt == FIX_OBJ_UPDATE_INTERVAL || obj_update_cnt == -1){ 
			
			if( temp_sect_obj[0].sect == 0 ){ 										//It's the first time on start up
				debug2_print("*****Creating Fixed object DB for the first time******\n");
				cpy_sect_obj( temp_sect_obj, sect_obj);
				debug2_print(">>>>Temp sect obj list <<<<\n");
				print_sect_obj(temp_sect_obj);
			
			} else {
				#if DEBUGLEVEL2
					debug2_print("******Update Fixed object DB******\n");				
					debug2_print(">>>>Temp sect obj list <<<<\n");
					print_sect_obj(temp_sect_obj);
				#endif
				
				reset_fix_obj("newdet");
				
				for(i=0;sect_obj[i].new_det;i++)
				{
					j=0;
					while( temp_sect_obj[j].new_det && sect_obj[i].sect >= temp_sect_obj[j].sect ) /// make new_det = 0  before copying new data
					{
						if( sect_obj[i].sect == temp_sect_obj[j].sect)
						{
							if( FIX_OBJ_AMP_VAR > fabs( sect_obj[i].amp - temp_sect_obj[j].amp  ) && FIX_OBJ_DIS_VAR > fabs( sect_obj[i].dist - temp_sect_obj[j].dist ) ){
									fix_obj[temp_sect_obj[j].sect-1].amp =temp_sect_obj[j].amp ;
									fix_obj[temp_sect_obj[j].sect-1].dist =temp_sect_obj[j].dist ;
									fix_obj[temp_sect_obj[j].sect-1].sect =temp_sect_obj[j].sect ;
									fix_obj[temp_sect_obj[j].sect-1].x =temp_sect_obj[j].x ;
									fix_obj[temp_sect_obj[j].sect-1].y =temp_sect_obj[j].y ;
									fix_obj[temp_sect_obj[j].sect-1].new_det =temp_sect_obj[j].new_det;
							}	
						}
						j++;
					}

				}
			
				reset_sect_obj(temp_sect_obj,"newdet");	// to avoid old data in temp_sect_obj
				cpy_sect_obj(temp_sect_obj, sect_obj);
				
				debug2_print(">>>>Fixed sect obj list <<<<\n");
				#if DEBUGLEVEL2
					print_fix_obj();
				#endif
				reset_sect_obj(sect_obj,"fixobj");		// set fix_obj flag for stationary object in recent scan
				
				debug2_print("<Marking stationary object in new object scan list>\n");
				for(i=0;i<SECCNT;i++)
				{
					j=0;
					while( sect_obj[j].new_det && fix_obj[i].sect >= sect_obj[j].sect) /// make new_det = 0  before copying new data
					{
						if( fix_obj[i].sect == sect_obj[j].sect)
						{
							if( FIX_OBJ_AMP_VAR > fabs( fix_obj[i].amp - sect_obj[j].amp  ) && FIX_OBJ_DIS_VAR > fabs( fix_obj[i].dist - sect_obj[j].dist ) ){
									sect_obj[j].fix_obj=1;
							}	
						}
						j++;
					}

				}
			}
			obj_update_cnt=0;
			
		} else if( fix_obj[0].sect != 0) {
			debug2_print("<<Marking stationary object in new object scan list>>\n");
				
			for(i=0;i<SECCNT;i++)
			{
				j=0;
				while( sect_obj[j].new_det && fix_obj[i].sect >= sect_obj[j].sect) /// make new_det = 0  before copying new data
				{
					if( fix_obj[i].sect == sect_obj[j].sect)
					{
						if( FIX_OBJ_AMP_VAR > fabs( fix_obj[i].amp - sect_obj[j].amp  ) && FIX_OBJ_DIS_VAR > fabs( fix_obj[i].dist - sect_obj[j].dist ) ){
								sect_obj[j].fix_obj=1;
						}	
					}
					j++;
				}

			}
		}
		obj_update_cnt++;
		#if DEBUGLEVEL2
			debug3_print(">>>>Sect obj list <<<<\n");
			print_sect_obj(sect_obj);
		#endif
		debug2_print("%d Update done !!\n",obj_update_cnt);
}

/********************************************************* ALGO LEVEL 1 : STOP ************************************************/

void leddar_algo()
{
	int i=0;
	float x,y;

	reset_sect_obj(sect_obj,"fixobj");			// reset fix_obj flag. fix_obj flag will be set after comparing with fix_obj list
	
	fill_coordinates();							// find coordinates of new detected items in sector
	#if DEBUGLEVEL2
		print_sect_obj(sect_obj);
	#endif
	
	update_fixobj();							// update fix object for each sector
	update_nonfixsectobj();						// create non fix item ( new detected obj) list 
	#if DEBUGLEVEL2
		debug2_print(">>>>>>>>>> Non fixed sect object <<<<<<<<<<<<\n");
		print_sect_obj(nonfix_sect_obj);
	#endif
	
	new_obj_detection();						// combine sector detection, to make a valid object	
	#if DEBUGLEVEL2
		print_new_obj();
	#endif
	
	object_update();							// compare new detected object with pervious object list

	reset_sect_obj(sect_obj,"newdet");			// Reset new_obj flag. So in next round of detection new detected data can be distigush from old object
}

/**
 * To test with raw data.
 * Read data from file.
 **/
int read_csv(char *line, char *delim, float *amp, float *dist, int *sect)
{

	int j = 0; char delim_n[3]; char* tmp = strdup(line); const char* tok; 
	
	strcpy(delim_n,delim);
	strcat(delim_n,"\n");
	    
	for (tok = strtok(line, " "); tok && *tok; j++, tok = strtok(NULL, " \n"))
	{
		if(j==sect_col){
			*sect = atoi(tok)+1;		// 1 to 16
		}else if(j==amp_col){
			*amp = atof(tok);			debug3_print("<%f> ", amp[index]);
		}else if(j==dist_col){
			*dist = atof(tok);			debug3_print("<<%f>> ", dist[index]);
		}else
			atof(tok);
	}
	
    free(tmp);
    return *sect;
}

void record_data(int index, float amp, float dist, int sect)
{
		sect_obj[index].sect = sect;		// 1 to 16
		sect_obj[index].amp = amp;			debug3_print("<%f> ", amp[index]);
		sect_obj[index].dist = dist;		debug3_print("<<%f>> ", dist[index]);	
		sect_obj[index].new_det=1;
		sect_obj[index].timestamp= get_micro();	// in microsecond
}

static void init_localbuffer()
{
	int i,j;
	
	for(i=0;i< (sizeof(sect_obj) / sizeof(sect_obj[0]));i++){
			sect_obj[i]={0};
	}
	
	for(i=0;i< (sizeof(temp_sect_obj) / sizeof(temp_sect_obj[0]));i++){
			temp_sect_obj[i]={0};
	}
	
	for(i=0;i< (sizeof(nonfix_sect_obj) / sizeof(nonfix_sect_obj[0]));i++){
			nonfix_sect_obj[i]={0};
	}
	
	for(i=0;i< (sizeof(fix_obj) / sizeof(fix_obj[0]));i++){
			fix_obj[i]={0};
	}
	
	for(i=0;i< (sizeof(new_objs) / sizeof(new_objs[0]));i++){
			new_objs[i]={0};
	}
	
	for(i=0;i< (sizeof(objs) / sizeof(objs[0]));i++){
			objs[i]={0};
	}
}

int leddar_init()
{
	/*sockfd = create_socket(8080);
    if ( -1 == sockfd ){
		return -1;
	}*/
	
	configuration();
	init_localbuffer();
	if(localvisualizationenable)	init_visualization();
	return 0;
}

void leddar_free()
{	
	closesocket(sockfd);
}


#if testwithfile
int main(int argc, char const *argv[])
{
	if (argc < 2){
		printf("Please specify the CSV file as an input.\n");
		exit(0);
	}
	
	int i,sect_n,sect,index=0; float dist, amp; char line[50],fname[256];	FILE *file;
	char delim[3]= " ";
	
	/*sockfd = create_socket();
    if ( -1 == sockfd ){
		return 0;
	}*/
	
	configuration();
	init_localbuffer();
	if(localvisualizationenable)	init_visualization();
	
	strcpy(fname, argv[1]);
	file = fopen(fname, "r");
	if(!file){
		printf("%s file not present\n",fname);
		return 0;
	}

    memset(line,'\0',sizeof(line));
			    
	while (fgets(line, 50, file) ){
		sect_n=read_csv(line, delim, &amp, &dist, &sect);
		record_data(index, amp, dist, sect);
		
		if(sect_n == SECCNT){ 						// data collection for single scan completed
			while(sect_n == SECCNT){				// to detect multiple last sector values
				memset(line,'\0',sizeof(line));
				if(!fgets(line, 50, file))
					break;
				sect_n=read_csv(line, delim, &amp, &dist, &sect);
				if(sect_n == SECCNT){				// if sector 1 came then break 
					index++;
					record_data(index, amp, dist, sect);
				}
			}
			
			#if DEBUGLEVEL3
			print_sect_obj(sect_obj);
			#endif
			
			leddar_algo();	
			debug3_print(".............End of a scan loop .............\n");

			index = 0;
			record_data(index, amp, dist, sect);	// store sector 1 value
		} 
		index++;
		memset(line,'\0',sizeof(line));
	}
	debug3_print(".................END................\n");
	fclose(file);	
	//closesocket(sockfd);
	return 0;
}
#endif
