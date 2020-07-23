// MultiChannelLibModbusDemo.cpp: Sample that uses the open source libmodbus library.
#include <stdio.h>
#include <string.h>
#include <thread>
#include "extern/libmodbus-3.0.6/src/modbus.h"
#include "HighResClock.hpp"
#include <atomic>
#include <math.h>
#include "leddar4.h"
// ***********************************************************************************************
// MACROS
// ***********************************************************************************************

#define LEDDAR_MAX_DETECTIONS 1*16					 // limited to one per channel in this example

#define GETS_S(str) { \
	fgets(str, sizeof(str), stdin); \
	if (strlen(str) > 0) str[strlen(str)-1] = 0; }

#ifndef _MSC_VER
typedef unsigned int UINT;
#endif

#ifndef MAX
#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#endif

#define FAR 3.0
#define MIDDLE 2.0
#define NEAR 1.0
#define ALARM 0.3

#define BEAMWIDTH 45
#define SECCNT 16
//#define PI 3.141592
// ***********************************************************************************************
// TYPES
// ***********************************************************************************************

struct SDetection
{
	UINT channel;							 // Channel number (0..15)
	double dDistance;						 // distance from the sensor, in meters
	double dAmplitude;						 // signal amplitude
};


// ***********************************************************************************************
// GLOBAL VARIABLES
// ***********************************************************************************************

std::atomic <int>CommandBuf(0);
std::atomic <int>SoundPlay(0);
std::atomic <int>SoundFile(0);
modbus_t* mb = nullptr;

///float sectwidth = BEAMWIDTH/SECCNT; 
///float offsetwidth = 90-(BEAMWIDTH/2);

// ***********************************************************************************************
// THREADS
// ***********************************************************************************************

static void readKeyboard(){
  char Buf[256] = "";
  printf("Enter:\n");
  printf("\"exit\"    - to stop\n");
  printf("\"start\"   - to start reading sensor\n");
  for(;;){
    GETS_S(Buf);
    if(!strcmp(Buf, "exit")){
      CommandBuf.store(1, std::memory_order_seq_cst);
      break;
    } else if(!strcmp(Buf, "start")){
      CommandBuf.store(2, std::memory_order_seq_cst);
    } else if(!strcmp(Buf, "alarm1")){
      SoundFile.store(0, std::memory_order_seq_cst);
    } else if(!strcmp(Buf, "alarm2")){
      SoundFile.store(1, std::memory_order_seq_cst);
    }
  }
}

static void playAudio(){
	//system("pulseaudio --start");
	SoundPlay.store(0, std::memory_order_seq_cst);
	for(;;){
		if(CommandBuf.load(std::memory_order_seq_cst) == 1){
      			break;
    		}
		else
		if(SoundPlay.load(std::memory_order_seq_cst) == 1){
			system("sudo i2cset -y -m 0xff 1 0x38 0 8");
			if(SoundFile.load(std::memory_order_seq_cst) == 0){
				system("aplay -D plughw:2,0 /home/nvidia/workspaces/av_workspace/hornhonk.wav");
			} else if(SoundFile.load(std::memory_order_seq_cst) == 1){
				system("aplay -D plughw:2,0 /home/nvidia/workspaces/av_workspace/woopwoop.wav");
			}
			//system("aplay -D plughw:2,0 /home/nvidia/workspaces/av_workspace/bike_horn.wav");
			system("sudo i2cset -y -m 0xff 1 0x38 0 9");
			SoundPlay.store(0, std::memory_order_seq_cst);
		}
	}
}
						

