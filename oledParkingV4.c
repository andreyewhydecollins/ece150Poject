#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

#include <oled-exp.h>
#include <ugpio/ugpio.h>

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
	uint8_t byteRow = aRow / 8;						//Refer Above
	uint16_t targetByte = 128*byteRow + aCol;				//Refer Above
	uint8_t targetBit = aRow % 8;						//Refer Above
	*(dbuffer+targetByte) = (*(dbuffer+targetByte)&(~bitMask[targetBit]));	//Bitwise 'and' with the complement of the target bit to 											//remove given pixel
}


//Draws the left initial parking boundaries to buffer
void drawStartPB()
{
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
	oledSetCursorByPixel(0,0);
	drawPS(aParkSpace);
	oledDraw(tBuffer, 1024);
	displayText();
}

//Clears given Parking space
void clearPS(uint8_t aParkSpace)
{
	oledSetCursorByPixel(0,0);
	removePS(aParkSpace);
	oledDraw(tBuffer, 1024);
	displayText();
}

//Draws given path
void setPath(uint8_t anEntrance, uint8_t aParkSpace)
{
	oledSetCursorByPixel(0,0);
	drawReserve(anEntrance, aParkSpace);
	oledDraw(tBuffer, 1024);
	displayText();
}

//Clears given path
void clearPath(uint8_t anEntrance, uint8_t aParkSpace)
{
	oledSetCursorByPixel(0,0);
	removeReserve(anEntrance, aParkSpace);
	oledDraw(tBuffer, 1024);
	displayText();
}

//Declaration of gpio's and direction (All inputs)
int gpioDeclare ()
{
	uint8_t rv;
	uint8_t rq;

	// check if gpio is already exported
	if ((rq = gpio_is_requested(1)) < 0)
	{
		perror("gpio_is_requested");
		return EXIT_FAILURE;
	}
	if ((rq = gpio_is_requested(E0)) < 0)
	{
		perror("gpio_is_requested");
		return EXIT_FAILURE;
	}
	if ((rq = gpio_is_requested(E1)) < 0)
	{
		perror("gpio_is_requested");
		return EXIT_FAILURE;
	}
	if ((rq = gpio_is_requested(P0)) < 0)
	{
		perror("gpio_is_requested");
		return EXIT_FAILURE;
	}
	if ((rq = gpio_is_requested(P1)) < 0)
	{
		perror("gpio_is_requested");
		return EXIT_FAILURE;
	}
	if ((rq = gpio_is_requested(P2)) < 0)
	{
		perror("gpio_is_requested");
		return EXIT_FAILURE;
	}
	if ((rq = gpio_is_requested(P4)) < 0)
	{
		perror("gpio_is_requested");
		return EXIT_FAILURE;
	}
	if ((rq = gpio_is_requested(P5)) < 0)
	{
		perror("gpio_is_requested");
		return EXIT_FAILURE;
	}
	if ((rq = gpio_is_requested(P6)) < 0)
	{
		perror("gpio_is_requested");
		return EXIT_FAILURE;
	}
	//Declare gpio
	if ((rv = gpio_request(1, NULL)) < 0)
	{
		perror("gpio_request");
		return EXIT_FAILURE;
	}
	if ((rv = gpio_request(E0, NULL)) < 0)
	{
		perror("gpio_request");
		return EXIT_FAILURE;
	}
	if ((rv = gpio_request(E1, NULL)) < 0)
	{
		perror("gpio_request");
		return EXIT_FAILURE;
	}
	if ((rv = gpio_request(P0, NULL)) < 0)
	{
		perror("gpio_request");
		return EXIT_FAILURE;
	}
	if ((rv = gpio_request(P1, NULL)) < 0)
	{
		perror("gpio_request");
		return EXIT_FAILURE;
	}
	if ((rv = gpio_request(P2, NULL)) < 0)
	{
		perror("gpio_request");
		return EXIT_FAILURE;
	}
	if ((rv = gpio_request(P4, NULL)) < 0)
	{
		perror("gpio_request");
		return EXIT_FAILURE;
	}
	if ((rv = gpio_request(P5, NULL)) < 0)
	{
		perror("gpio_request");
		return EXIT_FAILURE;
	}
	if ((rv = gpio_request(P6, NULL)) < 0)
	{
		perror("gpio_request");
		return EXIT_FAILURE;
	}

	// set to input direction
	printf("> setting to input\n");
	if ((rv = gpio_direction_input(1)) < 0)
	{
		perror("gpio_direction_input");
	}
	if ((rv = gpio_direction_input(E0)) < 0)
	{
		perror("gpio_direction_input");
	}
	if ((rv = gpio_direction_input(E1)) < 0)
	{
		perror("gpio_direction_input");
	}
	if ((rv = gpio_direction_input(P0)) < 0)
	{
		perror("gpio_direction_input");
	}
	if ((rv = gpio_direction_input(P1)) < 0)
	{
		perror("gpio_direction_input");
	}
	if ((rv = gpio_direction_input(P2)) < 0)
	{
		perror("gpio_direction_input");
	}
	if ((rv = gpio_direction_input(P4)) < 0)
	{
		perror("gpio_direction_input");
	}
	if ((rv = gpio_direction_input(P5)) < 0)
	{
		perror("gpio_direction_input");
	}
	if ((rv = gpio_direction_input(P6)) < 0)
	{
		perror("gpio_direction_input");
	}

	return 0;
}

