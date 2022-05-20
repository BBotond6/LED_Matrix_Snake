
/*
    Name:       Snake.ino
    Created:	20/05/2022 00:28:12
    Author:     BBotond6
*/


#include <MD_MAX72xx.h>
#include <SPI.h>

#define  delay_t  50  // in milliseconds

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

//Hardware SPI Arduino UNO
// CLK Pin  > 13 SCK
// Data Pin > 11 MOSI

#define CS_PIN    10

//JoyStick
#define joyXa A0
#define joyYa A1
#define joySWa 3
int joyX;
int joyY;
int joySW;
int joyDir; //JoyStick direction 0=none, 1=down, 2=up, 3=left, 4=right

// Hardware SPI connection
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

/////////////////////////////////////Remote controller variables and constans
#define PHOTOTRANSISTOR      2    //I/O port of the phototransistor.
#define NUM_OF_GAPS         33    //In one shot there are 33 gaps between pulses.

#define RECEPTION_TIMEOUT   70    //Time slot for the code bytes to arrive after the first rising edge (of the header).

#define MIN_H     4000      //Minimum value of the header gap in microseconds.
#define MAX_H     5500      //Maximum value of the header gap in microseconds.
#define MIN_0      450      //Minimum value of a gap meaning '0', in microseconds.
#define MAX_0      650     //Maximum value of a gap meaning '0', in microseconds.
#define MIN_1     1550      //Minimum value of a gap meaning '1', in microseconds.
#define MAX_1     1750     //Maximum value of a gap meaning '1', in microseconds.

#define MB_MASK     (byte)0x80  //0x80 = 0%10000000 - Mask for the most significant bit in a byte.
#define MOD8_MASK   (byte)0x07  //0x07 = 0%00000111 - Mask for the least 3 bits in a byte.
//With this mask the modulo division by 8 can be substituted.
//Note that (NUM % 8) is equivalent to (NUM & 0x07).

#define TRUE    1       //True value
#define FALSE   0       //False value

#define IR_ON   LOW       //Constants for the states of the phototransistor. The ON and OFF terms refer to
#define IR_OFF  HIGH      //the IR ray. When the infra LED of the remote controller is ON or OFF.

byte previous_state;        //Global variable of the previous state of the infra communication.
byte actual_state;          //Global variable of the actual state of the infra communication.
byte reception_started;     //Flag to signalize that the reception has been started.
byte reception_completed;   //Flag to signalize that the reception has been completed.
byte length_index;               //Index for the array of gap length data.

unsigned long timestamp_gap;            //Timestamp for a falling edge. Here starts the next gap between pulses.
                                        //Unit: microseconds.
unsigned long timestamp_reception;      //Timestamp for the beginning of the reception process. Milliseconds.

unsigned long gap_length[NUM_OF_GAPS];      //Time values of the gap lengths in microseconds.

//                                                   {0x84 , "ok"},
//                                                   {0x01 , "^"},
//                                                   {0x81 , "¡"},
//                                                   {0xB2 , ">"},
//                                                   {0x8A , "<"},
#define RemoteUp 0x01
#define RemoteDown 0x81
#define RemoteLeft 0x8A
#define RemoteRight 0xB2
#define RemoteOk 0x84

//Snake
byte sn[8][4] = {
  {0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00}
};

byte snak[8][32];
int x;
int y;
int bx;
int by;
int count;

unsigned int timestamp;
unsigned int bef_timestamp;

/*a számos mátrixba haladni 8 asával és minden sort egybõl kiiratni (ott helybe
létre hozni a B0010000 es formátumot)*/

// Switch between JoyStick and Remote controller
byte JRswitch;
unsigned int bef_timestamp_remote;    //the JoyStick's button dosn't work well so switch back with ok button

