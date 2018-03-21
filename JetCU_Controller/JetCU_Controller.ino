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




#include <Wire.h>
#define ENGINE_ADDRESS 0x01
#define PUMP_ADDRESS 0x02


//timer setup for timer0, timer1, and timer2.
//For arduino uno or any board with ATMEL 328/168.. diecimila, duemilanove, lilypad, nano, mini...

//this code will enable two arduino timer interrupts.
//timer0 will not be changed
//timer1 will interrupt at 532Hz
//timer2 will interrupt at 40kHz


//constants and variables for starter motor
#define STARTER_MOTOR_PIN 6
#define STARTER_MOTOR_COUNTER_MAX 19
int starterMotor_dutyCycle = 0; //maybe add volatile

//constants and variables for glow plug
#define GLOW_PLUG_PIN 5
#define GLOW_PLUG_COUNTER_MAX 19
int glowPlug_dutyCycle = 0;

//constants and variables for serial communication
String incoming_command = "";

//variable for fuel pump mode
//int fuelPumpMode = 0;
int pumpvalue1=0;
int pumpvalue2=0;
bool fuelPumpFlag = false;

//constants for testing
//to be removed later
#define TIMER_ONE_PIN 7

void setup(){
  
  //set pins as outputs
  pinMode(STARTER_MOTOR_PIN, OUTPUT);
  pinMode(GLOW_PLUG_PIN, OUTPUT);
  pinMode(TIMER_ONE_PIN, OUTPUT);

  init_interrupts();

  Serial.begin(115200);
  Serial.setTimeout(10); //sets the timeout of the serial read to 10ms

  //setup I2C communication
  Wire.begin();
  Wire.setClock(20000);
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
  static int counter = 0;
  counter++;
  if (counter >= 500){
    fuelPumpFlag = true;
    counter = 0;
  }
  else{
    fuelPumpFlag = false;
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
  if (Serial.available() > 0){
    incoming_command = Serial.readString();
    String response = handle_command(incoming_command);
    Serial.println(response);
  }

  if (fuelPumpFlag){
    fuelPumpControl();
  }
}

String handle_command(String command){
  //Serial.println("handle_command");
  // handle commands here
  String response = "";
  while (command.length() > 1){
    command.trim();
    //Serial.println("command 2: " + command);
    String commandType = command.substring(0,3);
    //Serial.println("command type: " + commandType);
    if (commandType == "SMD"){
      int requestedDutyCycle = command.substring(3,5).toInt();
      response += setStarterMotorDutyCycle(requestedDutyCycle) + "\n";
      command = command.substring(5);
    }
    else if (commandType == "GPD"){
      int requestedDutyCycle = command.substring(3,5).toInt();
      response += setGlowPlugDutyCycle(requestedDutyCycle) + "\n";
      command = command.substring(5);
    }
    else if (commandType == "END"){
      fuelPump(0,0);
      setStarterMotorDutyCycle(0);
      setGlowPlugDutyCycle(0);
      burnerValve(0);
      fuelValve(0);
      response += "end\n";
      command = "";
    }
    else if (commandType == "RQT"){
      int temp = requestTemperature();
      response += ("rqt" + String(temp)) + "\n";
      command = command.substring(3);
    }
    else if (commandType == "RQR"){
      long rpm = requestRPM();
      response += ("rqr" + String(rpm)) + "\n";
      command = command.substring(3);
    }
    else if (commandType == "FVP"){
      int requestedPercent = command.substring(3,5).toInt();
      response += fuelValve(requestedPercent) + "\n";
      command = command.substring(5);
    }
    else if (commandType == "BVP"){
      int requestedPercent = command.substring(3,5).toInt();
      response += burnerValve(requestedPercent) + "\n";
      command = command.substring(5);
    }
    else if (commandType == "FPM"){
      int requestedPump1 = command.substring(3,6).toInt();
      int requestedPump2 =  command.substring(7,10).toInt();
      response += fuelPump(requestedPump1, requestedPump2) + "\n";
      command = command.substring(10);
    }
    else {
      response += "inv\n";
      command = "";
    }
  }
  return response;
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
  Wire.beginTransmission(ENGINE_ADDRESS);
  Wire.write(0x0b);
  Wire.endTransmission(false);
  Wire.requestFrom(ENGINE_ADDRESS,3);
  int temperature = 0;
  while (Wire.available()) { 
    int byte1 = Wire.read();
    int byte2 = Wire.read();
    int byte3 = Wire.read();
    //Output temperature to serial
    if (byte1+byte2+byte3==245){
      temperature=byte1*1.0193+261*byte2-30.451;
      //Serial.print(temperature);
      //Serial.println("C");
    }
  }
  Wire.endTransmission();
  return temperature;
}

long requestRPM(){
  //Prompt RPM
  Wire.beginTransmission(ENGINE_ADDRESS);
  Wire.write(0x08);
  Wire.endTransmission(false);
  Wire.requestFrom(ENGINE_ADDRESS,3);
  long rpm = 0;
  while (Wire.available()) { 
    int byte1 = Wire.read();
    int byte2 = Wire.read();
    int byte3 = Wire.read();
    if (byte1+byte2+byte3==248){
      rpm=5.025*byte1+1300*byte2-33.439;
      //Serial.print(rpm);
      //Serial.println(" RPM");
    }
  }
  Wire.endTransmission();
  return rpm;
}

String burnerValve(int percent){
   //Turn on burner valve
  Wire.beginTransmission(ENGINE_ADDRESS);
  Wire.write(0x01);Wire.write(0x1f);Wire.write(percent);Wire.write(225-percent);
  Wire.endTransmission();
  return "bvp" + String(percent);
}

String fuelValve(int percent){
  //Turn on fuel valve
  Wire.beginTransmission(ENGINE_ADDRESS);
  Wire.write(0x01);Wire.write(0x1e);Wire.write(percent);Wire.write(226-percent);
  Wire.endTransmission();
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
  if (pumpvalue2<255){
    Wire.beginTransmission(PUMP_ADDRESS);
    Wire.write(0x01);Wire.write(0x1c);Wire.write(pumpByte1);Wire.write(0x00);Wire.write(pumpByte2);Wire.write(228-pumpvalue1-pumpByte2);Wire.write(0x07);
    Wire.endTransmission();
  }
  else{
    Wire.beginTransmission(PUMP_ADDRESS);
    Wire.write(0x01);Wire.write(0x1c);Wire.write(pumpByte1);Wire.write(0x00);Wire.write(pumpByte2);Wire.write(484-pumpvalue1-pumpByte2);Wire.write(0x07);
    Wire.endTransmission();
  }
}

