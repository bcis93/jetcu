/*
 * JetCU Controller
 * Revision 1.0
 * 
 * This is the main controller. It controls the
 * starter motor and glow plug PWM signals, 
 * communicates with the engine and fuel pump
 * through I2C, and communicates with the PC
 * through serial commands.
 * 
 */




//#include <Wire.h>
#include <I2C.h>
#define ENGINE_ADDRESS 0x01
#define PUMP_ADDRESS 0x02


//timer setup for timer0, timer1, and timer2.
//For arduino uno or any board with ATMEL 328/168.. diecimila, duemilanove, lilypad, nano, mini...

//this code will enable two arduino timer interrupts.
//timer0 will not be changed
//timer1 will interrupt at 532Hz
//timer2 will interrupt at 40kHz

//constants and variables for safety checks
bool emergencyStopFlag = false;
#define COMMUNICATION_TIMEOUT 500 //in milliseconds
int communication_timeout_counter = 0;

//constants and variables for starter motor
#define STARTER_MOTOR_PIN 6
#define STARTER_MOTOR_COUNTER_MAX 19
int starterMotor_dutyCycle = 0; //maybe add volatile

//constants and variables for glow plug
#define GLOW_PLUG_PIN 5
#define GLOW_PLUG_COUNTER_MAX 19
int glowPlug_dutyCycle = 0;

//constants and variables for communication
#define FAST 1
String incoming_command = "";

//variable for fuel pump mode
//int fuelPumpMode = 0;
volatile int pumpvalue1=0;
volatile int pumpvalue2=0;
bool fuelPumpFlag = false;


void setup(){
  
  //set pins as outputs
  pinMode(STARTER_MOTOR_PIN, OUTPUT);
  pinMode(GLOW_PLUG_PIN, OUTPUT);

  init_interrupts();

  Serial.begin(115200);
  Serial.setTimeout(10); //sets the timeout of the serial read to 10ms

  //setup I2C communication
//  Wire.begin();
//  Wire.setClock(20000);
  I2c.begin();
  I2c.timeOut(100); //100 ms timeout
  I2c.setSpeed(FAST); //sets to high speed I2C
}

void init_interrupts(){
  cli();//stop interrupts

  // enable timer compare interrupt
  TIMSK0 |= (1 << OCIE0A);

  //set timer1 interrupt at 532Hz
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 532hz increments
  OCR1A = 30075;// = (16*10^6) / (532*1) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS10 for 1 prescaler
  TCCR1B |= (1 << CS10);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);

  //set timer2 interrupt at 40kHz
  TCCR2A = 0;// set entire TCCR2A register to 0
  TCCR2B = 0;// same for TCCR2B
  TCNT2  = 0;//initialize counter value to 0
  // set compare match register for 20khz increments
  OCR2A = 50;// = (16*10^6) / (40000*8) - 1 (must be <256)
  // turn on CTC mode
  TCCR2A |= (1 << WGM21);
  // Set CS21 bit for 8 prescaler
  TCCR2B |= (1 << CS21);   
  // enable timer compare interrupt
  TIMSK2 |= (1 << OCIE2A);


  sei();//allow interrupts
}

ISR(TIMER0_COMPA_vect){
  static int fuel_pump_counter = 0;
  fuel_pump_counter++;
  communication_timeout_counter++;
  if (fuel_pump_counter >= 250){
    fuelPumpFlag = true;
    fuel_pump_counter = 0;
  }
  else{
    fuelPumpFlag = false;
  }
  if (communication_timeout_counter > COMMUNICATION_TIMEOUT){
    emergencyStopFlag = true;
  }
  else{
    emergencyStopFlag = false;
  }
}

ISR(TIMER1_COMPA_vect){//timer1 interrupt 532Hz
  static int counter = 0;
  if (counter < glowPlug_dutyCycle){
    digitalWrite(GLOW_PLUG_PIN, HIGH );
    counter++;
  }
  else if (counter < GLOW_PLUG_COUNTER_MAX){
    digitalWrite(GLOW_PLUG_PIN, LOW);
    counter++;
  }
  else{
    counter = 0;
  }
}
  
ISR(TIMER2_COMPA_vect){//timer2 interrupt 40kHz for starter motor
    static int counter = 0;
    if (counter < starterMotor_dutyCycle){
      digitalWrite(STARTER_MOTOR_PIN, HIGH );
      counter++;
    }
    else if (counter < STARTER_MOTOR_COUNTER_MAX){
      digitalWrite(STARTER_MOTOR_PIN, LOW);
      counter++;
    }
    else{
      counter = 0;
    }
}

