#include <stdio.h>                      // for sprintf()
#include <stdbool.h>                    // for data type bool
#include <plib.h>                       // Peripheral Library
#include <time.h>
#include <adc10.h>
#include "PmodOLED.h"
#include "OledChar.h"
#include "OledGrph.h"
#include "delay.h"

// Digilent board configuration
#pragma config ICESEL       = ICS_PGx1  // ICE/ICD Comm Channel Select
#pragma config DEBUG        = OFF       // Debugger Disabled for Starter Kit
#pragma config FNOSC        = PRIPLL	// Oscillator selection
#pragma config POSCMOD      = XT	// Primary oscillator mode
#pragma config FPLLIDIV     = DIV_2	// PLL input divider
#pragma config FPLLMUL      = MUL_20	// PLL multiplier
#pragma config FPLLODIV     = DIV_1	// PLL output divider
#pragma config FPBDIV       = DIV_8	// Peripheral bus clock divider
#pragma config FSOSCEN      = OFF	// Secondary oscillator enable

struct Ball
    {
        int Posx;
        int Posy;
        int Velx;
        int Vely;
        int Direction;
        //sets up the ball for movement as a global variable
    };
    
int ADC_meanA, ADC_meanB;
//initializes the mean values for the ADC
#define NUM_ADC_SAMPLES 32
#define LOG2_NUM_ADC_SAMPLES 5

// Function prototypes
void Initialize();
bool getInput1();
bool getInput2();
void OledStringCursor(char *buf, int x, int y);
void Timer2Init();
void Timer4Init();
void debounce();
void putPongLines();
void putPongLines1P();
void putScore1P(int score);
void putScore2P(int score1, int score2);
bool ballMovement2(int score1, int score2, struct Ball balls);
int ballMovement1(int reqscore, struct Ball balls);
void gameOver(bool winner);
void initADC();

void __ISR(_ADC_VECTOR, IPL7SRS) _ADCHandler(void) {
    static unsigned int current_reading_A = 0;
    static unsigned int current_reading_B = 0;
    static unsigned int ADC_ReadingsA[NUM_ADC_SAMPLES];
    static unsigned int ADC_ReadingsB[NUM_ADC_SAMPLES];
    unsigned int i = 0;
    unsigned int total = 0;
    //initializes static variables for the readings and arrays, so they stay the same each time the function is called
    
        if (ReadActiveBufferADC10() == 1)
        {
            if(current_reading_A%2 == 0)
            {
                ADC_ReadingsA[current_reading_A] = ReadADC10(0);
                current_reading_A++;
            }
            else
            {
                ADC_ReadingsB[current_reading_B] = ReadADC10(1);
                current_reading_B++;
            }
            //if the current buffer is the second buffer, alternates to the first buffer and alternates between the current readings
        }
        else
        {
            if(current_reading_B%2 == 0)
            {
                ADC_ReadingsA[current_reading_A] = ReadADC10(8);
                current_reading_A++;
            }
            else
            {
                ADC_ReadingsB[current_reading_B] = ReadADC10(9);
                current_reading_B++;
            }
            //if the current buffer is the first buffer, alternates to the second buffer and alternates between the current readings
        }
    if (current_reading_A == NUM_ADC_SAMPLES)
    {
        current_reading_A = 0;
        total = 0;
        for (i = 0; i < NUM_ADC_SAMPLES; i++) {
            total += ADC_ReadingsA[i];
        }
        ADC_meanA = total >> 5; // divide by num of samples
        //reads in all the current readings and divides it by 32
    }
    if (current_reading_B == NUM_ADC_SAMPLES)
    {
        current_reading_B = 0;
        total = 0;
        for (i = 0; i < NUM_ADC_SAMPLES; i++) {
            total += ADC_ReadingsB[i];
        }
        ADC_meanB = total >> 5; // divide by num of samples
        //reads in all the current readings and divides it by 32
    }
    INTClearFlag(INT_AD1);
    //clears the interrupt flag
}

void Timer2Init() 
{     
   OpenTimer2(T2_ON | T2_IDLE_CON | T2_SOURCE_INT | T2_PS_1_16 | T2_GATE_OFF, 62499);     
   return; 
}//Opens a 100ms timer

void Timer4Init() 
{     
   OpenTimer4(T4_ON | T4_IDLE_CON | T4_SOURCE_INT | T4_PS_1_16 | T4_GATE_OFF, 31249);     
   return; 
}////Opens a 50ms timer

