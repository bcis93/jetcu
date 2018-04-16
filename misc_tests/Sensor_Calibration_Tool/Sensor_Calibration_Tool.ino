#include <Wire.h>
#define SLAVE_ADDRESS 0x01 
 volatile byte command=0x00;

void setup() {
Wire.begin(SLAVE_ADDRESS);
Wire.onReceive(receiveData);
Wire.onRequest(sendData);}

//RPM byte values to send
int RPMbyte1=117;//Small step
int RPMbyte2=195;//Large step

//Temperature byte values to send
int Tempbyte1=249;//Small step (0-250 for the P100)
int Tempbyte2=3;//Large step (0-3 for the P100)

//Engine Identifier byte values to send
int Identbyte1=0;
int Identbyte2=8;
int Identbyte3=1;
int Identbyte4=0;
int Identbyte5=255;
int Identbyte6=248;

void loop() {}

//Simple loop that receives read commands
void receiveData(volatile int byteCount){
 command=Wire.read();
 while (Wire.available()>0){
 Wire.read();}
}

//Replies to read commands with the programmed response
void sendData(){
    if (command==0x07){Wire.write(0xaa);}//Unknown function, this value seems to be constant
    if (command==0x0e){Wire.write(0x40);Wire.write(0x01);Wire.write(0xb1);}//Unknown function,possibly another sensor, possibly a clock.
    if (command==0x0b){Wire.write(Tempbyte1);Wire.write(Tempbyte2);Wire.write(245-Tempbyte1-Tempbyte2);}//EGT thermocouple
    if (command==0x08){Wire.write(RPMbyte1);Wire.write(RPMbyte2);Wire.write(248-RPMbyte1-RPMbyte2);}//RPM sensor
    if (command==0x15){Wire.write(Identbyte1);Wire.write(Identbyte2);Wire.write(Identbyte3);Wire.write(Identbyte4);Wire.write(Identbyte5);Wire.write(Identbyte6);}//Engine identifier
    if (command==0x1f){Wire.write(0x49);}//Unknown function
    
}
