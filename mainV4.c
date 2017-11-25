/*SMART PARKING SYSTEM MACHINE INDEPENDANT SOFTWARE 
HANYUAN YE
CHRIS YU
YI FAN YU
ECE 150 1A FALL 2017
*/

#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>



/* THINGS LEFT TO DO:
   
   -oled reset needs to pathFinding entrance 1 on, pathFinding entrance 2 on;
   -timer to activate standalone statistics collection is not implemented
   -think of better statistics to satisfy our customer needs
	-Statistics:
		-Most used parking spots
		-Least used parking spots
		-Most/Least used entrances
		-Average idle time
   -LOGLEVEL is still not implemented hardish
   
*/

const int LOGLEVEL = 2; // 
// 3 means log the action of every single function (debugging)
// 2 means log the action of functions that call other functions and their exits (RECOMMENDED)
// 1 means log only the functions that call other functions and not their exits (RECOMMENDED)
// 0 means no logging (dangerous)
const int TOTAL_SLOTS = 11; //total slot allowed
const int OCCUPIED = 1; //occupied is 1
const int UNOCCUPIED = 0; //unocuped is 0
int usage = 0; // logs the usage of our system;
int rejected = 0; // logs how many cars get bounced because of full ParkLot
int shutdown = 0;
const int MAX_TIME = 10; // will alert the operator if anyone stays more than MAX_TIME;

//keeps track of the time the program boots up;
time_t current_time;
struct tm* current_time_format;
//current_time_format = localtime(&current_time);

typedef struct
{
	int occupy;
	int timeStamp; // when the person arrives
	int reserved; 
	float timeTotalSpent;
	// when the car shows up at the entrance, the system reserves 
}ParkingSpace;

/////////////////////////////////////////////////////////////////////////////////////////
//
//FUNCTIONS SIGNATURE

ParkingSpace* BootUp(ParkingSpace* map,char fileName[],int reboot)
	// maybe allocate memory to all the parking spots and reset them to zero or
	// read from a file that contains the information in case the program is restarting;
	// even logs to the right files since we do not want to have a big fat log file for weeks 
{
	FILE* fnBootup;
	if (!reboot){
		// if reboot is false;
		for (int i = 0; i < TOTAL_SLOTS; i++)
		{
			map[i].occupy = UNOCCUPIED;
			map[i].timeStamp = UNOCCUPIED;
			map[i].timeTotalSpent = 0;
		}
	} else{ // reboot is true
		fnBootup = fopen("lastInfo.txt", "r");
		int location;
		time_t timeRecorded;
		
		while (fscanf(fnBootup, "%d, %d", location, timeRecorded) == 2)
		{
			map[location].occupy = OCCUPIED;
			map[location].reserved = OCCUPIED;
			map[location].timeStamp = timeRecorded;
		}
		fclose(fnBootup);
	}
	return map;
}

void shutDown (char fileName[], ParkingSpace* map)
	// cleans up the mess
	// stores current parking lot information in a csv file ready to be read again by BootUp;
{
	FILE* fnShutdown;
	fnShutdown = fopen("lastInfo.txt", "a");
	for (int i = 0; i < TOTAL_SLOTS; i++)
	{
		if (map[i].occupy == OCCUPIED)
		{
			fprintf(fnShutdown,"%d, %d,\n", i, map[i].timeStamp);
		}
	}
	fclose(fnShutdown);
}
void eventLog(const char name[], const int loggingRequirement)
	//prints out to the eventLogs.txt
	//logs function calls 
{
    time_t timeSec;
	struct tm* timeFormated;
    timeSec = time(NULL);
	timeFormated = localtime(&timeSec);
	printf("it is now %d , %d \n", timeFormated -> tm_hour, timeFormated -> tm_min);
    //unfinished time function logger;
    
    FILE* fnLogs;
    
    fnLogs = fopen("eventLogs.txt", "a");
	fprintf (fnLogs, "%d-%02d-%02d %02d:%02d:%02d  ",(timeFormated -> tm_year)+1900, timeFormated -> tm_mon,
			timeFormated -> tm_mday,timeFormated -> tm_hour,timeFormated -> tm_min,
			timeFormated -> tm_sec);
    fprintf (fnLogs, "entering %s\n", name);
    fclose(fnLogs);
}


