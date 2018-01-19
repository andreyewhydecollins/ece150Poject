#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

#include <oled-exp.h>
#include <ugpio/ugpio.h>

/*SMART PARKING SYSTEM MACHINE INDEPENDENT SOFTWARE
HANYUAN YE
CHRIS YU
YI FAN YU
ECE 150 1A FALL 2017
*/

#define NUMSPB 3		//Number of Parking Space Boundaries (# of rows)
#define NUMPARKSPACE 11		//Total number of parking spaces in array
#define NUMPSCOL 4		//Number of columns in parking space array
#define NUMPSROW 3		//Number of rows in parking space array
#define NUMENTRANCE 2		//Number of enterances
#define E0 0			//Pin number for Entrance 0
#define E1 6			//Pin number for Entrance 1
#define P0 45			//Pin number for Parking Space 0
#define P1 46			//Pin number for Parking Space 1
#define P2 2			//Pin number for Parking Space 2
#define P4 18			//Pin number for Parking Space 4
#define P5 19			//Pin number for Parking Space 5
#define P6 3			//Pin number for Parking Space 6

//Enumerated types for status of parking spaces
typedef enum status {UNOCCUPIED, RESERVED, OCCUPIED} PSTATUS;

//Structure for positional x, y coordinates defined TYPE POSITION 
typedef struct pos
{
	uint8_t x;
	uint8_t y;
} TPOS;

//Structure for positional corners of desired blocks to be drawn made up of coordinates
//Defined by type TYPE BLOCK
typedef struct block
{
	TPOS startpos;
	TPOS endpos;
} TBLOCK;

TBLOCK startPB[NUMSPB];			//Array of coordinates for the initial NUMSPB boundaries
TBLOCK parkSpacePB[NUMPARKSPACE];	//Array of coordinates for Parking Space Boundaries
TBLOCK parkSpace[NUMPARKSPACE];		//Array of coordinates for Parking Space 'filled' area
TBLOCK entrance[NUMENTRANCE];		//Array of coordinates for entrances
TPOS entPos[NUMENTRANCE];		//Array of the coordinates of the center of the entrances
TPOS psPos[NUMPARKSPACE];		//Array of the coordinates of the center of the Parking Spaces

typedef struct oled
{
	PSTATUS PLArray [NUMPARKSPACE];
	bool ENTArray [NUMENTRANCE];
	uint8_t PLStatus [NUMPARKSPACE];

	uint8_t distance[NUMENTRANCE][NUMPARKSPACE];

	uint8_t tempDist;
	uint8_t tempPS;

	uint8_t spot[NUMENTRANCE];
	int spotCounter[NUMENTRANCE];
} TDISPLAY;

TDISPLAY display;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int LOGLEVEL = 2; //
// 3 means log the action of every single function (debugging)
// 2 means log the action of functions that call other functions and their exits (RECOMMENDED)
// 1 means log only the functions that call other functions and not their exits (RECOMMENDED)
// 0 means no logging (dangerous)
const int TOTAL_SLOTS = 11; //total slot allowed
const int OCCUPIED_STATS = 1; //OCCUPIED_STATS is 1
const int UNOCCUPIED_STATS = 0; //unocuped is 0
int usage = 0; // logs the usage of our system;
int runningUsage = 0; 
const int RANGE = 10;
const int MAX_TIME = 60; // will alert the operator if anyone stays more than MAX_TIME;

//keeps track of the time the program boots up;
time_t lastStat_time;
time_t current_time;
struct tm* current_time_format;
//current_time_format = localtime(&current_time);

typedef struct
{
	int occupy;
	int timeStamp; // when the person arrives
	int timeTotalSpent;
	// when the car shows up at the entrance, the system reserves
}ParkingSpace;

/////////////////////////////////////////////////////////////////////////////////////////
//
//FUNCTIONS SIGNATURE
void eventLog(const char name[], const int loggingRequirement);



void addTime(FILE* outputName, struct tm* myTimeStruc, int ret){
    
    if (ret){
        fprintf (outputName, " %d-%02d-%02d %02d:%02d:%02d\n",(myTimeStruc -> tm_year)+1900, myTimeStruc -> tm_mon,
			    myTimeStruc -> tm_mday,myTimeStruc -> tm_hour,myTimeStruc -> tm_min,
			    myTimeStruc -> tm_sec);
    } else {
        fprintf (outputName, "%d-%02d-%02d %02d:%02d:%02d ",(myTimeStruc -> tm_year)+1900, myTimeStruc -> tm_mon,
			    myTimeStruc -> tm_mday,myTimeStruc -> tm_hour,myTimeStruc -> tm_min,
			    myTimeStruc -> tm_sec);
    }
    return;
			    
}

void bootUp(ParkingSpace* map)
	// maybe allocate memory to all the parking spots and reset them to zero or
	// read from a file that contains the information in case the program is restarting;
	// even logs to the right files since we do not want to have a big fat log file for weeks
{
    eventLog("bootUp", 1);
	time(&current_time);
	time(&lastStat_time); // initialize the stats timer;
	printf("bootup initiated at time%ld\n",lastStat_time);
	current_time_format = localtime(&current_time);
	FILE* fnBootup;
	
	fnBootup = fopen("records.txt", "a");
	fprintf(fnBootup, "Bootup recorded at");
	addTime(fnBootup, current_time_format,1);
			
	fclose(fnBootup);
	//open stats
	fnBootup = fopen("stats.txt", "a");
	fprintf(fnBootup, "Bootup recorded at");
	addTime(fnBootup, current_time_format,1);
    fclose(fnBootup);
    // close stats
    //open eventlog
    fnBootup = fopen("eventLogs.txt" , "a");
    fprintf(fnBootup, "Bootup recorded at");
    addTime(fnBootup, current_time_format,1);
	fclose(fnBootup);
	//close event log;
	for (int i = 0; i < TOTAL_SLOTS; i++)
	{ //initialize everything
		map[i].occupy = UNOCCUPIED_STATS;
		map[i].timeStamp = 0;
		map[i].timeTotalSpent = 0;
	}
	return;
}

