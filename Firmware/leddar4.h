#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */

#define PI 3.14159265
//#define MIN_SECT_VAR 1

typedef enum {Unknown,Human,Fourwheeler,TwoWheeler} OBJ_TYPE;
typedef enum {UNKNOWN,W,SWW,SWS,S,SES,SEE,E,NEE,NEN,N,NWN,NWW} OBJ_DIR;

void leddar_algo();
int read_csv(char *line, char *delim, float *amp, float *dist, int *sect);
void record_data(int index, float amp, float dist, int sect);
static void init_localbuffer();
void leddar_free();
int leddar_init();

