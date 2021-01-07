#include <MIDI.h>
#include <Tsunami.h>
#include <Bounce.h>

Tsunami tsunami; 

#define BUTTON1 2
#define BUTTON2 3
#define BUTTON3 4
#define OUT_PORT 3

Bounce bounceButtonUP = Bounce(BUTTON1, 5); 
Bounce bounceButtonDWN = Bounce(BUTTON2, 5); 
Bounce bounceButtonSEL = Bounce(BUTTON3, 5); 

int  gSeqState = 0;             // Main program sequencer state
int  gRateOffset = 0;           // Tsunami sample-rate offset
int  gNumTracks;                // Number of tracks on SD card

char gTsunamiVersion[VERSION_STRING_LEN];    // Tsunami version string

// Create the Serial MIDI ports
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI1);

short translationMap[16][128];

int currentTrack = 1;

long upPressedTime = 0; // millis() result when up button is pressed
long dwnPressedTime = 0; // millis() result when down button is pressed
long selPressedTime = 0; // millis() result when select button is pressed
long upRelTime = 0; // millis() result when up button is release
long dwnRelTime = 0; // millis() result when down button is release
long selRelTime = 0; // millis() result when select button is release

boolean upPressed = false,
dwnPressed = false,
selPressed = false,
upProcessed = false,
dwnProcessed = false,
selProcessed = false,
upDwnComboProcessed = false,
menuEntered = false,
oneOutPort = false;

int menuState = 1,
oneOutPortNum = 0,
holdTimeToJump = 500,
jumpSize = 25;