void shutDown (ParkingSpace* map)
	// cleans up the mess
	// stores current parking lot information in a csv file ready to be read again by BootUp;
{
	eventLog("shutDown", 1);
	time(&current_time);
    current_time_format = localtime(&current_time);
    FILE* fnShutdown;
	fnShutdown = fopen("records.txt", "a");
	fprintf(fnShutdown, "Shutdown recorded at" );
	addTime(fnShutdown, current_time_format,1);
	fclose(fnShutdown);
	
	fnShutdown = fopen("stats.txt", "a");
	fprintf(fnShutdown, "Shutdown recorded at");
    addTime(fnShutdown, current_time_format,1);
    fclose(fnShutdown);
	
	fnShutdown = fopen("lastInfo.csv", "a");
	for (int i = 0; i < TOTAL_SLOTS; i++)
	{
		if (map[i].occupy == OCCUPIED_STATS)
		{
			fprintf(fnShutdown,"%d, %d \n", i, map[i].timeStamp);
		}
	}
	fclose(fnShutdown);
}

void eventLog(const char name[], const int loggingRequirement)
{
	//prints out to the eventLogs.txt
	//logs function calls
	if (!loggingRequirement){
		return;
	}
	if (loggingRequirement <= LOGLEVEL){
		time(&current_time);
		current_time_format = localtime(&current_time);
		//printf("it is now %d , %d \n", timeFormated -> tm_hour, timeFormated -> tm_min);
		//unfinished time function logger;
    
		FILE* fnLogs;
    
	
		fnLogs = fopen("eventLogs.txt", "a");
		addTime(fnLogs, current_time_format,0);
	
		fprintf (fnLogs, "entering %s\n", name);
		fclose(fnLogs);
	}
}

void errorLog(const int lineNumber, const char errorMessage[], const int importance)
	// prints to the eventLogs.txt Warnings and ERRORs that occured
	// importance 0: ERROR, importance 1: WARNING
	// ERROR will likely cause program crash so flush that
	// WARNING will detect problems of the system such as no more parking spot;
{
	FILE *error = fopen("eventLogs.txt", "a");
	
	char warningTitle[8] = "WARNING";
	char errorTitle[6] = "ERROR";
	//char initialMessage[8] = (importance == 1) ? "WARNING" : "ERROR"; // if importance is 1 print warning;
	if (importance){
		fprintf(error, "%s : %s",warningTitle, errorMessage);
	}
	else{
		fprintf(error, "%s : %s",errorTitle, errorMessage);
	}
	fprintf(error, " recorded on line: %d\n", lineNumber);
	fclose(error);
}


//COULD be implemented with OLED, could also be displayed on the oled at all time
float checkCapacity(const ParkingSpace* map)
    //checks the capacity of the parkingSpace as of rightNow
    //returns the decimal value such as 0.50
	// if virtual > 0; will instead look at reservations.
{
    eventLog("checkCapacity", 3); // logs
    
	int occupiedCount = 0;
	//int reservedCount = 0;
	float capacity;
	for (int i = 0; i < TOTAL_SLOTS; i ++)
	{
		if (map[i].occupy == OCCUPIED_STATS)
			occupiedCount++;
		// if (map[i].reserved == OCCUPIED_STATS)
		    // reservedCount++;
	}
	//printf("occupiedCout: %d reservedCount %d", occupiedCount,reservedCount);

	capacity = (float)occupiedCount / (float)TOTAL_SLOTS;
	//printf("capacity: %f\n",capacity);
	return capacity * 100.0;
}




void enterF(ParkingSpace *map, int location)
//triggers everytime a new car has arrived at his defined parkingLot
//put occupy as occupied when everything works out
{
    //logs when a car has entered a parking spot
    eventLog("enterF", 3); // logs

	map[location].occupy = OCCUPIED_STATS;
	map[location].timeStamp = current_time;
}

int exitF(ParkingSpace *map, int location)
//triggers everytime a car leaves his parking lot
//
{
    eventLog("exitF", 3); // logs
    // logs an exit
    // return the duration in second of the stay;
	float duration = current_time - map[location].timeStamp;
	map[location].occupy = UNOCCUPIED_STATS;
	map[location].timeStamp = UNOCCUPIED_STATS;
	map[location].timeTotalSpent += duration;
	//map[location].reserved = UNOCCUPIED_STATS;
	
	return duration;
}

float priceF(int duration)
{
	float price;
	if (duration <= MAX_TIME)
	{
		price = duration * 0.01;
	}
	else
	{
		price = MAX_TIME * 0.01 + (duration - MAX_TIME) * 0.05;
	}
	return price;
}

/////////////////////////////////////////////////////////////////////////
// STATISTICS CALCULATION-
// once every hour would be good

// checkCapacity would be part of statistics

int maxTimeStay(ParkingSpace *map)
// return the maximum stay time for the time of the check
// it is actually the min timeStamp
{
    eventLog("maxTimeStay", 3); // logs
	int min = INT_MAX;
	int unoccupiedCount = 0;
	for (int i = 0; i < TOTAL_SLOTS; i ++)
	{
		if (map[i].occupy == UNOCCUPIED_STATS)
		{
			unoccupiedCount ++;
			continue;
		}
		if (map[i].timeStamp < min)
		{
			min = map[i].timeStamp;
		}
	}
	if (unoccupiedCount == TOTAL_SLOTS)
		return 0;
		// just to be safe
	return min;
}
int minTimeStay(ParkingSpace *map)
{	//Actually max timeStamp
    eventLog("minTimeStay", 3); // logs
	int max = 0;
	int unoccupiedCount = 0;
	for (int i = 0; i < TOTAL_SLOTS; i++)
	{
		if (map[i].occupy == UNOCCUPIED_STATS)
		{
			unoccupiedCount ++;
			continue;
		}
		if (map[i].timeStamp > max)
		{
			max = map[i].timeStamp;
		}
	}
	if (unoccupiedCount == TOTAL_SLOTS)
		return 0;
	return max;
}
float average(ParkingSpace *map, int curr)
{
    eventLog("average", 3); // logs
	float sum = 0;
	int unoccupiedCount = 0;
	for (int i = 0; i < TOTAL_SLOTS; i ++)
	{
		if (map[i].occupy == UNOCCUPIED_STATS)
		{
			unoccupiedCount ++;
			continue;
		}
		sum += difftime(curr, map[i].timeStamp);
	}
	if (sum == 0)
		return 0;
	float temp = TOTAL_SLOTS - unoccupiedCount;
	float average = sum / temp;
	return average;
}