int main()
{
    enum states {Init, PlayerSel1, PlayerSel2, Target1, Target2, Target3, Confirm1, Confirm2, Game};
    enum states Menu = Init;
    int reqScore;
    bool TwoPlayers;
    struct Ball Ball1;
    char buf[17];
    Initialize();
    //initializes all the variables needed outside of the while loop
    
    
    while(1)
    {
    int score1 = 0;
    int score2 = 0;
    bool gameFin = false;
    bool moved = false;
    //Sets the game to its initial state where no one has scored
        while(!gameFin)
        {
            int randomNum = ReadADC10(0)+ReadADC10(1)+ReadADC10(8)+ReadADC10(9);
            int randomVal = randomNum%6;
            int rand2 = randomNum%2+1;
            //ADC generates pseudorandom numbers constantly during the game
            Ball1.Direction = randomVal;
            switch (Ball1.Direction)
            {
                case 0:
                    Ball1.Vely = 0;
                    Ball1.Velx = rand2;
                    break;
                    //Ball will move directly towards the 1P paddle
                case 1:
                    Ball1.Vely = rand2;
                    Ball1.Velx = rand2;
                    break;
                    //Ball will move diagonally upward towards the 1P paddle
                case 2:
                    Ball1.Vely = rand2;
                    Ball1.Velx = -rand2;
                    break;
                    //Ball will move diagonally upward away from the 1P paddle
                case 3:
                    Ball1.Vely = 0;
                    Ball1.Velx = -rand2;
                    break;
                    //Ball will move directly away from the 1P paddle
                case 4:
                    Ball1.Vely = -rand2;
                    Ball1.Velx = -rand2;
                    break;
                    //Ball will move diagonally downward away from the 1P paddle
                case 5:
                    Ball1.Vely = -rand2;
                    Ball1.Velx = rand2;
                    break;
                    //Ball will move diagonally downward towards the 1P paddle
            }
            switch (Menu)
            {
                case Init:
                    OledClearBuffer();
                    Ball1.Posx = 63;
                    Ball1.Posy = 15;
                    Menu = PlayerSel1;
                    break;
                    //sets up the initial position of the ball after clearing the buffer
                case PlayerSel1:
                    OledClearBuffer();
                    moved = false;
                    //initializes ADC as centered and allows the player to choose 1P mode or 2P mode
                    sprintf(buf, "Player Selection", 0);
                    OledStringCursor(buf, 0, 0);
                    sprintf(buf, "-> 1P", 0);
                    OledStringCursor(buf, 0, 1);
                    sprintf(buf, "   2P", 0);
                    OledStringCursor(buf, 0, 2);
                    while(!moved)
                    {
                        if(ADC_meanB > 1000)
                        {
                            Menu = PlayerSel2;
                            moved = true; 
                            debounce();
                            
                        }
                        else if(ADC_meanB < 50)
                        {
                            Menu = PlayerSel2;
                            moved = true; 
                            debounce();
                        }
                        //If ADC is moved up or down, it moves the cursor to 2P option
                        else if(ADC_meanA > 1000)
                        {
                            TwoPlayers = false;
                            Menu = Target1;
                            moved = true; 
                            debounce();
                        }
                        //If ADC is moved to the right, it moves to the next menu
                    }
                    break;
                case PlayerSel2:
                    OledClearBuffer();
                    moved = false;
                    //initializes ADC as centered and allows the player to choose 1P mode or 2P mode
                    sprintf(buf, "Player Selection", 0);
                    OledStringCursor(buf, 0, 0);
                    sprintf(buf, "   1P", 0);
                    OledStringCursor(buf, 0, 1);
                    sprintf(buf, "-> 2P", 0);
                    OledStringCursor(buf, 0, 2);
                    while(!moved)
                    {
                        if(ADC_meanB > 1000)
                        {
                            Menu = PlayerSel1;
                            moved = true; 
                            debounce();
                        }
                        else if(ADC_meanB < 50)
                        {
                            Menu = PlayerSel1;
                            moved = true; 
                            debounce();
                        }
                        //If ADC is moved up or down, it moves the cursor to 1P option                        
                        else if(ADC_meanA > 1000)
                        {
                            TwoPlayers = true;
                            Menu = Target1;
                            moved = true; 
                            debounce();
                        }
                        //If ADC is moved to the right, it moves to the next menu
                    }
                    break;
                case Target1:
                    OledClearBuffer();
                    moved = false;
                    sprintf(buf, "Target Score", 0);
                    OledStringCursor(buf, 0, 0);
                    sprintf(buf, "-> 3", 0);
                    OledStringCursor(buf, 0, 1);
                    sprintf(buf, "   5", 0);
                    OledStringCursor(buf, 0, 2);
                    sprintf(buf, "   9", 0);
                    OledStringCursor(buf, 0, 3);
                    //Allows user to select their target score. For 1P, this is the number of times 
                    //the ball hits the paddle; for 2P this is the number of times the opposing player is scored on
                    while(!moved)
                    {
                        if(ADC_meanB > 1000)
                        {
                            Menu = Target3;
                            moved = true; 
                            debounce();
                            //if the joystick is moved up, cursor will move to 9
                        }
                        else if(ADC_meanB < 50)
                        {
                            Menu = Target2;
                            moved = true; 
                            debounce();
                            //if the joystick is moved down, cursor will move to 5
                        }
                        else if(ADC_meanA > 1000)
                        {
                            reqScore = 3;
                            Menu = Confirm1;
                            moved = true; 
                            debounce();
                            //if the joystick is moved right, required score is 3 and will move to the next menu
                        }
                    }
                    break;

                case Target2:
                    OledClearBuffer();
                    moved = false;
                    sprintf(buf, "Target Score", 0);
                    OledStringCursor(buf, 0, 0);
                    sprintf(buf, "   3", 0);
                    OledStringCursor(buf, 0, 1);
                    sprintf(buf, "-> 5", 0);
                    OledStringCursor(buf, 0, 2);
                    sprintf(buf, "   9", 0);
                    OledStringCursor(buf, 0, 3);
                    while(!moved)
                    {
                        if(ADC_meanB > 1000)
                        {
                            Menu = Target1;
                            moved = true; 
                            debounce();
                            //if the joystick is moved up, cursor will move to 3
                        }
                        else if(ADC_meanB < 50)
                        {
                            Menu = Target3;
                            moved = true; 
                            debounce();
                            //if the joystick is moved down, cursor will move to 9
                        }
                        else if(ADC_meanA > 1000)
                        {
                            reqScore = 5;
                            Menu = Confirm1;
                            moved = true; 
                            debounce();
                            //if the joystick is moved right, required score is 5 and will move to the next menu
                        }
                    }
                    break;

                case Target3:
                    OledClearBuffer();
                    moved = false;
                    sprintf(buf, "Target Score", 0);
                    OledStringCursor(buf, 0, 0);
                    sprintf(buf, "   3", 0);
                    OledStringCursor(buf, 0, 1);
                    sprintf(buf, "   5", 0);
                    OledStringCursor(buf, 0, 2);
                    sprintf(buf, "-> 9", 0);
                    OledStringCursor(buf, 0, 3);
                    while(!moved)
                    {
                        if(ADC_meanB > 1000)
                        {
                            Menu = Target2;
                            moved = true; 
                            debounce();
                            //if the joystick is moved up, cursor will move to 5
                        }
                        else if(ADC_meanB < 50)
                        {
                            Menu = Target1;
                            moved = true; 
                            debounce();
                            //if the joystick is moved down, cursor will move to 3
                        }
                        else if(ADC_meanA > 1000)
                        {
                            reqScore = 9;
                            Menu = Confirm1;
                            moved = true; 
                            debounce();
                            //if the joystick is moved right, required score is 9 and will move to the next menu
                        }
                    }
                    break;
                case Confirm1:
                    OledClearBuffer();
                    moved = false;
                    sprintf(buf, "Are You Sure?", 0);
                    OledStringCursor(buf, 0, 0);
                    sprintf(buf, "-> Yes", 0);
                    OledStringCursor(buf, 0, 1);
                    sprintf(buf, "   No", 0);
                    OledStringCursor(buf, 0, 2);
                    while(!moved)
                    {
                        if(ADC_meanB > 1000)
                        {
                            Menu = Confirm2;
                            moved = true; 
                            debounce();
                        }
                        else if(ADC_meanB < 50)
                        {
                            Menu = Confirm2;
                            moved = true; 
                            debounce();
                        }
                        //if joystick is moved up or down, the cursor moves to "no"
                        else if(ADC_meanA > 1000)
                        {
                            Menu = Game;
                            moved = true; 
                            debounce();
                            //if joystick is moved right, the countdown begins
                        }
                    }
                    break;
                case Confirm2:
                    OledClearBuffer();
                    moved = false;
                    sprintf(buf, "Are You Sure?", 0);
                    OledStringCursor(buf, 0, 0);
                    sprintf(buf, "   Yes", 0);
                    OledStringCursor(buf, 0, 1);
                    sprintf(buf, "-> No", 0);
                    OledStringCursor(buf, 0, 2);
                    while(!moved)
                    {
                        if(ADC_meanB > 1000)
                        {
                            Menu = Confirm1;
                            moved = true; 
                            debounce();
                        }
                        else if(ADC_meanB < 50)
                        {
                            Menu = Confirm1;
                            moved = true; 
                            debounce();
                        }
                        //if joystick is moved up or down, the cursor moves to "yes"
                        else if(ADC_meanA > 1000)
                        {
                            Menu = PlayerSel1;
                            moved = true; 
                            debounce();
                            //if joystick is moved right, the menu moves back to player selection
                        }
                    }
                    break;
                case Game:
                    if(TwoPlayers)//2P game
                    {
                        while(score1 < reqScore && score2 < reqScore)//Check for required score
                        {
                            bool whoScored = ballMovement2(score1, score2, Ball1);
                            //moves the ball around the screen and returns true for a 1P score and false otherwise, then adds it to the player score
                            if(whoScored)
                                score1++;
                            else score2++;
                            OledUpdate();
                        }
                        if(score1 > score2)//outside while loop, if score 1 is greater, displays the winner
                        {
                            gameOver(0);
                            gameFin = true;
                            TMR4 = 0x0;
                            Timer4Init();
                            int timer4_past = ReadTimer4();
                            int displayed = 0;
                            while (displayed < 100)
                            {
                                int timer4_cur = ReadTimer4();
                                if (timer4_past > timer4_cur)
                                {
                                    displayed++;
                                }
                                timer4_past = timer4_cur;
                            }
                            //displays winner for 5 seconds, then returns to player select
                            Menu = PlayerSel1;
                        }
                        else//outside while loop, if score 1 is greater, displays the winner
                        {
                            gameOver(1);
                            gameFin = true;
                            TMR4 = 0x0;
                            Timer4Init();
                            int timer4_past = ReadTimer4();
                            int displayed = 0;
                            while (displayed < 100)
                            {
                                int timer4_cur = ReadTimer4();
                                if (timer4_past > timer4_cur)
                                {
                                    displayed++;
                                }
                                timer4_past = timer4_cur;
                            }
                            //displays winner for 5 seconds, then returns to player select
                            Menu = PlayerSel1;
                        } 
                    }
                    else//1P mode has been selected
                    {
                        int finalScore = 0;
                        while(finalScore < reqScore)
                        {
                            finalScore = ballMovement1(reqScore, Ball1);
                            //returns the score from the 1P ball movement function
                        }
                        gameOver(0);
                        gameFin = true;
                        TMR4 = 0x0;
                        Timer4Init();
                        int timer4_past = ReadTimer4();
                        int displayed = 0;
                        while (displayed < 100)
                        {
                            int timer4_cur = ReadTimer4();
                            if (timer4_past > timer4_cur)
                            {
                                displayed++;
                            }
                            timer4_past = timer4_cur;
                        }
                        //displays the winning message after the player wins
                        Menu = PlayerSel1;
                    }
                    break;
            }
        }
    }
}