void setup() {
    // Switch between JoyStick and Remote controller
    JRswitch = 2;

    /////////////////////////////////////Remote controller setup
    byte idx;

    pinMode(PHOTOTRANSISTOR, INPUT);      //Configuring the phototransistor's pin as INPUT pin.

    previous_state = IR_OFF;        //Initial values of the state variables.
    actual_state = IR_OFF;

    reception_started = FALSE;      //Inital values for the process flags.
    reception_completed = FALSE;

    for (idx = 0; idx < NUM_OF_GAPS; idx++) { gap_length[idx] = 0; }    //Initializing the array of gap lengths.

    //Snake
    randomSeed(analogRead(0));
    pinMode(joyXa, INPUT);
    pinMode(joyYa, INPUT);
    pinMode(joySW, INPUT_PULLUP);
    joyX = 0;
    joyY = 0;
    joySW = 0;
    joyDir = 4;

    count = 1;

    mx.begin();
    mx.control(MD_MAX72XX::INTENSITY, 0);
    mx.clear();

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 32; j++) {
            snak[i][j] = 0;
        }
    }
    x = 2;
    y = 0;
    bx = 16;
    by = 16;
    snak[y][x] = 1;

    Serial.begin(38400);
    while (!Serial);
    GetFood();
    timestamp = millis();
    bef_timestamp = millis();
    bef_timestamp_remote = 0;
}

//Remote controll
byte decoding(unsigned long* p_gap_length, byte* p_code_byte)
{
    //Gap length data (32 pieces) are converted into 4 code bytes.
    byte byte_idx;    //Index for the code bytes: 0, 1, 2, 3.
    byte len_idx;     //Index for the gap length values: 1, 2, ..., 31, 32.

    for (byte_idx = 0; byte_idx < 4; byte_idx++)   //There are 4 code bytes.
    {
        p_code_byte[byte_idx] = 0;    //Clearing the code bytes.
    }

    for (len_idx = 1; len_idx < NUM_OF_GAPS; len_idx++)   //Processing the array of pulse distances.
    {
        byte_idx = (len_idx - 1) / 8;   //With fixpoint arithmetics this expression gives back the correct
                                        //byte index from the length index.
                                        //  1, 2, 3, 4, 5, 6, 7, 8 |-> 0
                                        //  9,10,11,12,13,14,15,16 |-> 1
                                        // 17,18,19,20,21,22,23,24 |-> 2
                                        // 25,26,27,28,29,30,31,32 |-> 3

        //Shall digit 1 be decoded?
        if ((MIN_1 < p_gap_length[len_idx]) && (p_gap_length[len_idx] < MAX_1))
        {
            //Assembly of the right code byte, bit by bit. Various algebraic expressions are given here.

            //p_code_byte[byte_idx] = p_code_byte[byte_idx] | ( 1 << (7 - (len_idx - 1) % 8) );
            //p_code_byte[byte_idx] = p_code_byte[byte_idx] | ( MB_MASK >> ((len_idx - 1) % 8) );
            p_code_byte[byte_idx] = p_code_byte[byte_idx] | (MB_MASK >> ((len_idx - 1) & MOD8_MASK));
        }
    }
    /*
    Serial.println("-----------------------------------");
    Serial.println(p_code_byte[2],HEX);
    Serial.println("-----------------------------------");
    */
    return p_code_byte[2];
}

