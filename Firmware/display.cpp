#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <string.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <jsoncpp/json/json.h> 
#include <fstream>

#include "display.h"

#define testvisualization 0
#define globalcoordinateactive 1
#define PI 3.14159
#define origin_x (img_l/4)
#define origin_y (img_w/2)
#define y_slot ((img_w/2)/range)
#define x_slot (((img_l/2)+(img_l/4))/range)

using namespace cv;
using namespace std;

float x_pix, y_pix, ang_r, x,y, leddar1_ang, leddar2_ang;
int i,j,img_l, img_w, dev_size, arrow_len, grid_angle_slot, range;
Mat backgrdimg;
Mat img;
Scalar l01_color = Scalar( 0, 0, 255 );
Scalar l02_color = Scalar( 0, 255, 255 );

static void configuration();
string GetStdoutFromCommand(string cmd);
static void image_resolution();
static void get_lable(char* sensor_id, int ang);
static void get_infrastructure ();
static void plot_obj(int leddar_id, float x, float y, int obj_postype);
static void crdtrn(float xp,float yp,float angle,float x0,float y0, float *x1, float *y1);

namespace patch
{
    template < typename T > std::string to_string( const T& n )
    {
        std::ostringstream stm ;
        stm << n ;
        return stm.str() ;
    }
}

static void configuration()
{
	Json::Value root;   				// starts as "null"; will contain the root value after parsing
	Json::Reader reader;
	
	std::ifstream userconfigfile("user_config.json", std::ifstream::binary);
	userconfigfile >> root;
	
	const Json::Value leddar_dev = root["display"];
	for ( int index = 0; index < leddar_dev.size(); ++index )  // Iterates over the sequence elements.
	{
		if( (leddar_dev[index]["device_id"].asString()).compare("leddar_01") == 0 )
		{	leddar1_ang = leddar_dev[index]["angleofrotation"].asFloat();
			//std::cout << leddar1_ang <<"\n";
		}
		if( (leddar_dev[index]["device_id"].asString()).compare("leddar_02") == 0 )
		{	leddar2_ang = leddar_dev[index]["angleofrotation"].asFloat();
			//std::cout << leddar2_ang <<"\n";
		}
	}	
	
	std::ifstream devconfigfile("dev_config.json", std::ifstream::binary);
	devconfigfile >> root;
	
	dev_size = root["display_infrastructure"]["leddar_range"].asInt();
	arrow_len = root["display_infrastructure"]["arrow_len"].asInt();
	grid_angle_slot = root["display_infrastructure"]["grid_angle_slot"].asInt();
	range = root["display_infrastructure"]["leddar_range"].asInt();
}

string GetStdoutFromCommand(string cmd)
{

    string data;
    FILE * stream;
    const int max_buffer = 256;
    char buffer[max_buffer];
    cmd.append(" 2>&1");

    stream = popen(cmd.c_str(), "r");
    if (stream) {
		while (!feof(stream))
			if (fgets(buffer, max_buffer, stream) != NULL) data.append(buffer);
		pclose(stream);
    }
    return data;
}

static void image_resolution()
{
	string res; char *ptr;
	
	res=GetStdoutFromCommand("xdpyinfo | awk '/dimensions:/ { print $2; exit }'");
	//cout << "data: " << res << endl;
	
	char * res_str = new char [res.length()+1];
	strcpy (res_str, res.c_str());
	
	ptr = strchr(res_str,'x');
	img_w = atoi(ptr+1)-100;
	img_l = img_w;
	printf("Image resolution %d x %d \n",img_l,img_w);
	backgrdimg = Mat::zeros( img_w, img_l, CV_8UC3 );
}
 
static void get_lable(char* sensor_id, int ang)
{
	float s_x,s_y,e_x,e_y;
	
	if( ang == 0){
		s_x	= origin_x;	s_y	= origin_y-arrow_len;	e_x	= origin_x+arrow_len;	e_y	= origin_y-arrow_len ;
	} else if ( ang == 90){
		s_x	= origin_x+arrow_len;	s_y	= origin_y;		e_x	= origin_x+arrow_len;	e_y	= origin_y+arrow_len ;
	} else if ( ang == 180){
		s_x	= origin_x;	s_y	= origin_y+arrow_len;	e_x	= origin_x-arrow_len;	e_y	= origin_y+arrow_len ;
	} else if ( ang == 270){
		s_x	= origin_x-arrow_len;	s_y	= origin_y;		e_x	= origin_x-arrow_len;	e_y	= origin_y-arrow_len ;
	}
	
	arrowedLine(img, Point( s_x, s_y ), Point( e_x, e_y ), Scalar( 100, 155, 25 ), 2, 4, 0, 0.3);     	 
	putText(img, sensor_id, Point( e_x, e_y ), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0,200,200), 1);
}