/////////////////////////////////////////////////////////////////
// Function:    getInput
// Description: Perform a nonblocking check to see if BTN1 has been pressed
// Inputs:      None
// Returns:     TRUE if 0-to-1 transition of BTN1 is detected;
//                otherwise return FALSE
//
bool getInput1()
{
    enum Button1Position {UP, DOWN}; // Possible states of BTN1
    
    static enum Button1Position button1CurrentPosition = UP;  // BTN1 current state
    static enum Button1Position button1PreviousPosition = UP; // BTN1 previous state
    // Reminder - "static" variables retain their values from one call to the next.
    button1PreviousPosition = button1CurrentPosition;
    // Read BTN1
    if(PORTG & 0x40)                                
    {
        button1CurrentPosition = DOWN;
    } else
    {
        button1CurrentPosition = UP;
    } 
    if((button1CurrentPosition == DOWN) && (button1PreviousPosition == UP))
    {
        TMR4 = 0x0;
        Timer4Init();
        int timer4_past = ReadTimer4();
        bool debouncing = true;
        while (debouncing)
        {
            int timer4_cur = ReadTimer4();
            if (timer4_past > timer4_cur)
            {
                debouncing = false;
            }
            timer4_past = timer4_cur;
        }
        return TRUE; // 0-to-1 transition has been detected
    }
    return FALSE;    // 0-to-1 transition not detected
}
bool getInput2()
{
    enum Button2Position {UP, DOWN}; // Possible states of BTN1
    
    static enum Button2Position button2CurrentPosition = UP;  // BTN1 current state
    static enum Button2Position button2PreviousPosition = UP; // BTN1 previous state
    // Reminder - "static" variables retain their values from one call to the next.
    
    button2PreviousPosition = button2CurrentPosition;
    // Read BTN1
    if(PORTG & 0x80)                                
    {
        button2CurrentPosition = DOWN;
    } else
    {
        button2CurrentPosition = UP;
    } 
    if((button2CurrentPosition == DOWN) && (button2PreviousPosition == UP))
    {
        TMR4 = 0x0;
        Timer4Init();
        int timer4_past = ReadTimer4();
        bool debouncing = true;
        while (debouncing)
        {
            int timer4_cur = ReadTimer4();
            if (timer4_past > timer4_cur)
            {
                debouncing = false;
            }
            timer4_past = timer4_cur;
        }
        return TRUE; // 0-to-1 transition has been detected
    }
    return FALSE;    // 0-to-1 transition not detected
}