byte GetRemoteSignal() {
    byte retur = 0;
    byte code_byte[4];               //Local array of the code bytes.
    unsigned long pulse_distance;           //Time difference between two consecutive pulses.

    actual_state = digitalRead(PHOTOTRANSISTOR);    //Reading the actual state of the phototransistor.

    //Hunting for the FIRST rising edge. This is the starting event of data reception.
    if ((previous_state == IR_OFF) && (actual_state == IR_ON) && (reception_started == FALSE))
    {
        reception_started = TRUE;         //The reception has been started.
        reception_completed = FALSE;      //Of course the reception has not been completed yet.
        length_index = 0;                 //Clearing the index of gap lenght data.
        timestamp_reception = millis();   //Taking the timestamp of the start of the reception. Milliseconds.
    }
    //Hunting for a falling edge during data reception.
    else if ((previous_state == IR_ON) && (actual_state == IR_OFF) && (reception_started == TRUE))
    {
        timestamp_gap = micros();     //Taking the timestamp at the start of a gap between pulses. Microseconds.
    }
    //Hunting for a rising edge during data reception.
    else if ((previous_state == IR_OFF) && (actual_state == IR_ON) && (reception_started == TRUE))
    {
        pulse_distance = micros() - timestamp_gap;   //Calculating the length of the gap between pulses.

        if (((MIN_H < pulse_distance) && (pulse_distance < MAX_H))   //Is the gap length in one of the
            || ((MIN_0 < pulse_distance) && (pulse_distance < MAX_0))   //dedicated ranges?
            || ((MIN_1 < pulse_distance) && (pulse_distance < MAX_1))
            )
        {
            gap_length[length_index] = pulse_distance;    //Storing the actual pulse distance.
            length_index++;     //Increasing the index of pulse distances.

            if (length_index == NUM_OF_GAPS)    //Do we have ALL the expected length values?
            {
                reception_completed = TRUE;     //Yes, the reception ha been completed.
            }
        }
        else    //The actual pulse distance is not in any of the given ranges.
        {
            reception_started = FALSE;    //Aborting the reception process.
            //Serial.println("\n\nInvalid length");   //Sending an error message.
        }
    }

    //If the timeout value of the reception is reached but it has not been completed yet...
    if ((millis() - timestamp_reception > RECEPTION_TIMEOUT)
        && (reception_started == TRUE) && (reception_completed == FALSE))
    {
        reception_started = FALSE;      //Aborting the reception process.
        //Serial.println("\nTimeout");    //Sending an error message.
    }

    if (reception_completed == TRUE)    //If the reception has been completed with success...
    {
        reception_started = FALSE;          //Clearing the process flags.
        reception_completed = FALSE;

        retur = decoding(gap_length, code_byte);     //Decoding the sequence of pulse distances.
        //display_code_bytes(code_byte);      //Displaying the code bytes.
        //name_of_the_button(code_byte[2]);   //Searching for the name of the button.
    }

    previous_state = actual_state;    //Shifting the states.

    /*if (retur != 0) {
        Serial.println(JRswitch, DEC);
    }*/

    return retur;
}


//LOOP
void loop() {
    timestamp = millis();
    
    //if (JRswitch == 2) {    //JoyStick Mode on    //this code is the right, but the JoyStick's button dosn't work well so swithk back with ok button
    //    GetJoyStickDirection();
    //}
    //else if (JRswitch == 1) {   //Remote controller mode on
    //    GetRemoteControllerDirection();
    //}

    //Read JoyStick and Remote controller in one time, because the JoyStick's button dosn't work well, so switch between JoyStick and Remote controller with OK button in every time
    
    GetRemoteControllerDirection();
    //GetJoyStickDirection();
    
    if (timestamp - bef_timestamp_remote > 50) {    //the JoyStick's button dosn't work well so switch back with ok button (delay due to remote control)
        GetJoyStickDirection();
        bef_timestamp_remote = timestamp;
    }

    if (timestamp - bef_timestamp > 400) {
        drawShape();
        Step();
        bef_timestamp = timestamp;
    }


    //delay(500);
}