void errorLog(const int lineNumber, const char errorMessage[], const int importance)
	// prints to the eventLogs.txt Warnings and ERRORs that occured
	// importance 0: ERROR, importance 1: WARNING
	// ERROR will likely cause program crash so flush that
	// WARNING will detect problems of the system such as no more parking spot;
{
	FILE *error = fopen("eventLog.txt", "a");
	
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
float checkCapacity(ParkingSpace* map, int virtCapac)
    //checks the capacity of the parkingSpace as of rightNow
    //returns the decimal value such as 0.50
	// if virtual > 0; will instead look at reservations.
{
    eventLog("checkCapacity",LOGLEVEL); // logs
    
	int occupiedCount = 0;
	int reservedCount = 0;
	float capacity;
	for (int i = 0; i < 12; i ++)
	{
		if (map[i].occupy == OCCUPIED)
			occupiedCount++;
		if (map[i].reserved == OCCUPIED)
		    reservedCount++;
	}
	if (virtCapac){
	    capacity = reservedCount / TOTAL_SLOTS;
	}
	else{
	    capacity = occupiedCount / TOTAL_SLOTS;
	}
	return capacity;
}




void enterF(ParkingSpace *map, int location)
//triggers everytime a new car has arrived at his defined parkingLot
//put occupy as occupied when everything works out
{
    //logs when a car has entered a parking spot
    eventLog("enterF",LOGLEVEL); // logs

	map[location].occupy = OCCUPIED;
	map[location].timeStamp = current_time;
}

int exitF(ParkingSpace *map, int location)
//triggers everytime a car leaves his parking lot
//
{
    eventLog("exitF",LOGLEVEL); // logs
    // logs an exit
    // return the duration in second of the stay;
	float duration = current_time - map[location].timeStamp;
	map[location].
	map[location].occupy = UNOCCUPIED;
	map[location].timeStamp = UNOCCUPIED;
	
	return duration;
}

/////////////////////////////////////////////////////////////////////////
// STATISTICS CALCULATION-
// once every hour would be good

// checkCapacity would be part of statistics

int maxTimeStay(ParkingSpace *map)
// return the maximum stay time for the time of the check
{
    eventLog("maxTimeOfStay",LOGLEVEL); // logs
	int max = map[0].timeStamp;
	int unoccupiedCount = 0;
	for (int i = 0; i < TOTAL_SLOTS; i ++)
	{
		if (map[i].occupy == UNOCCUPIED)
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
		// just to be safe
	return max;
}
int minTimeStay(ParkingSpace *map)
{
    eventLog("minTimeStay",LOGLEVEL); // logs
	int min = 0;
	int unoccupiedCount = 0;
	for (int i = 0; i < TOTAL_SLOTS; i++)
	{
		if (map[i].occupy == UNOCCUPIED)
		{
			unoccupiedCount ++;
			continue;
		}
		if (map[i].timeStamp > min)
		{
			min = map[i].timeStamp;
		}
	}
	if (unoccupiedCount == TOTAL_SLOTS)
		return 0;
	return min;
}
float average(ParkingSpace *map, int curr)
{
    eventLog("average",LOGLEVEL); // logs
	float sum = 0;
	int unoccupiedCount = 0;
	for (int i = 0; i < TOTAL_SLOTS; i ++)
	{
		if (map[i].occupy == UNOCCUPIED)
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

int* mostPopularParkingSpace(float useByParkingLot[])
	// 0 is the first 10 is the last
	// checks the parking spot with a car in it the longest amount of time
	// holly shit what happens when there are 2 with exact pupolar parking space
	// maybe return a struct that tells us the amount of popular parkingSpaces
{
	eventLog("mostPopularParkingSpace",LOGLEVEL);
	
	int repetitionCounter = 1; //gets incremented everytime currentMaxValue = temp
	int repetitionArray[11];
	int currentMax = 0; // current popular parking spot;
	time_t currentMaxValue = map[currentMax].timeTotalSpent +  (current_time - map[currentMax].timeStamp);
	time_t temp; // temp to take the next input data
	for (int i = 1; i < TOTAL_SLOTS; i++){
		if (map[i].occupy = OCCUPIED){
			temp = map[i].timeTotalSpent + (current_time - map[currentMax].timeStamp); 
			// need to add the extra time spent there if car is still parked
		}else{ // parking slot is not being used;
			temp = map[i].timeTotalSpent;
		}
		
		//processing the data 
		if (currentMaxValue < temp){
			currentMax = i;
			currentMaxValue = temp;
			repetitionCounter = 1; 
			// we have a new max Value; // reset repetition
		}
		else if (currentMaxValue == temp){
			repetitionArray[repetitionCounter-1] = i;
			repetitionCounter++;
		}
		
	}
	
	// declare a pointer to the popular spots;
	int* popularArray;
	if (repetitionCounter != 1){ // this means we have not 1 max;
		popularArray = malloc(repetitionCounter * sizeof(*popularArray) );
		// create an array with size equal to how many repetitionCounter we have
		for (int j = 0; j < repetitionCounter ; j++ ){
			popularArray[j] = repetitionArray[j];
		}

	} else {
		popularArray = malloc(sizeof (*popularArray));
		popularArray[0] = currentMax;
	}
	return popularArray;
	
}
void statistics(ParkingSpace *map)
{
    eventLog("statistics",LOGLEVEL); // logs
	
	int* popular;
	int* notVeryPopular; // POPULAR / not very popular parking Spots
	int numPopular;
	int numNotVeryPopular;
	
	float totalTimeOccupied[11];
	for (int i; i < 11; i++){ // give totalTImeOccupied the amount of time 
		
		if (map[i].occupy == 1){
			totalTimeOccupied = map[i].timeTotalSpent +  (current_time - map[i].timeStamp);
		} else{
			totalTimeOccupied = map[i].timeTotalSpent;
		}
	
	}	
	float capacity = checkCapacity(map);
	
	int maxDuration = maxTimeStay(map);
	if (maxDuration != 0)
		// maxDuration = difftime(curr, maxDuration);
	// int minDuration = minTimeStay(map);
	// if (minDuration != 0)
		// minDuration = difftime(curr, minDuration);
	// float avgDuration = average(map, curr);
	
	FILE *stats = fopen("stats.txt", "a");
	if (stats == NULL)
	{
	        // eventLog("opening failed",LOGLEVEL); // logs
		// logError(__LINE__);
	}
	// else
	// {
		// fprintf(stats, "Currrent Time: %ld, Capacity: %f, Maximum Duration: %d, Minimum Duration: %d, Average Duration: %f\n"
				// , curr, capacity, maxDuration, minDuration, avgDuration);
		// fclose(stats);
	// }
}

int input(ParkingSpace *map, int location)
{	
	//Something was inputted

	eventLog("input",LOGLEVEL); // logs
	FILE *record = fopen("record.txt", "a");
	if (record == NULL)
	{
		errorLog(__LINE__);
		//perror("not able to open record.txt");
		return -5;
	}
	statistics(map);
	if ( ( checkCapacity (map) ) >= 1.00)
	{ // if full
		logError(__LINE__);
		// logs this attempt to join
		//display something on the oled 
		return -1;
	}
		
	if (map[location].occupy == OCCUPIED) // the car exits the parking lot;
		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		//potential bug if the shortest path gives a wrong parkingSpace
	{
		int duration = exitF(map, location);
		
		if (duration > MAX_TIME) // if more than necessary time;
			fprintf(record, "Exit recorded at location: %d, at time %ld, with duration %d seconds. Duration over maximum alloted time by %d seconds.\n"
					, location, current_time, duration, duration - MAX_TIME);
		else // if normal exit
			fprintf(record, "Exit recorded at location: %d, at time %ld, with duration %d seconds.\n", location, current_time, duration);
	}
	else // if normal entry;
	{ // parkingLot number
		enterF(map, location);
		usage ++;
		fprintf(record, "Entry recorded at location: %d, at time %ld. Car number %d.\n", location, current_time, usage);
	}
	fclose(record);
	return 0;
}

int testmain()
{
	ParkingSpace map[TOTAL_SLOTS];
	for (int i = 0; i < TOTAL_SLOTS; i ++)
	{
		map[i].occupy = UNOCCUPIED;
		map[i].timeStamp = UNOCCUPIED;
	}
	logError(__LINE__);
	time_t start, end;
	
	time(&start);
	input(map, 0);
	sleep(2);
	time(&start);
	input(map, 4);
	sleep(5);
	time(&start);
	input(map, 3);
	sleep(1);
	time(&start);
	input(map, 11);
	sleep(5);
	input(map, 1);
	sleep(9);
	input(map,1);
	sleep(2);
	input(map,11);
	exit(0);
	return 0;
}

int test2main()
{
	int startTime;
	int currTime;
	//Boot up
	while (shutdown = false)
	{
		//read time
		// compare
		// if on for certain amount of time, call reset oled function
		
		// ask if there is an input from the entrances
		// call shortest path function - > which calls input - > which calls everything else
		// if exit, call chris function - > which calls exit 
		// we can implement time interval statistics using currtime - start time % something
		// if currtime - starttime is big enough, call shutdown, set shutdown to true
	}
	return 0;
}

int main{
	return 0;
}
/* take in filename
	change last digit of filename to +1 of whatever
	return filename
	craete file with that filename
*/


				