//Debounces for 50ms
void debounce()
{
    TMR4 = 0x0;
    Timer4Init();
    int timer4_past = ReadTimer4();
    bool debouncing = true;
    while (debouncing)
    {
        int timer4_cur = ReadTimer4();
        if (timer4_past > timer4_cur)
        {
            debouncing = false;
        }
        timer4_past = timer4_cur;
    }
}

/////////////////////////////////////////////////////////////////
// Function:     Initialize
// Description:  Initialize the system
// Inputs:       None
// Return value: None
//
void Initialize()
{
   // Initialize GPIO for all LEDs
   TRISGSET = 0x40;     // For BTN1: configure PortG bit for input
   TRISGSET = 0x80;     // For BTN2: configure PortG bit for input
   TRISGCLR = 0xf000;   // For LEDs 1-4: configure PortG pins for output
   ODCGCLR  = 0xf000;   // For LEDs 1-4: configure as normal output (not open drain)

   // Initialize Timer1 and OLED
   DelayInit();
   OledInit();
   initADC();
   
   INTConfigureSystem(INT_SYSTEM_CONFIG_MULT_VECTOR);

   INTEnableInterrupts();
   
   TRISBSET = 0x0C;
   TRISBCLR = 0x40; 
   LATBCLR = 0x40;
   PORTB = PORTB | 0x40;
    // Initializes portB to be an output, allowing for a second VCC

   // Send a welcome message to the OLED display
   OledClearBuffer();
   OledSetCursor(0, 0);
   OledPutString("ECE 2534");
   OledSetCursor(0, 1);
   OledPutString("PONG");
   OledSetCursor(0, 2);
   OledPutString("Josh Rehm");
   OledUpdate();
    TMR4 = 0x0;
    Timer4Init();
    int timer4_past = ReadTimer4();
    int displayed = 0;
    while (displayed < 100)
    {
        int timer4_cur = ReadTimer4();
        if (timer4_past > timer4_cur)
        {
            displayed++;
        }
        timer4_past = timer4_cur;
    }
   return;
}