//OLED Display Data to track positions
void initializeDisplayInfo()
{
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
		if(i == 0 || i == 1 || i == 2 || i == 4 || i == 5 || i == 6)
		{
			display.PLArray[i] = UNOCCUPIED;
		}
		else
		{
			setPS(i);
			display.PLArray[i] = OCCUPIED;
		}
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
	if(display.PLArray[input-1] == UNOCCUPIED || display.PLArray[input-1] == RESERVED)	//Occupies given parking space if empty or
	{											//reserved
		for(uint8_t i = 0; i < NUMENTRANCE; i++)
		{
			if(display.spot[i] == input-1)						//If reserved remove reservation line
			{
				oledSetDisplayPower(0);
				clearPath(i, input-1);
				display.spotCounter[input-1] = 0;
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
	initializePL();			//Initialize Parking Lot (OLED And Co-ordinates)
	
	initializeDisplayInfo();	//Initialize OLED Display info

	int input = 1;

	if(gpioDeclare())		//Declare GPIOS
	{
		perror("ERROR");
		return -1;
	}

	while(input != 0)		//Loop until Exited
	{
		bool loop = true;
		while(loop)		//Loops for valid input
		{
			usleep(10000);
			for(uint8_t i = 0; i < NUMENTRANCE; i++)	//Removes path to Parking space if beyond given threshold
			{
				if(display.ENTArray[i])
				{
					display.spotCounter[i] ++;
				}
				if(display.spotCounter[i] > 500)
				{
					clearPath(i, display.spot[i]);
					display.PLArray[display.spot[i]] = UNOCCUPIED;
					display.spotCounter[i] = 0;
					display.ENTArray[i] = false;
				}
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
				input = -1;
				loop = false;
			}
			else if((!display.ENTArray[1]) && gpio_get_value(E1) == 0)
			{
				input = -2;
				loop = false;
			}

			else if(gpio_get_value(P0) != display.PLStatus[0])
			{
				input = 1;
				display.PLStatus[0] = gpio_get_value(P0);
				loop = false;
			}

			else if(gpio_get_value(P1) != display.PLStatus[1])
			{
				input = 2;
				display.PLStatus[1] = gpio_get_value(P1);
				loop = false;
			}
			else if(gpio_get_value(P2) != display.PLStatus[2])
			{
				input = 3;
				display.PLStatus[2] = gpio_get_value(P2);
				loop = false;
			}
			else if(gpio_get_value(P4) != display.PLStatus[4])
			{
				input = 5;
				display.PLStatus[4] = gpio_get_value(P4);
				loop = false;
			}	
			else if(gpio_get_value(P5) != display.PLStatus[5])
			{
				input = 6;
				display.PLStatus[5] = gpio_get_value(P5);
				loop = false;
			}
			else if(gpio_get_value(P6) != display.PLStatus[6])
			{
				input = 7;
				display.PLStatus[6] = gpio_get_value(P6);
				loop = false;
			}		
		}

		if(input > 0)	//If Parking Space Input
		{
			PSInput(input);
		}
		if(input < 0)	//If Entrance Input
		{
			input = -input;
			EInput(input);
		}
	}

	oledClear();			//Clear OLED information
	oledSetDisplayPower(0);
}