void loop() {
  // put your main code here, to run repeatedly:
//  if (emergencyStopFlag){
//    //stop!!
//  }
//  else{

    if (Serial.available() > 0){
    incoming_command += Serial.readString();
    String response = handle_command();
    Serial.println(response);
    }
  
    if (fuelPumpFlag){
      fuelPumpControl();
    }
    
//  }

}

String handle_command(){
  //Serial.println("handle_command");
  // handle commands here
  String response = "";
  if (incoming_command.length() > 1){
    incoming_command.trim(); //removes extra whitespace before and after command
    String commandType = incoming_command.substring(0,3);
    if (commandType == "SMD"){
      int requestedDutyCycle = incoming_command.substring(3,5).toInt();
      response += setStarterMotorDutyCycle(requestedDutyCycle) + "\n";
      incoming_command = incoming_command.substring(5);
      communication_timeout_counter = 0;
    }
    else if (commandType == "GPD"){
      int requestedDutyCycle = incoming_command.substring(3,5).toInt();
      response += setGlowPlugDutyCycle(requestedDutyCycle) + "\n";
      incoming_command = incoming_command.substring(5);
      communication_timeout_counter = 0;
    }
    else if (commandType == "END"){
      response += "end\n";
      communication_timeout_counter = 0;
      emergencyStop();
      incoming_command = "";
    }
    else if (commandType == "RQT"){
      int temp = requestTemperature();
      char tmp[5];
      sprintf(tmp, "%04d", temp);
      response += ("rqt" + String(tmp)) + "\n";
      incoming_command = incoming_command.substring(3);
      communication_timeout_counter = 0;
    }
    else if (commandType == "RQR"){
      long rpm = requestRPM();
      char tmp[7];
      sprintf(tmp, "%06ld",rpm);
      response += ("rqr" + String(tmp)) + "\n";
      incoming_command = incoming_command.substring(3);
      communication_timeout_counter = 0;
    }
    else if (commandType == "FVP"){
      int requestedPercent = incoming_command.substring(3,6).toInt();
      response += fuelValve(requestedPercent) + "\n";
      incoming_command = incoming_command.substring(6);
      communication_timeout_counter = 0;
    }
    else if (commandType == "BVP"){
      int requestedPercent = incoming_command.substring(3,6).toInt();
      response += burnerValve(requestedPercent) + "\n";
      incoming_command = incoming_command.substring(6);
      communication_timeout_counter = 0;
    }
    else if (commandType == "FPM"){
      int requestedPump1 = incoming_command.substring(3,6).toInt();
      int requestedPump2 =  incoming_command.substring(7,10).toInt();
      response += fuelPump(requestedPump1, requestedPump2) + "\n";
      incoming_command = incoming_command.substring(10);
      communication_timeout_counter = 0;
    }
    else {
      response += "inv\n";
      incoming_command = incoming_command.substring(1);
    }
  }
  return response;
}

void emergencyStop(){
  fuelPump(0,0);
  setStarterMotorDutyCycle(0);
  setGlowPlugDutyCycle(0);
  delay(600); //delay 600 ms to avoid fuel buildup
  burnerValve(0);
  fuelValve(0);
}

String setStarterMotorDutyCycle(int dutyCycle){
  if (dutyCycle >= 0 && dutyCycle <= STARTER_MOTOR_COUNTER_MAX){
    starterMotor_dutyCycle = dutyCycle;
    String response = String("smd") + String(starterMotor_dutyCycle);
    return response;
  }
  else return "inv";
}

String setGlowPlugDutyCycle(int dutyCycle){
  if (dutyCycle >= 0 && dutyCycle <= GLOW_PLUG_COUNTER_MAX){
    glowPlug_dutyCycle = dutyCycle;
    String response = String("gpd") + String(glowPlug_dutyCycle);
    return response;
  }
  else return "inv";
}


int requestTemperature(){
   //Prompt temperature
  I2c.read(ENGINE_ADDRESS, 0x0b, 3);
  int byte1 = I2c.receive();
  int byte2 = I2c.receive();
  int byte3 = I2c.receive();
  int temperature = 0;
  if (byte1+byte2+byte3==245){
    temperature=byte1*1.0193+261*byte2-30.451;
  }
  return temperature;
}

long requestRPM(){
  //Prompt RPM
  I2c.read(ENGINE_ADDRESS, 0x08, 3);
  long byte1 = I2c.receive();
  long byte2 = I2c.receive();
  long byte3 = I2c.receive();
  long rpm = 0;
  if (byte1+byte2+byte3==248){
    rpm=5.025*byte1+1300*byte2-33.439;
  }
  //Serial.println(rpm);
  return rpm;
}