//Places a string, but not super helpful due to the OledUpdate() function
void OledStringCursor(char *buf, int x, int y){
    OledSetCursor(x, y);
    OledPutString(buf);
    OledUpdate();
}

//Draws all the pong lines for the top, bottom and middle lines
void putPongLines()
{
    OledClearBuffer();
    OledMoveTo(0,0);
    OledLineTo(127,0);
    OledMoveTo(127,1);
    OledLineTo(0,1);
    OledMoveTo(0,30);
    OledLineTo(127,30);
    OledMoveTo(127,31);
    OledLineTo(0,31);
    OledMoveTo(63,2);
    OledDrawPixel();
    OledMoveTo(63,4);
    OledDrawPixel();
    OledMoveTo(63,6);
    OledDrawPixel();
    OledMoveTo(63,8);
    OledDrawPixel();
    OledMoveTo(63,10);
    OledDrawPixel();
    OledMoveTo(63,12);
    OledDrawPixel();
    OledMoveTo(63,14);
    OledDrawPixel();
    OledMoveTo(63,16);
    OledDrawPixel();
    OledMoveTo(63,18);
    OledDrawPixel();
    OledMoveTo(63,20);
    OledDrawPixel();
    OledMoveTo(63,22);
    OledDrawPixel();
    OledMoveTo(63,24);
    OledDrawPixel();
    OledMoveTo(63,26);
    OledDrawPixel();
    OledMoveTo(63,28);
    OledDrawPixel();
}

//Places an extra wall for 1P mode
void putPongLines1P()
{
    OledMoveTo(0,2);
    OledLineTo(0,30);
    OledMoveTo(1,2);
    OledLineTo(1,30);
}