int midiChanOutPort[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

void setup(){
    pinMode(BUTTON1,INPUT_PULLUP);
    pinMode(BUTTON2,INPUT_PULLUP);
    pinMode(BUTTON3,INPUT_PULLUP);

    MIDI1.begin(MIDI_CHANNEL_OMNI);
    Serial.begin(115200);

    // init the translationMap array to all 1's so that it isnt random data.
    for(byte i = 0; i < 16; i++){
        for(byte p=0;p<128;p++){
            translationMap[i][p]=1;
        }
    }

    // We should wait for the Tsunami to finish reset before trying to send
    // commands.
    delay(1000);

    // Tsunami startup at 57600
    tsunami.start();
    delay(10);
    
    // Send a stop-all command and reset the sample-rate offset, in case we have
    // reset while the Tsunami was already playing.
    tsunami.stopAllTracks();
    tsunami.samplerateOffset(0, 0);
    tsunami.masterGain(0, 0);              // Reset the master gain to 0dB
    
    // Enable track reporting from the Tsunami
    tsunami.setReporting(true);
    
    // Allow time for the Tsunami to respond with the version string and
    //  number of tracks.
    delay(100);
}

void loop(){
    tsunami.update();
    if(bounceButtonUP.update()){
        if(bounceButtonUP.fallingEdge()){
            upPressed=true;
            upPressedTime=millis();
            upDwnComboProcessed = false;
        }
        if(bounceButtonUP.risingEdge()){
            upPressed=false;
            upRelTime=millis();
            upProcessed=false;
        }
    }
    if(bounceButtonDWN.update()){
        if(bounceButtonDWN.fallingEdge()){
            dwnPressed=true;
            dwnPressedTime=millis();
            upDwnComboProcessed = false;
        }
        if(bounceButtonDWN.risingEdge()){
            dwnPressed=false;
            dwnRelTime=millis();
            dwnProcessed=false;
        }
    }
    if(bounceButtonSEL.update()){
        if(bounceButtonSEL.fallingEdge()){
            selPressed = true;
            selPressedTime=millis();
        }
        if(bounceButtonSEL.risingEdge()){
            selPressed=false;
            selRelTime=millis();
            selProcessed=false;
        }
    }

    if(upPressed && !dwnPressed && !selPressed && !upProcessed){ // just up pressed
        if(menuEntered){
            menuUp();
        }else{
            if(currentTrack<4000){
                currentTrack++;
            }
            playTrack(currentTrack,100,OUT_PORT,false);
        }
        upProcessed = true;
    }

    if(upRelTime-upPressedTime>holdTimeToJump && !dwnPressed && !selPressed && !upDwnComboProcessed && !menuEntered){ // up button held for longer than timeout
        currentTrack+=24;
        playTrack(currentTrack,100,OUT_PORT,false);
        upRelTime=0;
        upPressedTime=0;
    }

    if(!upPressed && dwnPressed && !selPressed && !dwnProcessed){ // just down button pressed
        if(menuEntered){
            menuDown();
        }else{
            if(currentTrack>1){
                currentTrack--;
            }
            playTrack(currentTrack,100,OUT_PORT,false);
        }
        dwnProcessed=true;
    }

    if(dwnRelTime-dwnPressedTime>holdTimeToJump && !upPressed && !selPressed && !upDwnComboProcessed && !menuEntered){ // down button held for longer than timeout
        currentTrack-=currentTrack>24?24:0;
        playTrack(currentTrack,100,OUT_PORT,false);
        dwnRelTime=0;
        dwnPressedTime=0;
    }

    if(upPressed && dwnPressed && !selPressed ){ // upo abnd down pressed together
        currentTrack=1;
        upDwnComboProcessed = true;
        if(menuEntered){
            menuState=1;
        }
    }

    if(selPressed && !upPressed && !dwnPressed && !selProcessed){ // just saelect pressed
        if(menuEntered){
            menuSelect();
        }else{
            playTrack(currentTrack,100,OUT_PORT,false);
        }
        selProcessed=true;
    }

    if(selRelTime-selPressedTime>5000 && !upPressed && !dwnPressed && !selProcessed){ // down button held for longer than timeout
        currentTrack-=currentTrack>24?24:0;
        playTrack(menuEntered?4061:4062,100,0,false); // play sound for entering and exiting hte menu
        selRelTime=0;
        selPressedTime=0;
        menuEntered = !menuEntered;
        menuState = 1;
    }

    if (MIDI1.read()) {                    // Is there a MIDI message incoming ?
        byte note = 0;
        byte velocity = 0;
        byte channel = 0;
        byte type = MIDI1.getType();
        switch (type) {
            case midi::NoteOn:
                note = MIDI1.getData1();
                velocity = MIDI1.getData2();
                channel = MIDI1.getChannel();
                if (velocity > 0) {
                    if(selPressed){
                        translationMap[channel-1][note]=currentTrack;
                    }
                    playTrack(translationMap[channel-1][note],velocity,midiChanOutPort[channel-1],false);
                    break;
                }
            case midi::NoteOff:
                note = MIDI1.getData1();
                velocity = MIDI1.getData2();
                channel = MIDI1.getChannel();
                // Serial.println(String("Note Off: ch=") + channel + ", note=" + note + ", velocity=" + velocity);
                break;
        }
    }
}

void playTrack(int trackNum, int trackVelocity, int outPort, boolean lockEn){
    playTrack(trackNum, trackVelocity, outPort, lockEn, false);
}
void playTrack(int trackNum, int trackVelocity, int outPort, boolean lockEn, boolean solo){
    //  calculate gain value using midi velocity. Gain is in dB which is logarithmic and velocity is linear, so
    //      by squaring the difference of velocity and its max value, and then mapping that value to a reversed
    //      range with a max of (max velocity)^2 or 16129, we get a gain value that feels very close to the 
    //      expected volume when played on the hardware. 
    int gain = map(sq(127-trackVelocity),16129,0,-70 /* -70 is the lowest gain recognized and is effectively muted */,4 /* this value can go as high as 10 */);

    // set gain value for the track about to be played
    tsunami.trackGain(trackNum,gain);

    // play the track. 
    if(solo){
        tsunami.trackPlaySolo(trackNum, outPort, lockEn);
    }else{
        tsunami.trackPlayPoly(trackNum, outPort, lockEn);
    }
}

void playMenuStateAudio(){
    tsunami.stopAllTracks();
    switch(menuState){
        case 1: // main menu
            playTrack(4062,100,0,true,true);
            break;
        case 2: // output mode
            playTrack(4063,100,0,true,true);
            break;
        case 3: // all sample play through same...
            playTrack(4064,100,0,true,true);
            break;
        case 4: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 5: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 6: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 7: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 8: // each midi gets assigned outport
            playTrack(4065,100,0,true,true);
            break;
        case 9: // midi channel 1 outport
            playTrack(4066,100,0,true,true);
            break;
        case 10: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 11: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 12: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 13: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 14: // midi ch 2
            playTrack(4067,100,0,true,true);
            break;
        case 15: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 16: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 17: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 18: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 19: // midi ch 3
            playTrack(4068,100,0,true,true);
            break;
        case 20: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 21: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 22: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 23: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 24: // midi ch 4
            playTrack(4069,100,0,true,true);
            break;
        case 25: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 26: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 27: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 28: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 29: // midi ch 5
            playTrack(4070,100,0,true,true);
            break;
        case 30: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 31: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 32: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 33: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 34: // midi ch 6
            playTrack(4071,100,0,true,true);
            break;
        case 35: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 36: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 37: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 38: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 39: // midi ch 7
            playTrack(4072,100,0,true,true);
            break;
        case 40: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 41: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 42: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 43: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 44: // midi ch 8
            playTrack(4073,100,0,true,true);
            break;
        case 45: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 46: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 47: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 48: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 49: // midi ch 9
            playTrack(4074,100,0,true,true);
            break;
        case 50: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 51: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 52: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 53: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 54: // midi ch 10
            playTrack(4075,100,0,true,true);
            break;
        case 55: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 56: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 57: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 58: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 59: // midi ch 11
            playTrack(4076,100,0,true,true);
            break;
        case 60: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 61: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 62: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 63: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 64: // midi ch 12
            playTrack(4077,100,0,true,true);
            break;
        case 65: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 66: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 67: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 68: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 69: // midi ch 13
            playTrack(4078,100,0,true,true);
            break;
        case 70: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 71: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 72: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 73: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 74: // midi ch 14
            playTrack(4079,100,0,true,true);
            break;
        case 75: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 76: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 77: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 78: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 79: // midi ch 15
            playTrack(4080,100,0,true,true);
            break;
        case 80: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 81: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 82: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 83: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 84: // midi ch 16
            playTrack(4081,100,0,true,true);
            break;
        case 85: // ports 1 & 2
            playTrack(4083,100,0,true,true);
            break;
        case 86: // ports 3 & 4
            playTrack(4084,100,0,true,true);
            break;
        case 87: // ports 5 & 6
            playTrack(4085,100,0,true,true);
            break;
        case 88: // ports 7 & 8
            playTrack(4086,100,0,true,true);
            break;
        case 89: // hold time to jump
            playTrack(4087,100,0,true,true);
            break;
        case 90: // 250ms
            playTrack(4088,100,0,true,true);
            break;
        case 91: // 500ms
            playTrack(4089,100,0,true,true);
            break;
        case 92: // 750ms
            playTrack(4090,100,0,true,true);
            break;
        case 93: // 1 sec
            playTrack(4091,100,0,true,true);
            break;
        case 94: // jump size
            playTrack(4092,100,0,true,true);
            break;
        case 95: // jump 25
            playTrack(4093,100,0,true,true);
            break;
        case 96: // juymp 50
            playTrack(4094,100,0,true,true);
            break;
        case 97: // jump 100
            playTrack(4095,100,0,true,true);
            break;
        case 98: // jump 250
            playTrack(4096,100,0,true,true);
            break;
        case 99: // menu exit / go back
            playTrack(4061,100,0,true,true);
            break;
    }
}

void menuUp(){
    switch(menuState){
        case 1: // main menu
            break;
        case 2: // output mode
            break;
        case 3: // all sample play through same...
            break;
        case 4: // ports 1 & 2
            break;
        case 5: // ports 3 & 4
            menuState=4;
            break;
        case 6: // ports 5 & 6
            menuState=5;
            break;
        case 7: // ports 7 & 8
            menuState=6;
            break;
        case 8: // each midi gets assigned outport
            menuState=3;
            break;
        case 9: // midi channel 1 outport
            break;
        case 10: // ports 1 & 2
            break;
        case 11: // ports 3 & 4
            menuState--;
            break;
        case 12: // ports 5 & 6
            menuState--;
            break;
        case 13: // ports 7 & 8
            menuState--;
            break;
        case 14: // midi ch 2
        menuState-=5;
            break;
        case 15: // ports 1 & 2
            break;
        case 16: // ports 3 & 4
            menuState--;
            break;
        case 17: // ports 5 & 6
            menuState--;
            break;
        case 18: // ports 7 & 8
            menuState--;
            break;
        case 19: // midi ch 3
        menuState-=5;
            break;
        case 20: // ports 1 & 2
            break;
        case 21: // ports 3 & 4
            menuState--;
            break;
        case 22: // ports 5 & 6
            menuState--;
            break;
        case 23: // ports 7 & 8
            menuState--;
            break;
        case 24: // midi ch 4
        menuState-=5;
            break;
        case 25: // ports 1 & 2
            break;
        case 26: // ports 3 & 4
            menuState--;
            break;
        case 27: // ports 5 & 6
            menuState--;
            break;
        case 28: // ports 7 & 8
            menuState--;
            break;
        case 29: // midi ch 5
        menuState-=5;
            break;
        case 30: // ports 1 & 2
            break;
        case 31: // ports 3 & 4
            menuState--;
            break;
        case 32: // ports 5 & 6
            menuState--;
            break;
        case 33: // ports 7 & 8
            menuState--;
            break;
        case 34: // midi ch 6
        menuState-=5;
            break;
        case 35: // ports 1 & 2
            break;
        case 36: // ports 3 & 4
            menuState--;
            break;
        case 37: // ports 5 & 6
            menuState--;
            break;
        case 38: // ports 7 & 8
            menuState--;
            break;
        case 39: // midi ch 7
        menuState-=5;
            break;
        case 40: // ports 1 & 2
            break;
        case 41: // ports 3 & 4
            menuState--;
            break;
        case 42: // ports 5 & 6
            menuState--;
            break;
        case 43: // ports 7 & 8
            menuState--;
            break;
        case 44: // midi ch 8
        menuState-=5;
            break;
        case 45: // ports 1 & 2
            break;
        case 46: // ports 3 & 4
            menuState--;
            break;
        case 47: // ports 5 & 6
            menuState--;
            break;
        case 48: // ports 7 & 8
            menuState--;
            break;
        case 49: // midi ch 9
        menuState-=5;
            break;
        case 50: // ports 1 & 2
            break;
        case 51: // ports 3 & 4
            menuState--;
            break;
        case 52: // ports 5 & 6
            menuState--;
            break;
        case 53: // ports 7 & 8
            menuState--;
            break;
        case 54: // midi ch 10
        menuState-=5;
            break;
        case 55: // ports 1 & 2
            break;
        case 56: // ports 3 & 4
            menuState--;
            break;
        case 57: // ports 5 & 6
            menuState--;
            break;
        case 58: // ports 7 & 8
            menuState--;
            break;
        case 59: // midi ch 11
        menuState-=5;
            break;
        case 60: // ports 1 & 2
            break;
        case 61: // ports 3 & 4
            menuState--;
            break;
        case 62: // ports 5 & 6
            menuState--;
            break;
        case 63: // ports 7 & 8
            menuState--;
            break;
        case 64: // midi ch 12
        menuState-=5;
            break;
        case 65: // ports 1 & 2
            break;
        case 66: // ports 3 & 4
            menuState--;
            break;
        case 67: // ports 5 & 6
            menuState--;
            break;
        case 68: // ports 7 & 8
            menuState--;
            break;
        case 69: // midi ch 13
        menuState-=5;
            break;
        case 70: // ports 1 & 2
            break;
        case 71: // ports 3 & 4
            menuState--;
            break;
        case 72: // ports 5 & 6
            menuState--;
            break;
        case 73: // ports 7 & 8
            menuState--;
            break;
        case 74: // midi ch 14
        menuState-=5;
            break;
        case 75: // ports 1 & 2
            break;
        case 76: // ports 3 & 4
            menuState--;
            break;
        case 77: // ports 5 & 6
            menuState--;
            break;
        case 78: // ports 7 & 8
            menuState--;
            break;
        case 79: // midi ch 15
        menuState-=5;
            break;
        case 80: // ports 1 & 2
            break;
        case 81: // ports 3 & 4
            menuState--;
            break;
        case 82: // ports 5 & 6
            menuState--;
            break;
        case 83: // ports 7 & 8
            menuState--;
            break;
        case 84: // midi ch 16
        menuState-=5;
            break;
        case 85: // ports 1 & 2
            break;
        case 86: // ports 3 & 4
            menuState--;
            break;
        case 87: // ports 5 & 6
            menuState--;
            break;
        case 88: // ports 7 & 8
            menuState--;
            break;
        case 89: // hold time to jump
        menuState=2;
            break;
        case 90: // 250ms
            break;
        case 91: // 500ms
            menuState--;
            break;
        case 92: // 750ms
            menuState--;
            break;
        case 93: // 1 sec
            menuState--;
            break;
        case 94: // jump size
        menuState=89;
            break;
        case 95: // jump 25
            break;
        case 96: // juymp 50
            menuState--;
            break;
        case 97: // jump 100
            menuState--;
            break;
        case 98: // jump 250
            menuState--;
            break;       
    }
    playMenuStateAudio();
    return;
}
void menuDown(){
    switch(menuState){
        case 1: // main menu
            menuState++;
            break;
        case 2: // output mode
            menuState=89;
            break;
        case 3: // all sample play through same...
            menuState=8;
            break;
        case 4: // ports 1 & 2
            menuState++;
            break;
        case 5: // ports 3 & 4
            menuState++;
            break;
        case 6: // ports 5 & 6
            menuState++;
            break;
        case 7: // ports 7 & 8
            break;
        case 8: // each midi gets assigned outport
            break;
        case 9: // midi channel 1 outport
            menuState+=5;
            break;
        case 10: // ports 1 & 2
            menuState++;
            break;
        case 11: // ports 3 & 4
            menuState++;
            break;
        case 12: // ports 5 & 6
            menuState++;
            break;
        case 13: // ports 7 & 8
            break;
        case 14: // midi ch 2
            menuState+=5;
            break;
        case 15: // ports 1 & 2
            menuState++;
            break;
        case 16: // ports 3 & 4
            menuState++;
            break;
        case 17: // ports 5 & 6
            menuState++;
            break;
        case 18: // ports 7 & 8
            break;
        case 19: // midi ch 3
            menuState+=5;
            break;
        case 20: // ports 1 & 2
            menuState++;
            break;
        case 21: // ports 3 & 4
            menuState++;
            break;
        case 22: // ports 5 & 6
            menuState++;
            break;
        case 23: // ports 7 & 8
            break;
        case 24: // midi ch 4
            menuState+=5;
            break;
        case 25: // ports 1 & 2
            menuState++;
            break;
        case 26: // ports 3 & 4
            menuState++;
            break;
        case 27: // ports 5 & 6
            menuState++;
            break;
        case 28: // ports 7 & 8
            break;
        case 29: // midi ch 5
            menuState+=5;
            break;
        case 30: // ports 1 & 2
            menuState++;
            break;
        case 31: // ports 3 & 4
            menuState++;
            break;
        case 32: // ports 5 & 6
            menuState++;
            break;
        case 33: // ports 7 & 8
            break;
        case 34: // midi ch 6
            menuState+=5;
            break;
        case 35: // ports 1 & 2
            menuState++;
            break;
        case 36: // ports 3 & 4
            menuState++;
            break;
        case 37: // ports 5 & 6
            menuState++;
            break;
        case 38: // ports 7 & 8
            break;
        case 39: // midi ch 7
            menuState+=5;
            break;
        case 40: // ports 1 & 2
            menuState++;
            break;
        case 41: // ports 3 & 4
            menuState++;
            break;
        case 42: // ports 5 & 6
            menuState++;
            break;
        case 43: // ports 7 & 8
            break;
        case 44: // midi ch 8
            menuState+=5;
            break;
        case 45: // ports 1 & 2
            menuState++;
            break;
        case 46: // ports 3 & 4
            menuState++;
            break;
        case 47: // ports 5 & 6
            menuState++;
            break;
        case 48: // ports 7 & 8
            break;
        case 49: // midi ch 9
            menuState+=5;
            break;
        case 50: // ports 1 & 2
            menuState++;
            break;
        case 51: // ports 3 & 4
            menuState++;
            break;
        case 52: // ports 5 & 6
            menuState++;
            break;
        case 53: // ports 7 & 8
            break;
        case 54: // midi ch 10
            menuState+=5;
            break;
        case 55: // ports 1 & 2
            menuState++;
            break;
        case 56: // ports 3 & 4
            menuState++;
            break;
        case 57: // ports 5 & 6
            menuState++;
            break;
        case 58: // ports 7 & 8
            break;
        case 59: // midi ch 11
            menuState+=5;
            break;
        case 60: // ports 1 & 2
            menuState++;
            break;
        case 61: // ports 3 & 4
            menuState++;
            break;
        case 62: // ports 5 & 6
            menuState++;
            break;
        case 63: // ports 7 & 8
            break;
        case 64: // midi ch 12
            menuState+=5;
            break;
        case 65: // ports 1 & 2
            menuState++;
            break;
        case 66: // ports 3 & 4
            menuState++;
            break;
        case 67: // ports 5 & 6
            menuState++;
            break;
        case 68: // ports 7 & 8
            break;
        case 69: // midi ch 13
            menuState+=5;
            break;
        case 70: // ports 1 & 2
            menuState++;
            break;
        case 71: // ports 3 & 4
            menuState++;
            break;
        case 72: // ports 5 & 6
            menuState++;
            break;
        case 73: // ports 7 & 8
            break;
        case 74: // midi ch 14
            menuState+=5;
            break;
        case 75: // ports 1 & 2
            menuState++;
            break;
        case 76: // ports 3 & 4
            menuState++;
            break;
        case 77: // ports 5 & 6
            menuState++;
            break;
        case 78: // ports 7 & 8
            break;
        case 79: // midi ch 15
            menuState+=5;
            break;
        case 80: // ports 1 & 2
            menuState++;
            break;
        case 81: // ports 3 & 4
            menuState++;
            break;
        case 82: // ports 5 & 6
            menuState++;
            break;
        case 83: // ports 7 & 8
            break;
        case 84: // midi ch 16
            break;
        case 85: // ports 1 & 2
            menuState++;
            break;
        case 86: // ports 3 & 4
            menuState++;
            break;
        case 87: // ports 5 & 6
            menuState++;
            break;
        case 88: // ports 7 & 8
            break;
        case 89: // hold time to jump
            menuState=94;
            break;
        case 90: // 250ms
            menuState++;
            break;
        case 91: // 500ms
            menuState++;
            break;
        case 92: // 750ms
            menuState++;
            break;
        case 93: // 1 sec
            break;
        case 94: // jump size
            break;
        case 95: // jump 25
            menuState++;
            break;
        case 96: // juymp 50
            menuState++;
            break;
        case 97: // jump 100
            menuState++;
            break;
        case 98: // jump 250
            break;
    }
    playMenuStateAudio();
    return;
}
void menuSelect(){switch(menuState){
        case 1: // main menu
            break;
        case 2: // output mode
            menuState = 3;
            break;
        case 3: // all sample play through same...
            oneOutPort = true;
            menuState = 4;
            break;
        case 4: // ports 1 & 2
            oneOutPortNum = 0;
            menuState=1;
            break;
        case 5: // ports 3 & 4
            oneOutPortNum = 1;
            menuState=1;
            break;
        case 6: // ports 5 & 6
            oneOutPortNum = 2;
            menuState=1;
            break;
        case 7: // ports 7 & 8
            oneOutPortNum = 3;
            menuState=1;
            break;
        case 8: // each midi gets assigned outport
            oneOutPort = false;
            menuState=9;
            break;
        case 9: // midi channel 1 outport
            menuState++;
            break;
        case 10: // ports 1 & 2
            midiChanOutPort[0] = 0;
            menuState = 9;
            break;
        case 11: // ports 3 & 4
            midiChanOutPort[0] = 1;
            menuState = 9;
            break;
        case 12: // ports 5 & 6
            midiChanOutPort[0] = 2;
            menuState = 9;
            break;
        case 13: // ports 7 & 8
            midiChanOutPort[0] = 3;
            menuState = 9;
            break;
        case 14: // midi ch 2
            menuState++;
            break;
        case 15: // ports 1 & 2
            midiChanOutPort[1] = 0;
            menuState = 14;
            break;
        case 16: // ports 3 & 4
            midiChanOutPort[1] = 1;
            menuState = 14;
            break;
        case 17: // ports 5 & 6
            midiChanOutPort[1] = 2;
            menuState = 14;
            break;
        case 18: // ports 7 & 8
            midiChanOutPort[1] = 3;
            menuState = 14;
            break;
        case 19: // midi ch 3
            menuState++;
            break;
        case 20: // ports 1 & 2
            midiChanOutPort[2] = 0;
            menuState = 19;
            break;
        case 21: // ports 3 & 4
            midiChanOutPort[2] = 1;
            menuState = 19;
            break;
        case 22: // ports 5 & 6
            midiChanOutPort[2] = 2;
            menuState = 19;
            break;
        case 23: // ports 7 & 8
            midiChanOutPort[2] = 3;
            menuState = 19;
            break;
        case 24: // midi ch 4
            menuState++;
            break;
        case 25: // ports 1 & 2
            midiChanOutPort[3] = 0;
            menuState = 24;
            break;
        case 26: // ports 3 & 4
            midiChanOutPort[3] = 1;
            menuState = 24;
            break;
        case 27: // ports 5 & 6
            midiChanOutPort[3] = 2;
            menuState = 24;
            break;
        case 28: // ports 7 & 8
            midiChanOutPort[3] = 3;
            menuState = 24;
            break;
        case 29: // midi ch 5
            menuState++;
            break;
        case 30: // ports 1 & 2
            midiChanOutPort[4] = 0;
            menuState = 29;
            break;
        case 31: // ports 3 & 4
            midiChanOutPort[4] = 1;
            menuState = 29;
            break;
        case 32: // ports 5 & 6
            midiChanOutPort[4] = 2;
            menuState = 29;
            break;
        case 33: // ports 7 & 8
            midiChanOutPort[4] = 3;
            menuState = 29;
            break;
        case 34: // midi ch 6
            menuState++;
            break;
        case 35: // ports 1 & 2
            midiChanOutPort[5] = 0;
            menuState = 34;
            break;
        case 36: // ports 3 & 4
            midiChanOutPort[5] = 1;
            menuState = 34;
            break;
        case 37: // ports 5 & 6
            midiChanOutPort[5] = 2;
            menuState = 34;
            break;
        case 38: // ports 7 & 8
            midiChanOutPort[5] = 3;
            menuState = 34;
            break;
        case 39: // midi ch 7
            menuState++;
            break;
        case 40: // ports 1 & 2
            midiChanOutPort[6] = 0;
            menuState = 39;
            break;
        case 41: // ports 3 & 4
            midiChanOutPort[6] = 1;
            menuState = 39;
            break;
        case 42: // ports 5 & 6
            midiChanOutPort[6] = 2;
            menuState = 39;
            break;
        case 43: // ports 7 & 8
            midiChanOutPort[6] = 3;
            menuState = 39;
            break;
        case 44: // midi ch 8
            menuState++;
            break;
        case 45: // ports 1 & 2
            midiChanOutPort[7] = 0;
            menuState = 44;
            break;
        case 46: // ports 3 & 4
            midiChanOutPort[7] = 1;
            menuState = 44;
            break;
        case 47: // ports 5 & 6
            midiChanOutPort[7] = 2;
            menuState = 44;
            break;
        case 48: // ports 7 & 8
            midiChanOutPort[7] = 3;
            menuState = 44;
            break;
        case 49: // midi ch 9
            menuState++;
            break;
        case 50: // ports 1 & 2
            midiChanOutPort[8] = 0;
            menuState = 49;
            break;
        case 51: // ports 3 & 4
            midiChanOutPort[8] = 1;
            menuState = 49;
            break;
        case 52: // ports 5 & 6
            midiChanOutPort[8] = 2;
            menuState = 49;
            break;
        case 53: // ports 7 & 8
            midiChanOutPort[8] = 3;
            menuState = 49;
            break;
        case 54: // midi ch 10
            menuState++;
            break;
        case 55: // ports 1 & 2
            midiChanOutPort[9] = 0;
            menuState = 54;
            break;
        case 56: // ports 3 & 4
            midiChanOutPort[9] = 1;
            menuState = 54;
            break;
        case 57: // ports 5 & 6
            midiChanOutPort[9] = 2;
            menuState = 54;
            break;
        case 58: // ports 7 & 8
            midiChanOutPort[9] = 3;
            menuState = 54;
            break;
        case 59: // midi ch 11
            menuState++;
            break;
        case 60: // ports 1 & 2
            midiChanOutPort[10] = 0;
            menuState = 59;
            break;
        case 61: // ports 3 & 4
            midiChanOutPort[10] = 1;
            menuState = 59;
            break;
        case 62: // ports 5 & 6
            midiChanOutPort[10] = 2;
            menuState = 59;
            break;
        case 63: // ports 7 & 8
            midiChanOutPort[10] = 3;
            menuState = 59;
            break;
        case 64: // midi ch 12
            menuState++;
            break;
        case 65: // ports 1 & 2
            midiChanOutPort[11] = 0;
            menuState = 64;
            break;
        case 66: // ports 3 & 4
            midiChanOutPort[11] = 1;
            menuState = 64;
            break;
        case 67: // ports 5 & 6
            midiChanOutPort[11] = 2;
            menuState = 64;
            break;
        case 68: // ports 7 & 8
            midiChanOutPort[11] = 3;
            menuState = 64;
            break;
        case 69: // midi ch 13
            menuState++;
            break;
        case 70: // ports 1 & 2
            midiChanOutPort[12] = 0;
            menuState = 69;
            break;
        case 71: // ports 3 & 4
            midiChanOutPort[12] = 1;
            menuState = 69;
            break;
        case 72: // ports 5 & 6
            midiChanOutPort[12] = 2;
            menuState = 69;
            break;
        case 73: // ports 7 & 8
            midiChanOutPort[12] = 3;
            menuState = 69;
            break;
        case 74: // midi ch 14
            menuState++;
            break;
        case 75: // ports 1 & 2
            midiChanOutPort[13] = 0;
            menuState = 74;
            break;
        case 76: // ports 3 & 4
            midiChanOutPort[13] = 1;
            menuState = 74;
            break;
        case 77: // ports 5 & 6
            midiChanOutPort[13] = 2;
            menuState = 74;
            break;
        case 78: // ports 7 & 8
            midiChanOutPort[13] = 3;
            menuState = 74;
            break;
        case 79: // midi ch 15
            menuState++;
            break;
        case 80: // ports 1 & 2
            midiChanOutPort[14] = 0;
            menuState = 79;
            break;
        case 81: // ports 3 & 4
            midiChanOutPort[14] = 1;
            menuState = 79;
            break;
        case 82: // ports 5 & 6
            midiChanOutPort[14] = 2;
            menuState = 79;
            break;
        case 83: // ports 7 & 8
            midiChanOutPort[14] = 3;
            menuState = 79;
            break;
        case 84: // midi ch 16
            menuState++;
            break;
        case 85: // ports 1 & 2
            midiChanOutPort[15] = 0;
            menuState = 84;
            break;
        case 86: // ports 3 & 4
            midiChanOutPort[15] = 1;
            menuState = 84;
            break;
        case 87: // ports 5 & 6
            midiChanOutPort[15] = 2;
            menuState = 84;
            break;
        case 88: // ports 7 & 8
            midiChanOutPort[15] = 3;
            menuState = 84;
            break;
        case 89: // hold time to jump
            menuState = 90;
            break;
        case 90: // 250ms
            menuState = 1;
            holdTimeToJump = 250;
            break;
        case 91: // 500ms
            menuState = 1;
            holdTimeToJump = 500;
            break;
        case 92: // 750ms
            menuState = 1;
            holdTimeToJump = 750;
            break;
        case 93: // 1 sec
            menuState = 1;
            holdTimeToJump = 1000;
            break;
        case 94: // jump size
            menuState = 95;
            break;
        case 95: // jump 25
            menuState = 1;
            jumpSize = 25;
            break;
        case 96: // juymp 50
            menuState = 1;
            jumpSize = 50;
            break;
        case 97: // jump 100
            menuState = 1;
            jumpSize = 100;
            break;
        case 98: // jump 250
            menuState = 1;
            jumpSize = 250;
            break;
            
    }
    playMenuStateAudio();
    return;
}


/*

Here, have an ASCII cat!!!

                     ;,_            ,
                  _uP~"b          d"u,
                 dP'   "b       ,d"  "o
                d"    , `b     d"'    "b
               l] [    " `l,  d"       lb
               Ol ?     "  "b`"=uoqo,_  "l
             ,dBb "b        "b,    `"~~TObup,_
           ,d" (db.`"         ""     "tbc,_ `~"Yuu,_
         .d" l`T'  '=                      ~     `""Yu,
       ,dO` gP,                           `u,   b,_  "b7
      d?' ,d" l,                           `"b,_ `~b  "1
    ,8i' dl   `l                 ,ggQOV",dbgq,._"  `l  lb
   .df' (O,    "             ,ggQY"~  , @@@@@d"bd~  `b "1
  .df'   `"           -=@QgpOY""     (b  @@@@P db    `Lp"b,
 .d(                  _               "ko "=d_,Q`  ,_  "  "b,
 Ql         .         `"qo,._          "tQo,_`""bo ;tb,    `"b,
(qQ         |L           ~"QQQgggc,_.,dObc,opooO  `"~~";.   __,7,
`qp         t\io,_           `~"TOOggQV""""        _,dg,_ =PIQHib.
 `qp        `Q["tQQQo,_                          ,pl{QOP"'   7AFR`
   `         `tb  '""tQQQg,_             p" "b   `       .;-.`Vl'
              "Yb      `"tQOOo,__    _,edb    ` .__   /`/'|  |b;=;.__
                            `"tQQQOOOOP""        `"\QV;qQObob"`-._`\_~~-._
                                 """"    ._        /   | |oP"\_   ~\ ~\_  ~\
                                         `~"\ic,qggddOOP"|  |  ~\   `\  ~-._
                                           ,qP`"""|"   | `\ `;   `\   `\
                                _        _,p"     |    |   `\`;    |    |
                                 "boo,._dP"       `\_  `\    `\|   `\   ;
                                  `"7tY~'            `\  `\    `|_   |
                                                           `~\  |
*/