String burnerValve(int percent){
   //Turn on burner valve
//  Wire.beginTransmission(ENGINE_ADDRESS);
//  Wire.write(0x01);Wire.write(0x1f);Wire.write(percent);Wire.write(225-percent);
//  Wire.endTransmission();
  uint8_t data[3] = {0x1f, percent, 225-percent};
  I2c.write(ENGINE_ADDRESS, ENGINE_ADDRESS, data, 3);
  return "bvp" + String(percent);
}

String fuelValve(int percent){
  //Turn on fuel valve
//  Wire.beginTransmission(ENGINE_ADDRESS);
//  Wire.write(0x01);Wire.write(0x1e);Wire.write(percent);Wire.write(226-percent);
//  Wire.endTransmission();
  uint8_t data[3] = {0x1e, percent, 226-percent};
  I2c.write(ENGINE_ADDRESS, ENGINE_ADDRESS, data, 3);
  return "fvp" + String(percent);
}

//String fuelPump(int input){
//  if (input == 0){ //turn off pump
//    fuelPumpMode = 0;
//    return "fpm0";
//  }
//  else if (input == 1){ //turn on phase 1
//    fuelPumpMode = 1;
//    return "fpm1";
//  }
//  else if (input == 2){ //turn on phase 2
//    fuelPumpMode = 2;
//    return "fpm2";
//  }
//  else return "inv";
//}

String fuelPump(int input1, int input2){
  pumpvalue1=input1;
  pumpvalue2=input2;
  return "fpm" + String(input1) + "," + String(input2);
}

//void fuelPumpControl(){
//  //Serial.println("fuelPumpControl");
//  if (fuelPumpMode == 1){
//    Wire.beginTransmission(PUMP_ADDRESS);
//    Wire.write(0x01);Wire.write(0x1c);Wire.write(0xA0);Wire.write(0x00);Wire.write(0x10);Wire.write(0x34);Wire.write(0x07);
//    Wire.endTransmission();
//  }
//  else if (fuelPumpMode == 2){
//    Wire.beginTransmission(PUMP_ADDRESS);
//    Wire.write(0x01);Wire.write(0x1c);Wire.write(0xA0);Wire.write(0x00);Wire.write(0x15);Wire.write(0x2F);Wire.write(0x07);
//    Wire.endTransmission();
//  }
//  else{
//    Wire.beginTransmission(PUMP_ADDRESS);
//    Wire.write(0x01);Wire.write(0x1c);Wire.write(0x00);Wire.write(0x00);Wire.write(0x00);Wire.write(0xe4);Wire.write(0x07);
//    Wire.endTransmission();
//  }
//}

void fuelPumpControl(){
  //Serial.println("fuelPumpControl");  
  byte pumpByte1 = pumpvalue1;
  byte pumpByte2 = pumpvalue2;
  //Serial.println(pumpByte1);

  uint8_t data[5] = {0x1c, pumpByte1, 0x00, pumpByte2, 256 - ((0x1c+pumpByte1+pumpByte2) % 256)};
  //delay(10);
  I2c.write(PUMP_ADDRESS, 0x01, data, 5);
  
//  if (pumpvalue2<255){
////    Wire.beginTransmission(PUMP_ADDRESS);
////    Wire.write(0x01);Wire.write(0x1c);Wire.write(pumpByte1);Wire.write(0x00);Wire.write(pumpByte2);Wire.write(228-pumpvalue1-pumpByte2);Wire.write(0x07);
////    Wire.endTransmission();
//
////    uint8_t data[6] = {0x1c, pumpByte1, 0x00, pumpByte2, 228-pumpByte1-pumpByte2, 0x07};
////    I2c.write(PUMP_ADDRESS, 0x01, data, 6);
//  }
//  else{
////    Wire.beginTransmission(PUMP_ADDRESS);
////    Wire.write(0x01);Wire.write(0x1c);Wire.write(pumpByte1);Wire.write(0x00);Wire.write(pumpByte2);Wire.write(484-pumpvalue1-pumpByte2);Wire.write(0x07);
////    Wire.endTransmission();
//      
////    uint8_t data[6] = {0x1c, pumpByte1, 0x00, pumpByte2, 484-pumpByte1-pumpByte2, 0x07};
////    I2c.write(PUMP_ADDRESS, 0x01, data, 6);
//  }
}