static void get_infrastructure ()
{	
	rectangle( img, Point( origin_x-dev_size, origin_y-dev_size-2 ), Point( origin_x+dev_size, origin_y+dev_size+2 ), Scalar( 100, 155, 25 ), -1, 8 );

	Point x_p1(0,origin_y), x_p2(img_l,origin_y), y_p1(origin_x,0), y_p2(origin_x,img_w);                           
  
	LineIterator x_it(img, x_p1, x_p2, 8);            // get a line iterator
	for( i = 0; i < x_it.count; i++,x_it++)
		if ( i%5!=0 ) {(*x_it)[0] = 200;}         	  // every 5'th pixel gets dropped, blue stipple line

	LineIterator y_it(img, y_p1, y_p2, 8);           
	for( i = 0; i < y_it.count; i++,y_it++)
		if ( i%5!=0 ) {(*y_it)[0] = 200;}    
	
	for(i=0;i<360/grid_angle_slot;i++){
		ang_r = ((PI/180) * (i*grid_angle_slot));
		y = ((float)img_l * sin(ang_r)) + origin_y;
		x = (float)img_l * cos(ang_r) + origin_x;
		
		LineIterator it(img, Point( origin_x, origin_y ), Point( x, y ), 8);           
		for( j = 0; j < it.count; j++,it++)
			if ( j%5!=0 || j%4!=0 || j%3!=0 ) {(*it)[0] = 200;} 
	}
	
	for( i=0;i<range+5;i++)
		circle( img, Point( origin_x, origin_y ), ((i*x_slot) +x_slot), Scalar( 255, 255, 255 ), 0.1, 8 );
	
	get_lable("Leddar_01",leddar1_ang);	
	get_lable("Leddar_02",leddar2_ang);	
}

static void plot_obj(int leddar_id, float x, float y, int obj_postype)
{
	x_pix= origin_x + (x_slot * x);
	y_pix= origin_y - (y_slot * y); 										// changing signed just for display purpose
	
	if (obj_postype == cur_pos) {
		if( leddar_id == 1) {
			circle( img, Point( x_pix, y_pix ), 5, l01_color, -1, 8 );
			std::string c_cord = "(" + patch::to_string(x) + " , " + patch::to_string(y) +")";
			//putText(img, c_cord , Point( x_pix, y_pix ), FONT_HERSHEY_SIMPLEX, 0.4, l01_color, 1);
		} else if ( leddar_id == 2) {
			circle( img, Point( x_pix, y_pix ), 5, l02_color, -1, 8 );
			std::string c_cord = "(" + patch::to_string(x) + " , " + patch::to_string(y) +")";
			//putText(img, c_cord , Point( x_pix, y_pix ), FONT_HERSHEY_SIMPLEX, 0.4, l02_color, 1);
		}
	} else {
		circle( img, Point( x_pix, y_pix ), 7, Scalar( 224, 224, 224 ), -1, 8 );
		std::string f_cord = "(" + patch::to_string(x) + " , " + patch::to_string(y) +")";
		//putText(img, f_cord , Point( x_pix, y_pix ), FONT_HERSHEY_SIMPLEX, 0.4, Scalar( 224, 224, 224 ), 1);
	}
}

static void crdtrn(float xp,float yp,float angle,float x0,float y0, float *x1, float *y1)
{
	double phi,r;
	
	r = sqrt( pow(xp,2) + pow(yp,2) );
	phi = atan2 (yp,xp);						// in radian
	*x1 = x0 + r*cos(phi - (angle * PI/180.0));
	*y1 = y0 + r*sin(phi - (angle * PI/180.0));
}

#if globalcoordinateactive
void put_image(int leddar_id, float  c_x, float  c_y, float  f_x, float  f_y)	// global co-ordinate system
{
	printf("Dispaly: %f %f %f %f \n",c_x,c_y,f_x,f_y);
	float x,y;
	
	if( leddar_id == leddar01_id) {
		crdtrn(f_x,f_y,leddar1_ang,0,0,&x,&y);
		plot_obj(leddar_id,x, y, futr_pos);
		
		crdtrn(c_x,c_y,leddar1_ang,0,0,&x,&y);
		plot_obj(leddar_id, x, y, cur_pos);
		
	} else if ( leddar_id == leddar02_id) {
		//crdtrn(f_x,f_y,leddar2_ang,0,0,&x,&y);
		//plot_obj(leddar_id,x, y, futr_pos);
		
		crdtrn(c_x,c_y,leddar2_ang,0,0,&x,&y);
		plot_obj(leddar_id, x, y, cur_pos);
	} 
}
#else
void put_image(int leddar_id, float  c_x,float  c_y, float  f_x,float  f_y)		// local co-ordinate system
{
	//plot_obj(leddar_id, f_x, f_y, futr_pos);
	plot_obj(leddar_id, c_x, c_y, cur_pos);
}
#endif

void get_image()
{
	imshow("Image",img);
	waitKey( 1 );
}

void init_image()
{
	img = backgrdimg.clone();
	get_infrastructure();
}

void init_visualization()
{
	configuration();
	image_resolution();									// Create black empty images
}

#if testvisualization
int main( )
{     
	int i=12; 
	init_visualization();
	while(1) 
	{		
		if(i<0)
			i=12;
			
		init_image();
		put_image(leddar01_id, (1*i), (1*i), (1*(i-0.3)), (1*(i-0.3)));
		get_image();
		
		i--;
		
		if( waitKey( 500 ) == 113 )						// decimal value for 'q' is 113
			break;
	}
	
	return(0);
}
#endif