//Outputs the score each time a player scores in 2P mode
void putScore2P(int score1, int score2)
{
    char buf[17];
    sprintf(buf, "%d", score1);
    OledSetCursor(6, 1);
    OledPutString(buf);
    sprintf(buf, "%d", score2);
    OledSetCursor(9, 1);
    OledPutString(buf);
    OledUpdate();
    TMR4 = 0x0;
    Timer4Init();
    int timer4_past = ReadTimer4();
    int displayed = 0;
    while (displayed < 20)
    {
        int timer4_cur = ReadTimer4();
        if (timer4_past > timer4_cur)
        {
            displayed++;
        }
        timer4_past = timer4_cur;
    }
}

bool ballMovement2(int score1, int score2, struct Ball balls)
{
    //Returns 0 if P1 scores, returns 1 if P2 scores
    bool scored1 = false;
    bool scored2 = false;
    char buf[17];
    putScore2P(score1,score2);
    int randomNum = ReadADC10(0)+ReadADC10(1)+ReadADC10(8)+ReadADC10(9);
    int randomVal = randomNum%6;
    int rand2 = randomNum%2+1;
    balls.Direction = randomVal;
    switch (balls.Direction)
    {
        case 0:
            balls.Vely = 0;
            balls.Velx = rand2;
            break;
        case 1:
            balls.Vely = rand2;
            balls.Velx = rand2;
            break;
        case 2:
            balls.Vely = rand2;
            balls.Velx = -rand2;
            break;
        case 3:
            balls.Vely = 0;
            balls.Velx = -rand2;
            break;
        case 4:
            balls.Vely = -rand2;
            balls.Velx = -rand2;
            break;
        case 5:
            balls.Vely = -rand2;
            balls.Velx = rand2;
            break;
    }
    //determines the velocity and the direction of the ball by using the ADC as a pseudorandom input
    int paddle1pos = 11;
    int paddle2pos = 11;
    //sets the initial position for the paddles
    TMR2 = 0x0;
    Timer2Init();
    int past = ReadTimer2();
    int count = 58;
    while(count > 2)
    {
        int current = ReadTimer2();
        if (past > current){
            count--;
            putPongLines();
            OledMoveTo(0, paddle1pos);
            OledLineTo(0, paddle1pos + 5);
            OledMoveTo(1, paddle1pos);
            OledLineTo(1, paddle1pos + 5);
            OledMoveTo(127, paddle2pos);
            OledLineTo(127, paddle2pos + 5);
            OledMoveTo(126, paddle2pos);
            OledLineTo(126, paddle2pos + 5);
            sprintf(buf, "%d", count/10);
            OledStringCursor(buf, 2, 1);
            OledUpdate();
            //draws the paddle each time the timer is polled, while displaying the countdown
        }
        past = current; 
        if(getInput1())
        {
            if(paddle1pos < 24)
            paddle1pos += 2;
        }
        //if button 1 is pressed, the paddle will move down by 2px, unless it hits the bottom of the screen
        if(getInput2())
        {
            if(paddle1pos > 2)
            paddle1pos -= 2;
        }
        //if button 2 is pressed, the paddle will move up by 2px, unless it hits the top of the screen
        if(ADC_meanB < 50)
        {
            if(paddle2pos < 24)
            paddle2pos++;
            debounce();
        }
        //if the joystick is in the down position, moves down by a pixel and debounces
        if(ADC_meanB > 1000)
        {
            if(paddle2pos > 2)
            paddle2pos --;
            debounce();
        }
        //if the joystick is in the up position, moves up by a pixel and debounces
    }
    
    while(!scored1 && !scored2)
    {
        bool up1 = false;
        bool down1 = false;
        bool up2 = false;
        bool down2 = false;
        putPongLines();
        if(getInput1())
        {
            if(paddle1pos < 24)
            paddle1pos += 2;
            down1 = true;
        }
        if(getInput2())
        {
            if(paddle1pos > 2)
            paddle1pos -= 2;
            up1 = true;
        }
        if(ADC_meanB < 50)
        {
            if(paddle2pos < 24)
            paddle2pos ++;
            down2 = true;
            debounce();
        }
        if(ADC_meanB > 1000)
        {
            if(paddle2pos > 2)
            paddle2pos --;
            up2 = true;
            debounce();
        }
        //joystick and buttons work the same as during the countdown, but also set a boolean value to check if it has moved during this iteration
        OledMoveTo(0, paddle1pos);
        OledLineTo(0, paddle1pos + 5);
        OledMoveTo(1, paddle1pos);
        OledLineTo(1, paddle1pos + 5);
        OledMoveTo(127, paddle2pos);
        OledLineTo(127, paddle2pos + 5);
        OledMoveTo(126, paddle2pos);
        OledLineTo(126, paddle2pos + 5);
        //moves the paddle to the input paddle position
        OledMoveTo( balls.Posx, balls.Posy);
        OledDrawPixel();
        //draws the new ball's position after it has moved
        OledUpdate();
        debounce();
        //debounces for 50ms
        
        if(balls.Posx == 2 || (balls.Posx == 3 && balls.Velx == -2))
        {
            OledMoveTo(balls.Posx + balls.Velx, balls.Posy);
            bool pixval = OledGetPixel();
            //checks the value on the border where the paddle is and returns true if it hits the paddle
            if(pixval)
            {
                balls.Velx *= -1;
                if(up1)
                    balls.Vely += 1;
                else if(down1)
                    balls.Vely -= 1;
                //if paddle 1 is moving during the while loop, the velocity will increment with its direction
            }
            else
                scored1 = true;
        }
        else if(balls.Posx == 125 || (balls.Posx == 124 && balls.Velx == 2))
        {
            OledMoveTo(balls.Posx + balls.Velx, balls.Posy);
            bool pixval = OledGetPixel();
            //checks the value on the border where the paddle is and returns true if it hits the paddle
            if(pixval)
            {
                balls.Velx *= -1;
                if(up2)
                    balls.Vely += 1;
                else if(down2)
                    balls.Vely -= 1;
                //if paddle 2 is moving during the while loop, the velocity will increment with its direction
            }
            else
                scored2 = true;
        }
        if(balls.Posy == 2 || (balls.Posy == 3 && balls.Vely == -2))
        {
            balls.Vely *= -1;
        }
        //reflects the ball off the the left paddle if it hits
        else if (balls.Posy == 29 || (balls.Posy == 28 && balls.Vely == 2))
        {
            balls.Vely *= -1;
        }
        //reflects the ball off the the right paddle if it hits
        balls.Posx += balls.Velx;
        balls.Posy += balls.Vely;
    }
    if(scored1)
        return false;
    else if(scored2)
        return true;
    //updates the score based on who hit the ball
}