int* mostPopularParkingSpace(int *duration, int *arraySize){
	int max = duration[0];
	for (int i = 1; i < TOTAL_SLOTS; i ++)
	{
		if (max < duration[i])
		{
			max = duration[i];
		}
	}
	int threshhold = max - RANGE; //has to be beyond threshhold
	printf("max: %d threshhold: %d\n", max,threshhold);
	int numInRange = 0; // our Counter for how many popular parkingspaces
	for (int i = 0; i < TOTAL_SLOTS; i++)
	{
		if (threshhold <= duration[i])
		{
			numInRange ++;
		}
		printf("numInRange %d\n", numInRange);
	}
	*arraySize = numInRange; //set it  equal to the amount of parkingSpot in range
	numInRange = 0; // reset 
	int* popularArray = malloc( (*arraySize) * sizeof(*popularArray) );
	
	for (int i = 0; i < TOTAL_SLOTS; i++){
		if (threshhold <= duration[i]){
			popularArray[numInRange] = (i+1); //add 1 from 0 to 10 so we get 1 to 11
			numInRange++;
		}
			
	}
	return popularArray ;
}

int* leastPopularParkingSpace(int *duration, int *arraySize) {
	int min = duration[0];
	for (int i = 1; i < TOTAL_SLOTS; i ++)
	{
		if (min > duration[i])
		{
			min = duration[i];
		}
	}
	int threshhold = min + RANGE; //has to be BELOW threshhold
	int numInRange = 0; // our Counter for how many popular parkingspaces
	for (int i = 0; i < TOTAL_SLOTS; i++)
	{
		if (threshhold >= duration[i])
		{
			numInRange++;
		}
	}
	*arraySize = numInRange; //set it equal
	numInRange = 0; // reset 
	int* popularArray = malloc( (*arraySize) * sizeof(*popularArray) );
	
	for (int i = 0; i < TOTAL_SLOTS; i++){
		if (threshhold >= duration[i]){
			popularArray[numInRange] = (i+1); //add 1 from 1 to 11
			numInRange++;
		}
			
	}
	return popularArray ;
}


void statistics(ParkingSpace *map)
{
	eventLog("statistics", 2); // logs
	time(&current_time);
	current_time_format = localtime(&current_time);
	
	
	float capacity = checkCapacity(map);
	int maxDuration = maxTimeStay(map);
	if (maxDuration != 0)
	maxDuration = difftime(current_time, maxDuration);
	int minDuration = minTimeStay(map);
	if (minDuration != 0)
		minDuration = difftime(current_time, minDuration);
	float avgDuration = average(map, current_time);
	FILE *stats = fopen("stats.txt", "a");
	if (stats == NULL)
	{
	    eventLog("ERROR: file open failed", 1); // logs
	}
	else
	{
		fprintf(stats, "Current Time:");
		addTime(stats, current_time_format, 0);
		fprintf(stats,"Capacity: %0.1f%%, Current Max Parking Time: %d, Min Parking Time: %d, Average Parking Time: %0.1f\n"
				,capacity, maxDuration, minDuration, avgDuration);
		fclose(stats);
	}
}

void periodStats(const ParkingSpace* map){
	eventLog("periodStats", 2);
	time(&current_time);
	current_time_format = localtime(&current_time);
	
	int totalTimeOccupied[TOTAL_SLOTS];
	int* max;
	int* numMax = malloc(sizeof(*numMax));
	int* least;
	int* numLeast = malloc(sizeof(*numLeast));
	float currCapacity;
	
	
	for (int i = 0; i < TOTAL_SLOTS; i++){ // give totalTImeOccupied the amount of time
	//adjsted because of the fact that timeTotalSpent only updates on exit
		
		if (map[i].occupy == 1){
			totalTimeOccupied[i] = map[i].timeTotalSpent +  (current_time - map[i].timeStamp);
		} else{
			totalTimeOccupied[i] = map[i].timeTotalSpent;
		}
		printf("%d ---> %d \n",i,totalTimeOccupied[i]);
	}
	max = mostPopularParkingSpace(totalTimeOccupied,numMax);
	least = leastPopularParkingSpace(totalTimeOccupied,numLeast);
	
	currCapacity = checkCapacity(map);
	FILE* fnStatisticsTimed = fopen("StatsTimed.txt", "a");
	fprintf (fnStatisticsTimed, "Time:");
	addTime(fnStatisticsTimed,current_time_format,1);
			
	fprintf (fnStatisticsTimed, "Current capacity: %0.1f%%\n",currCapacity);
	fprintf (fnStatisticsTimed, "Cars entered since last checked : %d\n",runningUsage);
	fprintf (fnStatisticsTimed, "Least used Parking Spaces: ");
	for (int i = 0; i < *numLeast; i++)
	{
		fprintf(fnStatisticsTimed, "%d", least[i]);
		if (i < *numLeast-1){
			fprintf(fnStatisticsTimed, ", ");
		}
	}
	fprintf(fnStatisticsTimed, "\nMost used Parking Spaces: ");
	for (int i = 0; i < *numMax; i ++)
	{
		fprintf(fnStatisticsTimed, "%d", max[i]);
		if (i < *numLeast-1){
			fprintf(fnStatisticsTimed, ", ");
		}
	}
	
	fprintf(fnStatisticsTimed,"\n");
	fclose(fnStatisticsTimed);
	
	//reset the running Usage
	runningUsage = 0;
	free(max);
	free(numMax);
	free(least);
	free(numLeast);
	fflush(stdout);

}

