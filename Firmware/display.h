#define cur_pos 0					//current position
#define futr_pos 1					//future position
#define leddar01_id 1
#define leddar02_id 2

void init_visualization();			// call only once for creating visualization canvas and configuration 
void init_image();					// call every time before ploating object to clone canvas
void put_image(int leddar_id, float  c_x,float  c_y, float  f_x,float  f_y);		// put point on canvas
void get_image();					// display final image