int ballMovement1(int reqscore, struct Ball balls)
{
    int hits = 0;
    bool scored2 = false;
    char buf[17];
    int randomNum = ReadADC10(0)+ReadADC10(1)+ReadADC10(8)+ReadADC10(9);
    int randomVal = randomNum%6;
    int rand2 = randomNum%2+1;
    //initializes the hits at 0 and creates random numbers for velocity and direction
    balls.Direction = randomVal;
    switch (balls.Direction)
    {
        case 0:
            balls.Vely = 0;
            balls.Velx = rand2;
            break;
        case 1:
            balls.Vely = rand2;
            balls.Velx = rand2;
            break;
        case 2:
            balls.Vely = rand2;
            balls.Velx = -rand2;
            break;
        case 3:
            balls.Vely = 0;
            balls.Velx = -rand2;
            break;
        case 4:
            balls.Vely = -rand2;
            balls.Velx = -rand2;
            break;
        case 5:
            balls.Vely = -rand2;
            balls.Velx = rand2;
            break;
    }
    //sets the direction and velocity by using random numbers to determine each of them. Same as 2P
    int paddle2pos = 11;
    //initializes the paddle position at 11 before the countdown
    TMR2 = 0x0;
    Timer2Init();
    int past = ReadTimer2();
    int count = 58;
    while(count > 2)
    {
        int current = ReadTimer2();
        if (past > current){
            count--;
            putPongLines();
            putPongLines1P();
            OledMoveTo(127, paddle2pos);
            OledLineTo(127, paddle2pos + 5);
            OledMoveTo(126, paddle2pos);
            OledLineTo(126, paddle2pos + 5);
            sprintf(buf, "%d", count/10);
            OledSetCursor(2,1);
            OledPutString(buf);
        }
        past = current;
        OledUpdate();
        //goes through the countdown, updating the counter and the paddle position as it moves 
        
        if(ADC_meanB < 50)
        {
            if(paddle2pos < 24)
            paddle2pos ++;
            debounce();
        }
        if(ADC_meanB > 1000)
        {
            if(paddle2pos > 2)
            paddle2pos --;
            debounce();
        }
        //paddle will move up or down based on the joystick inputs
    }
    
    while(!scored2 && hits < reqscore)
    {
        bool up = false;
        bool down = false;
        randomNum = ReadADC10(0)+ReadADC10(1)+ReadADC10(8)+ReadADC10(9);
        putPongLines();
        putPongLines1P();
        if(ADC_meanB < 50)
        {
            if(paddle2pos < 24)
            paddle2pos ++;
            up = true;
            debounce();
        }
        if(ADC_meanB > 1000)
        {
            if(paddle2pos > 2)
            paddle2pos --;
            down = true;
            debounce();
        }
        //paddle will move up and down according to joystick inputs, and sets a bool if it has moved up or down during this iteration
        OledMoveTo(127, paddle2pos);
        OledLineTo(127, paddle2pos + 5);
        OledMoveTo(126, paddle2pos);
        OledLineTo(126, paddle2pos + 5);
        //changes paddle position
        sprintf(buf, "%d", hits);
        OledSetCursor(9, 1);
        OledPutString(buf);
        //displays the score for 1P, continuously updated
        OledMoveTo( balls.Posx, balls.Posy);
        OledDrawPixel();
        //draws the new position of the ball
        OledUpdate();
        debounce();
        
        if(balls.Posx == 2 || (balls.Posx == 3 && balls.Velx == -2))
        {
            OledMoveTo(balls.Posx + balls.Velx, balls.Posy);
            bool pixval = OledGetPixel();
            if(pixval)
            {
                balls.Velx *= -1;
            }
            //reflects the ball off the wall
        }
        else if(balls.Posx == 125 || (balls.Posx == 124 && balls.Velx == 2))
        {
            OledMoveTo(balls.Posx + balls.Velx, balls.Posy);
            bool pixval = OledGetPixel();
            if(pixval)
            {
                balls.Velx *= -1;
                if(up)
                    balls.Vely += 1;
                else if(down)
                    balls.Vely -= 1;
                hits++;
            }
            //if the ball hits the paddle, then the ball is reflected. If the paddle is moving, it will add its speed to the ball
            else
                scored2 = true;
        }
        if(balls.Posy == 2 || (balls.Posy == 3 && balls.Vely == -2))
        {
            balls.Vely *= -1;
        }
        else if (balls.Posy == 29 || (balls.Posy == 28 && balls.Vely == 2))
        {
            balls.Vely *= -1;
        }
        //if the ball hits either the top or the bottom wall, it is reflected
        balls.Posx += balls.Velx;
        balls.Posy += balls.Vely;
        //velocity values are added to the position for movement
    }
        return hits;
        //returns the score, or number of times the paddle is hit
}