// ***********************************************************************************************
// IMPLEMENTATION
// ***********************************************************************************************
/*
static void coordinate_cal(float dis, int sect, float *x, float *y){
	float ang_rad, ang_deg;

	printf("dis = %f sect = %d\n",dis,sect);
	ang_deg = offsetwidth + ((sect-1)*sectwidth) + (sectwidth/2); // angle in radian
	ang_rad = ((PI/180) * ang_deg);
	*y = dis*sin(ang_rad);
	*x = dis*cos(ang_rad);
	printf("Pi = %f ang_deg = %f ang_rad= %f X = %f Y = %f\n",M_PI,ang_deg,ang_rad,*x,*y);
}
*/
static bool AskSlaveAddr(modbus_t* mb)
{
	/*char str[128];

	printf("Enter slave address: ");
	GETS_S(str); if (str[0] == 0) return false;

	if (modbus_set_slave(mb, atoi(str)) != 0)*/
	if (modbus_set_slave(mb, 1) != 0)
	{
		printf("Error setting slave id\n");
		return false;
	}

	return true;
}

static bool ReadDetections(modbus_t* mb, SDetection tabDetections[LEDDAR_MAX_DETECTIONS],
						   UINT& nbrDetections, UINT& uTimestamp)
{
	nbrDetections = 0;

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// Read 32+2 registers from address 14
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	const UINT NBR_REGISTERS = 32+2;
	uint16_t tabRegValues[NBR_REGISTERS];
	int numRead = modbus_read_input_registers(mb, 14, NBR_REGISTERS, tabRegValues);
	if (numRead == NBR_REGISTERS)
	{
		uTimestamp = tabRegValues[0] + (tabRegValues[1] << 16);

		for (UINT i = 0; i < 16; ++i)
		{
			tabDetections[i].channel = i/16;
			tabDetections[i].dDistance = (double)tabRegValues[2 + i] / 100.0;
			//tabDetections[i].dAmplitude = (double)tabRegValues[2 + (2*i)] / 256.0;
			tabDetections[i].dAmplitude = (double)tabRegValues[18 + i] / 64.0;
		}

		nbrDetections = 16;                    // limit to the first detection (one per segment)

		//printf("Read %i registers:\n", numRead);
		//for (int i = 0; i < numRead; ++i)
		//{
		//	printf("    Register %-2i = %u\n", i, (unsigned int)tabRegValues[i]);
		//}
	}
	else
	{
		//printf("Error reading registers: numRead=%i, errno=%i\n", numRead, errno);
		return false;
	}

	return true;    // success
}
//.load(std::memory_order_seq_cst) == 1) break;
//static void TestOneReading(modbus_t* mb)