void Step() {
    bx = x;
    by = y;
    if (joyDir == 3) {
        if (x < 31) {
            x++;
        }
        else {
            x = 0;
        }
    }
    else if (joyDir == 4) {
        if (x > 0) {
            x--;
        }
        else {
            x = 31;
        }
    }
    else if (joyDir == 1) {
        if (y < 7) {
            y++;
        }
        else {
            y = 0;
        }
    }
    else if (joyDir == 2) {
        if (y > 0) {
            y--;
        }
        else {
            y = 7;
        }
    }

    if (snak[y][x] == 255) {
        count++;
        snak[y][x] = 1;
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 32; j++) {
                if (snak[i][j] > 1 && snak[i][j] < 255) {
                    snak[i][j]++;
                }
            }
        }
        snak[by][bx] = 2;
        GetFood();
    }
    else if (snak[y][x] > 0) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 32; j++) {
                snak[i][j] = 0;
            }
        }
        x = 2;
        y = 0;
        bx = 16;
        by = 16;
        snak[y][x] = 1;
        count = 1;
        GetFood();
    }
    else {
        snak[y][x] = 1;

        if (count == 1) {
            snak[by][bx] = 0;
        }
        else {
            snak[by][bx] = 2;
        }

        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 32; j++) {
                if (snak[i][j] > 1 && snak[i][j] < count) {
                    snak[i][j]++;
                }
                else if (snak[i][j] == count && snak[i][j] != 1) {
                    snak[i][j] = 0;
                }
            }
        }

    }
    //snak[y][x]=1;


}

void GetFood() {
    byte r = 0;
    while (r == 0) {
        int rX = random(0, 31);
        int rY = random(0, 7);
        if (snak[rY][rX] == 0) {
            snak[rY][rX] = 255;
            r = 1;
        }
    }
}

void GetRemoteControllerDirection() {
    byte RemoteControllerValue = GetRemoteSignal();

    if (RemoteControllerValue == 0x84) {   //ok
        //JRswitch = 2;  //Switch to JoyStick               /////this is the rigth
        if (JRswitch == 1) {    //the JoyStick's button dosn't work well so switch back with ok button
            JRswitch = 2;
        }
        else {
            JRswitch = 1;
        }
    }

    if (JRswitch == 1) {    //the JoyStick's button dosn't work well so switch back with ok button

        if (RemoteControllerValue == 0x01 && joyDir != 1) {    //up
            joyDir = 2;
        }
        else if (RemoteControllerValue == 0x81 && joyDir != 2) {   //down
            joyDir = 1;
        }
        else if (RemoteControllerValue == 0xB2 && joyDir != 3) {   //rigth
            joyDir = 4;
        }
        else if (RemoteControllerValue == 0x8A && joyDir != 4) {   //left
            joyDir = 3;
        }
    }

}
void GetJoyStickDirection() {
    if (JRswitch == 2) {    //the JoyStick's button dosn't work well so switch back with ok button
        joyX = analogRead(joyXa);
        joyY = analogRead(joyYa);
        joySW = digitalRead(joySWa);
    }

    //if(joySW==0){ This is the right fork, but the JoyStick's button dosn't work well so switch back with ok button
    //    JRswitch = 1;  //Switch to remote controller
    //}

    if (JRswitch == 2) {    //the JoyStick's button dosn't work well so switch back with ok button

        if (joyX > 900 && joyY % 500 < 250 && joyDir != 2) {    //down
            joyDir = 1;
        }
        else if (joyX < 200 && joyY % 500 < 250 && joyDir != 1) {   //up
            joyDir = 2;
        }
        else if (joyY > 900 && joyX % 500 < 250 && joyDir != 4) {   //left
            joyDir = 3;
        }if (joyY < 200 && joyX % 500 < 250 && joyDir != 3) {   //right
            joyDir = 4;
        }
    }

}

void GetBform() {
    int i = 0;
    int j = 0;
    int k = 0;
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 4; j++) {
            byte aa = 0;
            int aaa = 0;
            for (k = 0; k < 8; k++) {
                if (snak[i][j * 8 + k] > 0) {
                    aa += pow(2, k);
                    if (k > 1 && aaa == 0) { //a random pow 2 fölött pl 2^3=7,999 ezért 1 et hozzá kell adni
                        aa += 1;
                    }
                }
            }

            /*if(aa!=0){
              Serial.println(aa,DEC);
            }*/

            sn[i][3 - j] = aa;
        }
    }

}

void drawShape() {
    GetBform();
    int i = 0;
    int j = 0;

    for (i = 0; i < 8; i++) {
        for (j = 0; j < 4; j++) {
            mx.setRow(3 - j, i, sn[i][j]);
        }
    }

}