//This function allows displays the winner in a 2P game
void gameOver(bool winner)
    {
        char buf[17];
        OledClearBuffer();
        if(winner)
        {
            sprintf(buf, "Player 2 Wins", 0);
            OledStringCursor(buf, 1, 1);
        }
        else
        {
            sprintf(buf, "Player 1 Wins", 0);
            OledStringCursor(buf, 1, 1);
        }
    }

#define AD_MUX_CONFIG ADC_CH0_POS_SAMPLEA_AN2 | ADC_CH0_NEG_SAMPLEA_NVREF | ADC_CH0_POS_SAMPLEB_AN3 | ADC_CH0_NEG_SAMPLEB_NVREF
//defines both mux A and mux B as the first two wires in the joystick

#define AD_CONFIG1 ADC_FORMAT_INTG | ADC_CLK_AUTO | ADC_AUTO_SAMPLING_ON
//sets the clock to automatically take samples and store them as integers

#define AD_CONFIG2 ADC_VREF_AVDD_AVSS | ADC_SCAN_OFF | \
                  ADC_SAMPLES_PER_INT_2 | \
                  ADC_BUF_8 | ADC_ALT_INPUT_ON
//Takes two samples per interrupt and stores them in alternating muxes

#define AD_CONFIG3 ADC_SAMPLE_TIME_8 | ADC_CONV_CLK_20Tcy
//sets the sample time for the interrupt

#define AD_CONFIGPORT ENABLE_ALL_DIG
//no analog ports

#define AD_CONFIGSCAN SKIP_SCAN_ALL
//skips all scanning

void initADC(void) {

    // Configure and enable the ADC HW
    SetChanADC10(AD_MUX_CONFIG);
    OpenADC10(AD_CONFIG1, AD_CONFIG2, AD_CONFIG3, AD_CONFIGPORT, AD_CONFIGSCAN);
    EnableADC10();

    // Set up, clear, and enable ADC interrupts
    INTSetVectorPriority(INT_ADC_VECTOR, INT_PRIORITY_LEVEL_7);
    INTClearFlag(INT_AD1);
    INTEnable(INT_AD1, INT_ENABLED);
}