int inputReceived(ParkingSpace *map, int location)
{
	//Something was inputted

	eventLog("inputReceived", 3); // logs
	time(&current_time);
	current_time_format = localtime(&current_time);
	
	FILE *record = fopen("records.txt", "a");
	if (record == NULL)
	{
		//perror("not able to open record.txt");
		return -5;
	}
	
	
	
	if (map[location].occupy == OCCUPIED_STATS) // the car exits the parking lot;
		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		//potential bug if the shortest path gives a wrong parkingSpace
	{
		int duration = exitF(map, location);
		float price = priceF(duration);
		if (duration > MAX_TIME) {// if more than necessary time;
			fprintf(record, "Exit recorded at location: %d, at time",location+1);
			fprintf(record, " %02d:%02d:%02d, ", current_time_format -> tm_hour,
			current_time_format -> tm_min,
			current_time_format -> tm_sec);
			fprintf(record, "with duration %d seconds. ", duration);
			fprintf(record, "Duration over maximum alloted time by %d seconds. ", duration - MAX_TIME);
			fprintf(record, "Price: $%1.2f\n", price);
		}
		else{// if normal exit
			fprintf(record, "Exit recorded at location: %d, at time",location+1);
			fprintf(record, " %02d:%02d:%02d, ", current_time_format -> tm_hour,
			current_time_format -> tm_min,
			current_time_format -> tm_sec);
			fprintf(record, "with duration %d seconds. ", duration);
			fprintf(record, "Price: $%1.2f\n", price);
		}
	}
	else
	{ // parkingLot number
		enterF(map, location);
		usage++;
		runningUsage++;
		fprintf(record, "Entry recorded at location: %d, at time", location+1);
		fprintf(record, " %02d:%02d:%02d, ", current_time_format -> tm_hour,
			current_time_format -> tm_min,
			current_time_format -> tm_sec);
		fprintf(record,"Car number %d.\n",usage);
	}
	statistics(map); // call statistics
	fclose(record);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t bitMask[8];			//bitMask used to define specific bit within a byte through hexidecimal

uint8_t tBuffer[1028];			//Buffer required for OLED to write bytes to OLED display

//Custom Functions
uint8_t min (uint8_t a, uint8_t b)	//Simple Min function
{
	if(a<b)
	{
		return a;
	}
	return b;
}

uint8_t max (uint8_t a, uint8_t b)	//Simple Max function
{
	if(a>b)
	{
		return a;
	}
	return b;
}

/*	Draws specific pixel to the buffer to draw to OLED
 *	@param *dbuffer the buffer for the OLED
 *	@param aCol integer of x co-ordinate
 *	@param aRow integer of y co-ordinate 
 */
void drawPixel(uint8_t *dbuffer, uint8_t aCol, uint8_t aRow)
{
	eventLog("draw Pixel", 3);
	uint8_t byteRow = aRow/8;			//OLED display divided into 8 rows of bytes, divide by 8 to get target row
	uint16_t targetByte = 128*byteRow + aCol;	//127 columns per row, multiply row number by 128 and add column for target Byte
	uint8_t targetBit = aRow % 8;			//taking modulus will define the bit within the target byte that needs to changed 
	*(dbuffer + targetByte) |= bitMask[targetBit];	//Or's in bit to the target byte
}

/*	Remove specific pixel from the buffer to draw to OLED
 *	@param *dbuffer the buffer for the OLED
 *	@param aCol integer of x co-ordinate
 *	@param aRow integer of y co-ordinate 
 */
void removePixel(uint8_t *dbuffer, uint8_t aCol, uint8_t aRow)
{
	eventLog("remove Pixel", 3);
	uint8_t byteRow = aRow / 8;						//Refer Above
	uint16_t targetByte = 128*byteRow + aCol;				//Refer Above
	uint8_t targetBit = aRow % 8;						//Refer Above
	*(dbuffer+targetByte) = (*(dbuffer+targetByte)&(~bitMask[targetBit]));	//Bitwise 'and' with the complement of the target bit to 											//remove given pixel
}


//Draws the left initial parking boundaries to buffer
void drawStartPB()
{
	eventLog("draw Start Parking Boundaries", 3);
	for(uint8_t i = 0; i < NUMSPB; i++)
	{
		for(uint8_t x=startPB[i].startpos.x; x <= startPB[i].endpos.x; x++) //Draws each column for boundary
		{
			for(uint8_t y=startPB[i].startpos.y; y <= startPB[i].endpos.y; y++)
			{
				drawPixel(tBuffer, x, y);
			}
		}
	}
}

//Writes the boundaries around a given parking space to the buffer
void drawPB(uint8_t aParkSpace)
{
	eventLog("draw Parking Boundary", 3);
	uint8_t x1 = parkSpacePB[aParkSpace].startpos.x;
	uint8_t x2 = parkSpacePB[aParkSpace].endpos.x;
	uint8_t y1 = parkSpacePB[aParkSpace].startpos.y;
	uint8_t y2 = parkSpacePB[aParkSpace].endpos.y;

	for(uint8_t x = x1; x <= x2; x++)
	{
		if(x< x2-1)					//Draws Horizontal bar
		{
			for(uint8_t y = y1; y< y1+2; y++)
			{
				drawPixel(tBuffer, x, y);
			}
		}
		else						//Draws Vertical bar
		{
			for(uint8_t y = y1; y <= y2; y++)
			{
				drawPixel(tBuffer, x, y);
			}
		}	
	}
}

//Writes the given parking space to the buffer
void drawPS(uint8_t aParkSpace)
{
	eventLog("draw Parking Space", 3);
	uint8_t x1 = parkSpace[aParkSpace].startpos.x;
	uint8_t x2 = parkSpace[aParkSpace].endpos.x;
	uint8_t y1 = parkSpace[aParkSpace].startpos.y;
	uint8_t y2 = parkSpace[aParkSpace].endpos.y;

	for(uint8_t x=x1; x <= x2; x++)			//Writes rectangle from corner to corner
	{
		for(uint8_t y=y1; y <= y2; y++)
		{
			drawPixel(tBuffer, x, y);
		}
	}
}

//Removes the given Parking space from the buffer
void removePS(uint8_t aParkSpace)
{
	eventLog("remove Parking Space", 3);
	uint8_t x1 = parkSpace[aParkSpace].startpos.x;
	uint8_t x2 = parkSpace[aParkSpace].endpos.x;
	uint8_t y1 = parkSpace[aParkSpace].startpos.y;
	uint8_t y2 = parkSpace[aParkSpace].endpos.y;

	for(uint8_t x=x1; x <= x2; x++)			//Removes rectangle from corner to corner
	{
		for(uint8_t y=y1; y <= y2; y++)
		{
			removePixel(tBuffer, x, y);
		}
	}
}

//Writes the given entrance to the buffer
void drawEntrance(uint8_t anEntrance)
{
	eventLog("draw Entrance", 3);
	uint8_t x1 = entrance[anEntrance].startpos.x;
	uint8_t x2 = entrance[anEntrance].endpos.x;
	uint8_t y1 = entrance[anEntrance].startpos.y;
	uint8_t y2 = entrance[anEntrance].endpos.y;

	for(uint8_t x=x1; x <= x2; x++)			//Draws from corner to corner
	{
		for(uint8_t y=y1; y <= y2; y++)
		{
			drawPixel(tBuffer, x, y);
		}
	}
}

/*	Writes the path from enterance to parking space
 *	@param anEntrance the entrance to start path from
 *	@param aParkSpace the Parking space to go to
 */
void drawReserve(uint8_t anEntrance, uint8_t aParkSpace)
{
	eventLog("draw Reserve", 3);
	uint8_t x1 = entPos[anEntrance].x;			//Gets centers of Entrance
	uint8_t y1 = entPos[anEntrance].y;

	uint8_t x2 = psPos[aParkSpace].x;			//Gets center of Parking Space
	uint8_t y2 = psPos[aParkSpace].y;

	uint8_t x = x1;
	for(uint8_t y = min(y1, y2)+5; y<=max(y1,y2);  y++)	//Writes vertical line up from entrance
	{
		drawPixel(tBuffer, x, y);
	}

	x = x2;
	for(uint8_t y = min(y1, y2); y<=min(y1,y2)+4;  y++)	//Writes vertical line up to Parking Space center
	{
		drawPixel(tBuffer, x, y);
	}

	uint8_t y = y2+5;
	for(uint8_t x = min(x1, x2); x<=max(x1,x2);  x++)	//Writes Horizontal line connecting two vertical lines
	{
		drawPixel(tBuffer, x, y);
	}
	
}

/*	Removes the path from enterance to parking space
 *	@param anEntrance the entrance to start path from
 *	@param aParkSpace the Parking space to go to
 */
void removeReserve(uint8_t anEntrance, uint8_t aParkSpace)
{
	eventLog("remove Reserve", 3);
	uint8_t x1 = entPos[anEntrance].x;			//Refer Above
	uint8_t y1 = entPos[anEntrance].y;

	uint8_t x2 = psPos[aParkSpace].x;			//Refer Above
	uint8_t y2 = psPos[aParkSpace].y;

	uint8_t x = x1;
	for(uint8_t y = min(y1, y2)+5; y<=max(y1,y2);  y++)	//Refer Above
	{
		removePixel(tBuffer, x, y);
	}

	x = x2;
	for(uint8_t y = min(y1, y2); y<=min(y1,y2)+4;  y++)	//Refer Above
	{
		removePixel(tBuffer, x, y);
	}

	uint8_t y = y2+5;
	for(uint8_t x = min(x1, x2); x<=max(x1,x2)+4;  x++)	//Refer Above
	{
		removePixel(tBuffer, x, y);
	}
	
}

//Displays written text to OLED, Function created due to buffer overwriting text
void displayText()
{
	eventLog("Display Text", 3);
	oledSetTextColumns();		//Writes text in top right corner
	oledSetCursorByPixel(0,97);
	oledWrite("Smart");
	oledSetTextColumns();
	oledSetCursorByPixel(1,85);
	oledWrite("Parking");

	oledSetTextColumns();		//Writes y-coordinates of Parking Spaces
	oledSetCursorByPixel(1,28);
	oledWrite("C");
	oledSetTextColumns();
	oledSetCursorByPixel(3,28);
	oledWrite("B");
	oledSetTextColumns();
	oledSetCursorByPixel(5,28);
	oledWrite("A");

	oledSetTextColumns();		//Writes x-coordinates of Parking Spaces
	oledSetCursorByPixel(7,42);
	oledWrite("1");
	oledSetTextColumns();
	oledSetCursorByPixel(7,55);
	oledWrite("2");
	oledSetTextColumns();
	oledSetCursorByPixel(7,68);
	oledWrite("3");
	oledSetTextColumns();
	oledSetCursorByPixel(7,81);
	oledWrite("4");

	oledSetTextColumns();		//Labels Entrances
	oledSetCursorByPixel(7,16);
	oledWrite("E0");
	oledSetTextColumns();
	oledSetCursorByPixel(7,106);
	oledWrite("E1");
}

//Initialize Parking lot with given parameters
void initializePL()
{
	eventLog("initialize ParkingLot", 2);
	oledDriverInit();		//Initialize OLED
	oledSetDisplayPower(0);
	oledClear();

	bitMask[0] = 0x01;		//Declare bitMask 00000001
	bitMask[1] = 0x02;		//Declare bitMask 00000010
	bitMask[2] = 0x04;		//Declare bitMask 00000100
	bitMask[3] = 0x08;		//Declare bitMask 00001000		
	bitMask[4] = 0x10;		//Declare bitMask 00010000
	bitMask[5] = 0x20;		//Declare bitMask 00100000
	bitMask[6] = 0x40;		//Declare bitMask 01000000
	bitMask[7] = 0x80;		//Declare bitMask 10000000

	for(uint16_t i = 0; i < 1024; i++)	//Initialize the buffer to 0
	{
		tBuffer[i] = 0x0;
	}

	for(uint8_t i = 0; i < NUMSPB; i++)	//Coordinates of initial Parking Space boundaries
	{
		startPB[i].startpos.x = 37;
		startPB[i].startpos.y = i*16 + 4;
		startPB[i].endpos.x = 38;
		startPB[i].endpos.y = i*16 + 15;		
	}

	for(uint8_t i = 0; i < NUMPSROW; i++)	//Coordinates of parking Space Boundaries, 'filled area', and centers
	{
		for(uint8_t j = 0; j < NUMPSCOL; j++)
		{
			uint8_t k = i*NUMPSCOL + j;
			if(k < NUMPARKSPACE)
			{
				parkSpacePB[k].startpos.x = j*13 + 39;
        			parkSpacePB[k].startpos.y = 36 - i*16;
        			parkSpacePB[k].endpos.x = j*13 + 51;
        			parkSpacePB[k].endpos.y = 47 - i*16;

        			parkSpace[k].startpos.x = parkSpacePB[k].startpos.x + 2;
        			parkSpace[k].startpos.y = parkSpacePB[k].startpos.y + 4;
        			parkSpace[k].endpos.x = parkSpacePB[k].endpos.x - 4;
        			parkSpace[k].endpos.y = parkSpacePB[k].endpos.y;
        
        			psPos[k].x = parkSpace[k].startpos.x + 3;
        			psPos[k].y = parkSpace[k].startpos.y + 4;
			}	
		}		
	}
	
	for(uint8_t i = 0; i <NUMENTRANCE; i++)	//Coordinates of Entrance boundaries and centers
	{
		entrance[i].startpos.x = 4 + i*111;
    		entrance[i].startpos.y = 60;
    		entrance[i].endpos.x = entrance[i].startpos.x + 8;
    		entrance[i].endpos.y = 63;
    
    		entPos[i].x = 8 + i*111;
    		entPos[i].y = 59;
	}

	drawStartPB();	//Draws initial boundaries

	for(uint8_t i = 0; i<NUMPARKSPACE; i++)	//Draws each parking space
	{
		drawPB(i);
	}
	for(uint8_t i = 0; i<NUMENTRANCE; i++)	//Draws each entrance
	{
		drawEntrance(i);
	}

	oledDraw(tBuffer, 1024);		//Display Buffer to OLED
	displayText();				//Write text on top of buffer
	
	oledSetDisplayPower(1);			//Turn display on
}

//Draws given Parking space
void setPS(uint8_t aParkSpace)
{
	eventLog("set Parking Space", 3);
	oledSetCursorByPixel(0,0);
	drawPS(aParkSpace);
	oledDraw(tBuffer, 1024);
	displayText();
}

//Clears given Parking space
void clearPS(uint8_t aParkSpace)
{
	eventLog("clear Parking Space", 3);
	oledSetCursorByPixel(0,0);
	removePS(aParkSpace);
	oledDraw(tBuffer, 1024);
	displayText();
}

//Draws given path
void setPath(uint8_t anEntrance, uint8_t aParkSpace)
{
	eventLog("set Path", 3);
	oledSetCursorByPixel(0,0);
	drawReserve(anEntrance, aParkSpace);
	oledDraw(tBuffer, 1024);
	displayText();
}

//Clears given path
void clearPath(uint8_t anEntrance, uint8_t aParkSpace)
{
	eventLog("clear Path", 3);
	oledSetCursorByPixel(0,0);
	removeReserve(anEntrance, aParkSpace);
	oledDraw(tBuffer, 1024);
	displayText();
}

//Declaration of gpio's and direction (All inputs)
int gpioDeclare ()
{
	eventLog("gpio Declare", 3);
	uint8_t rv;
	uint8_t rq;

	// check if gpio is already exported
	if ((rq = gpio_is_requested(1)) < 0)
	{
		perror("gpio_is_requested");
		eventLog("ERROR: gpio 1 request Fail", 1);
		return EXIT_FAILURE;
	}
	if ((rq = gpio_is_requested(E0)) < 0)
	{
		perror("gpio_is_requested");
		eventLog("ERROR: gpio 0 request Fail", 1);
		return EXIT_FAILURE;
	}
	if ((rq = gpio_is_requested(E1)) < 0)
	{
		perror("gpio_is_requested");
		eventLog("ERROR: gpio 6 request Fail", 1);
		return EXIT_FAILURE;
	}
	if ((rq = gpio_is_requested(P0)) < 0)
	{
		perror("gpio_is_requested");
		eventLog("ERROR: gpio 45 request Fail", 1);
		return EXIT_FAILURE;
	}
	if ((rq = gpio_is_requested(P1)) < 0)
	{
		perror("gpio_is_requested");
		eventLog("ERROR: gpio 46 request Fail", 1);
		return EXIT_FAILURE;
	}
	if ((rq = gpio_is_requested(P2)) < 0)
	{
		perror("gpio_is_requested");
		eventLog("ERROR: gpio 2 request Fail", 1);
		return EXIT_FAILURE;
	}
	if ((rq = gpio_is_requested(P4)) < 0)
	{
		perror("gpio_is_requested");
		eventLog("ERROR: gpio 18 request Fail", 1);
		return EXIT_FAILURE;
	}
	if ((rq = gpio_is_requested(P5)) < 0)
	{
		perror("gpio_is_requested");
		eventLog("ERROR: gpio 19 request Fail", 1);
		return EXIT_FAILURE;
	}
	if ((rq = gpio_is_requested(P6)) < 0)
	{
		perror("gpio_is_requested");
		eventLog("ERROR: gpio 3 request Fail", 1);
		return EXIT_FAILURE;
	}
	//Declare gpio
	if ((rv = gpio_request(1, NULL)) < 0)
	{
		perror("gpio_request");
		eventLog("ERROR: gpio 1 request use Fail", 1);
		return EXIT_FAILURE;
	}
	if ((rv = gpio_request(E0, NULL)) < 0)
	{
		perror("gpio_request");
		eventLog("ERROR: gpio 0 request use Fail", 1);
		return EXIT_FAILURE;
	}
	if ((rv = gpio_request(E1, NULL)) < 0)
	{
		perror("gpio_request");
		eventLog("ERROR: gpio 6 request use Fail", 1);
		return EXIT_FAILURE;
	}
	if ((rv = gpio_request(P0, NULL)) < 0)
	{
		perror("gpio_request");
		eventLog("ERROR: gpio 45 request use Fail", 1);
		return EXIT_FAILURE;
	}
	if ((rv = gpio_request(P1, NULL)) < 0)
	{
		perror("gpio_request");
		eventLog("ERROR: gpio 46 request use Fail", 1);
		return EXIT_FAILURE;
	}
	if ((rv = gpio_request(P2, NULL)) < 0)
	{
		perror("gpio_request");
		eventLog("ERROR: gpio 2 request use Fail", 1);
		return EXIT_FAILURE;
	}
	if ((rv = gpio_request(P4, NULL)) < 0)
	{
		perror("gpio_request");
		eventLog("ERROR: gpio 18 request use Fail", 1);
		return EXIT_FAILURE;
	}
	if ((rv = gpio_request(P5, NULL)) < 0)
	{
		perror("gpio_request");
		eventLog("ERROR: gpio 19 request use Fail", 1);
		return EXIT_FAILURE;
	}
	if ((rv = gpio_request(P6, NULL)) < 0)
	{
		perror("gpio_request");
		eventLog("ERROR: gpio 3 request use Fail", 1);
		return EXIT_FAILURE;
	}

	// set to input direction
	printf("> setting to input\n");
	if ((rv = gpio_direction_input(1)) < 0)
	{
		perror("gpio_direction_input");
		eventLog("ERROR: gpio 1 direction Fail", 1);
	}
	if ((rv = gpio_direction_input(E0)) < 0)
	{
		perror("gpio_direction_input");
		eventLog("ERROR: gpio 0 direction Fail", 1);
	}
	if ((rv = gpio_direction_input(E1)) < 0)
	{
		perror("gpio_direction_input");
		eventLog("ERROR: gpio 6 direction Fail", 1);
	}
	if ((rv = gpio_direction_input(P0)) < 0)
	{
		perror("gpio_direction_input");
		eventLog("ERROR: gpio 45 direction Fail", 1);
	}
	if ((rv = gpio_direction_input(P1)) < 0)
	{
		perror("gpio_direction_input");
		eventLog("ERROR: gpio 46 direction Fail", 1);
	}
	if ((rv = gpio_direction_input(P2)) < 0)
	{
		perror("gpio_direction_input");
		eventLog("ERROR: gpio 2 direction Fail", 1);
	}
	if ((rv = gpio_direction_input(P4)) < 0)
	{
		perror("gpio_direction_input");
		eventLog("ERROR: gpio 18 direction Fail", 1);
	}
	if ((rv = gpio_direction_input(P5)) < 0)
	{
		perror("gpio_direction_input");
		eventLog("ERROR: gpio 19 direction Fail", 1);
	}
	if ((rv = gpio_direction_input(P6)) < 0)
	{
		perror("gpio_direction_input");
		eventLog("ERROR: gpio 3 direction Fail", 1);
	}

	return 0;
}

//OLED Display Data to track positions
void initializeDisplayInfo()
{
	eventLog("initialize Display Info", 2);
	for(uint8_t i = 0; i < NUMPARKSPACE; i++)	//Calculates distance from each entrance to parking space
	{
		display.distance[0][i] = ((i)%NUMPSCOL)+1 + i/NUMPSCOL;
		display.distance[1][i] = NUMPSCOL-(i%NUMPSCOL) + i/NUMPSCOL;
		//printf(">DISTANCE %d: %d\n", i, distance[0][i]);
		//printf(">DISTANCE %d: %d\n", i, distance[1][i]);		
	}

	for(uint8_t i = 0; i < NUMPARKSPACE; i++)	//Initialize available parking spaces due to limitations in reed switches
	{
		display.PLStatus[i] = 1;
		//if(i == 0 || i == 1 || i == 2 || i == 4 || i == 5 || i == 6)
		//{
			display.PLArray[i] = UNOCCUPIED;
		//}
		//else
		//{
		//	setPS(i);
		//	display.PLArray[i] = OCCUPIED;
		//}
	}

	for(uint8_t i = 0; i < NUMENTRANCE; i++)	//Initialize entrances as empty
	{
		display.spotCounter[i] = 0;
		display.ENTArray[i] = false;
	}
}

//Parking Space input (input > 0)
void PSInput(int input)
{
	eventLog("oled Parking Space Input", 2);
	if(display.PLArray[input-1] == UNOCCUPIED || display.PLArray[input-1] == RESERVED)	//Occupies given parking space if empty or
	{											//reserved
		for(uint8_t i = 0; i < NUMENTRANCE; i++)
		{
			if(display.spot[i] == input-1)						//If reserved remove reservation line
			{
				oledSetDisplayPower(0);
				clearPath(i, input-1);
				display.spotCounter[i] = 0;
				display.ENTArray[i] = false;
			}
		}
	
		setPS(input-1);
		display.PLArray[input-1] = OCCUPIED;
		oledSetDisplayPower(1);
	}
	else						//Empties parking space
	{
		oledSetDisplayPower(0);
		clearPS(input-1);
		display.PLArray[input-1] = UNOCCUPIED;
		oledSetDisplayPower(1);
	}
	bool flag = true;
	for(uint8_t i = 0; i < NUMPARKSPACE; i++)	//Checks if parking lot is full
	{
		if(display.PLArray[i] == UNOCCUPIED)
		{
			flag = false;
		}
	}
	if(flag)					//Indicates full parkinglot
	{
		oledSetTextColumns();
		oledSetCursorByPixel(0,5);
		oledWrite("FULL");
	}
}

//Entrance input (input > 0)
void EInput(int input)
{
	eventLog("oled Entrance Input", 2);
	if(!(display.ENTArray[input-1]))			//If the entrance isn't being used yet run code
	{
		display.tempDist = 0;
		display.tempPS = 0;

		for(uint8_t i = 0; i < NUMPARKSPACE; i++)	//Check all available parking spaces and their distances from entrance
		{
			if(display.PLArray[i] == UNOCCUPIED)
			{
				if(!display.tempDist)
				{
					display.tempDist = display.distance[input-1][i];
					display.tempPS = i;
				}
				else if (display.distance[input-1][i] < display.tempDist)
				{	
					display.tempDist = display.distance[input-1][i];
					display.tempPS = i;
				}
			}
		
		}
		if(display.tempDist == 0)			//If no parking space found, parking lot is full
		{
			oledSetTextColumns();
			oledSetCursorByPixel(0,5);
			oledWrite("FULL");
		}
		else						//Draw path to closest parking space
		{
			oledSetDisplayPower(0);
			setPath(input-1, display.tempPS);
			display.spot[input-1] = display.tempPS;
			display.ENTArray[input-1] = true;
			display.PLArray[display.tempPS] = RESERVED;
			oledSetDisplayPower(1);
		}
	}
	else
	{
		clearPath(input-1, 3);
		display.ENTArray[input-1] = false;
	}
}

int main ()
{
	double timeElapsed;

	ParkingSpace map[TOTAL_SLOTS];
	bootUp(map);

	initializePL();			//Initialize Parking Lot (OLED And Co-ordinates)
	
	initializeDisplayInfo();	//Initialize OLED Display info	
	
	
	int input = 1;

	if(gpioDeclare())		//Declare GPIOS
	{
		eventLog("gpio declare failed", 1);
		perror("ERROR");
		return -1;
	}

	while(input != 0)		//Loop until Exited
	{
		bool loop = true;
		while(loop)		//Loops for valid input
		{
			usleep(80000);
			for(uint8_t i = 0; i < NUMENTRANCE; i++)	//Removes path to Parking space if beyond given threshold
			{
				if(display.ENTArray[i])
				{
					display.spotCounter[i] ++;
				}
				if(display.spotCounter[i] > 200)
				{
					clearPath(i, display.spot[i]);
					display.PLArray[display.spot[i]] = UNOCCUPIED;
					display.spotCounter[i] = 0;
					display.ENTArray[i] = false;
				}
			}
			time(&current_time);
			timeElapsed = difftime(current_time,lastStat_time);	//Stats per 30

			if ( timeElapsed >= 30){
				time(&lastStat_time); // update lastStat
				periodStats(map);
			}

			if(gpio_get_value(1) == 1)			//If overwrite button pressed, manual input
			{
				printf("> Please give an input: ");
				fflush(stdout);
				if(gpio_get_value(1) == 1)
				{
					scanf("%d", &input);
					printf("\n");
					loop = false;
				}
			}

			if((!display.ENTArray[0]) && gpio_get_value(E0) == 0)	//Inputs from Reed Switches (Must be declared individually)
			{
				usleep(50000);
				if((!display.ENTArray[0]) && gpio_get_value(E0) == 0)
				{
					input = -1;
					loop = false;
				}
			}
			else if((!display.ENTArray[1]) && gpio_get_value(E1) == 0)
			{
				usleep(50000);
				if((!display.ENTArray[1]) && gpio_get_value(E1) == 0)
				{
					input = -2;
					loop = false;
				}
			}

			else if(gpio_get_value(P0) != display.PLStatus[0])
			{
				usleep(1000000);
				if(gpio_get_value(P0) != display.PLStatus[0])
				{
					input = 1;
					display.PLStatus[0] = gpio_get_value(P0);
					loop = false;
				}
			}

			else if(gpio_get_value(P1) != display.PLStatus[1])
			{
				usleep(1000000);
				if(gpio_get_value(P1) != display.PLStatus[1])
				{
					input = 2;
					display.PLStatus[1] = gpio_get_value(P1);
					loop = false;
				}
			}
			else if(gpio_get_value(P2) != display.PLStatus[2])
			{
				usleep(1000000);
				if(gpio_get_value(P2) != display.PLStatus[2])
				{
					input = 3;
					display.PLStatus[2] = gpio_get_value(P2);
					loop = false;	
				}
			}
			else if(gpio_get_value(P4) != display.PLStatus[4])
			{
				usleep(1000000);
				if(gpio_get_value(P4) != display.PLStatus[4])
				{
					input = 5;
					display.PLStatus[4] = gpio_get_value(P4);
					loop = false;
				}
			}	
			else if(gpio_get_value(P5) != display.PLStatus[5])
			{
				usleep(1000000);
				if(gpio_get_value(P5) != display.PLStatus[5])
				{
					input = 6;
					display.PLStatus[5] = gpio_get_value(P5);
					loop = false;
				}
			}
			else if(gpio_get_value(P6) != display.PLStatus[6])
			{
				usleep(1000000);
				if(gpio_get_value(P6) != display.PLStatus[6])
				{
					input = 7;
					display.PLStatus[6] = gpio_get_value(P6);
					loop = false;
				}
			}		
		}

		if(input > 0)	//If Parking Space Input
		{
			inputReceived(map, input-1);
			PSInput(input);
		}
		if(input < 0)	//If Entrance Input
		{
			input = -input;
			EInput(input);
		}
	}
	
	shutDown(map);
	oledClear();			//Clear OLED information
	oledSetDisplayPower(0);
	
	
	return 0;
}