static void TestOneReading()
{
	// Shows the last reading.
	SDetection tabDetections[LEDDAR_MAX_DETECTIONS];
	UINT uDetectionCount;
	UINT uTimestamp;
	bool blink = false, blink_start = false;
	bool toofar_start = false, far_start = false, middle_start = false, near_start = false, alarm_start = false;
	std::chrono::system_clock::time_point tp1,tp2;
	std::chrono::duration<double, std::milli> dtn;
	float x,y;

	if (!AskSlaveAddr(mb)) {
		CommandBuf.store(1, std::memory_order_seq_cst);
		return;
	}

	tp1 = std::chrono::system_clock::now();
	tp2 = tp1;

	for(;;){
		if(CommandBuf.load(std::memory_order_seq_cst) == 1){
      			break;
    		} else if(CommandBuf.load(std::memory_order_seq_cst) == 2){

			tp2 = std::chrono::system_clock::now();

			// How long since last blink?
			dtn = std::chrono::duration_cast<std::chrono::milliseconds>(tp2 - tp1);
			if(dtn.count() > 400.0){
				blink = !blink;
				blink_start = false;
				tp1 = std::chrono::system_clock::now();
			}

			if (ReadDetections(mb, tabDetections, uDetectionCount, uTimestamp))
			{
				printf("Timestamp: %u\n", uTimestamp);
				printf("Got %u detections\n", uDetectionCount);

				bool toofar = false, far = false, middle = false, near = false, alarm = false;

				for (UINT i = 0; i < uDetectionCount; ++i)
				{
					if(CommandBuf.load(std::memory_order_seq_cst) == 1) break;
					if(!alarm && tabDetections[i].dDistance < 0.3 && tabDetections[i].dDistance > 0.0 && tabDetections[i].dAmplitude > 90.1){
						alarm = true;
						toofar = false; far = false; middle = false; near = false;
						//printf("should be alarm\n");
					} else
					if(!near && !alarm && tabDetections[i].dDistance > 0.3 && tabDetections[i].dDistance < 1.0 && tabDetections[i].dAmplitude > 125.1){
						near = true;
						toofar = false; far = false; middle = false;
					} else
					if(!near && !middle && tabDetections[i].dDistance > 1.0 && tabDetections[i].dDistance < 2.0/* && tabDetections[i].dAmplitude > 20.1*/){
						middle = true;
						toofar = false; far = false;
					} else
					if(!near && !middle && !far && tabDetections[i].dDistance > 2.0 && tabDetections[i].dDistance < 3.0/* && tabDetections[i].dAmplitude > 0.5*/){
						far = true;
						toofar = false;
					} else
					if(!near && !middle && !far && !toofar){
						toofar = true;
					}
					//coordinate_cal(i,tabDetections[i].dDistance,tabDetections[i].dAmplitude,&x,&y);
					printf("#%-2i: Segment %-2i   Distance = %.3lfm    Amplitude = %.1lf Coordinate: X= %f Y = %f\n", (i + 1), (UINT)tabDetections[i].channel, tabDetections[i].dDistance, tabDetections[i].dAmplitude,x,y);
					record_data(i, tabDetections[i].dAmplitude, tabDetections[i].dDistance, i+1); 
				}
				
				leddar_algo();
				
				if(alarm){
					if(SoundPlay.load(std::memory_order_seq_cst) == 0){
						SoundPlay.store(1, std::memory_order_seq_cst);
						//printf("alarm\n");
					} 
					if(!alarm_start && blink){
						system("sudo i2cset -y -m 0xff 1 0x38 0 3"); // frame red
						alarm_start = true;
					}

					if(blink){
						if(!blink_start){
							system("sudo i2cset -y -m 0xff 1 0x38 0 1"); // background on
							system("sudo i2cset -y -m 0xff 1 0x38 0 7"); // frame off
							blink_start = true;
						}
					} else {
						if(!blink_start){
							system("sudo i2cset -y -m 0xff 1 0x38 0 2"); // background off
							system("sudo i2cset -y -m 0xff 1 0x38 0 3"); // frame red
							blink_start = true;
						}
					}

					toofar_start = false;
					far_start = false;
					middle_start = false;
					near_start = false; //alarm_start = false;
					//alarm_start = false;
				}				
				// NEAR: DANGER
				else
				if(near){
					if(!near_start && blink){
						system("sudo i2cset -y -m 0xff 1 0x38 0 3"); // frame red
						near_start = true;
					}


					if(blink){
						if(!blink_start){
							system("sudo i2cset -y -m 0xff 1 0x38 0 1"); // background on
							system("sudo i2cset -y -m 0xff 1 0x38 0 7"); // frame off
							blink_start = true;
						}
					} else {
						if(!blink_start){
							system("sudo i2cset -y -m 0xff 1 0x38 0 2"); // background off
							system("sudo i2cset -y -m 0xff 1 0x38 0 3"); // frame red
							blink_start = true;
						}
					}

					toofar_start = false;
					far_start = false;
					middle_start = false;
					//near_start = false; //alarm_start = false;
				}
				else
				if(!near && middle){
					if(!middle_start && blink){
						system("sudo i2cset -y -m 0xff 1 0x38 0 6"); // frame on
						middle_start = true;
					}

					if(blink){
						if(!blink_start){
							system("sudo i2cset -y -m 0xff 1 0x38 0 1"); // background on
							system("sudo i2cset -y -m 0xff 1 0x38 0 7"); // frame off
							blink_start = true;
						}
					} else {
						if(!blink_start){
							system("sudo i2cset -y -m 0xff 1 0x38 0 2"); // background off
							system("sudo i2cset -y -m 0xff 1 0x38 0 6"); // frame on
							blink_start = true;
						}
					}

					toofar_start = false;
					far_start = false;
					//middle_start = false;
					near_start = false;
					//system("sudo i2cset -y -m 0xff 1 0x38 0 3"); // frame red
				}
				else
				if(!near && !middle && far){
					if(!far_start && blink){
						system("sudo i2cset -y -m 0xff 1 0x38 0 1"); // background on
						
						far_start = true;
					}

					if(blink){
						if(!blink_start){
							system("sudo i2cset -y -m 0xff 1 0x38 0 6"); // frame on
							system("sudo i2cset -y -m 0xff 1 0x38 0 1"); // background on
							blink_start = true;
						}
					} else {
						if(!blink_start){
							system("sudo i2cset -y -m 0xff 1 0x38 0 7"); // frame off
							system("sudo i2cset -y -m 0xff 1 0x38 0 1"); // background on
							blink_start = true;
						}
					}

					toofar_start = false;
					//far_start = false;
					middle_start = false;
					near_start = false;
				}
				// VERY FAR: all is safe
				else
				if(!far && !near && !middle && toofar && blink){
					if(!toofar_start){
						system("sudo i2cset -y -m 0xff 1 0x38 0 1"); // background on
						system("sudo i2cset -y -m 0xff 1 0x38 0 7"); // frame off
						toofar = true;
					}

					//toofar_start = false;
					far_start = false;
					middle_start = false;
					near_start = false;
					
				}
				
				
			}
			else{
				printf("Error reading registers, errno=%i\n", errno);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			//sleep(1);
		}
	}
}
//.load(std::memory_order_seq_cst) == 1) break;

int main(int argc, char* argv[])
{
	char str[128];
	char sSerialPort[128];
	std::thread ThreadRead;
	std::thread ThreadKbrd;
	std::thread ThreadPlay;

	if( -1 == leddar_init() )
	{
		printf("Leddar DB client init fail\n");
		return -1;
	}
	
	printf("Enter serial port [empty=quit]: ");
	GETS_S(sSerialPort); if (sSerialPort[0] == 0) return 1;

	//modbus_t* mb = nullptr;       // "handle" for the libmodbus library

	// Selects the serial modbus interface:
	mb = modbus_new_rtu(sSerialPort, 115200, 'N', 8, 1);
	if (mb == nullptr)
	{
		printf("Unable to create the libmodbus context\n");
		return 2;
	}

	//modbus_set_debug(mb, true);      // uncomment to view debug info

	// Connects to the sensor:
	if (modbus_connect(mb) != 0)
	{
		modbus_free(mb);
		printf("Connection error\n");
		return 3;
	}

	ThreadKbrd = std::thread(readKeyboard);
	ThreadRead = std::thread(TestOneReading);
	ThreadPlay = std::thread(playAudio);

	while ((true))
	{
		if(CommandBuf.load(std::memory_order_seq_cst) == 1) break;
 	}
  	if(ThreadKbrd.joinable()) ThreadKbrd.join();
  	if(ThreadRead.joinable()) ThreadRead.join();
	if(ThreadPlay.joinable()) ThreadPlay.join();
		/*printf("\n");
		printf("1. Test connection\n");
		printf("2. Read one measurement\n");
		printf("3. Test performance\n");
		printf("4. Side-by-side acquisition\n");
		printf("5. Quit\n");
		printf("\n");

		GETS_S(str);
		if ((str[0] == 0) || (str[0] == '5')) break;

		switch (str[0])
		{
			case '1': TestConnection(mb); break;
			case '2': TestOneReading(mb); break;
			case '3': TestPerformances(mb); break;
			case '4': TestSideBySideSensors(mb); break;
		}

		printf("\n");*/
	//}

	if (mb)
	{
		modbus_close(mb);
		modbus_free(mb);
	}

	return 0;
}

