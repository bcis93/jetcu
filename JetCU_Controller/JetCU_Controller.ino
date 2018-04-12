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

#include <I2C.h>
#define ENGINE_ADDRESS 1
#define PUMP_ADDRESS 2
#define RPM_REGISTER 8
#define TEMPERATURE_REGISTER 11
#define FUEL_VALVE_REGISTER 30
#define BURNER_VALVE_REGISTER 31
#define IDENTIFY_REGISTER 21


//constants and variables for safety checks
bool emergencyStopFlag = false;
#define COMMUNICATION_TIMEOUT 500 //in milliseconds
int communication_timeout_counter = 0;
/********* NOTE: communication timeout is not actually implemented yet *********/

//constants and variables for starter motor
#define STARTER_MOTOR_PIN 6
#define STARTER_MOTOR_COUNTER_MAX 19
int starterMotor_dutyCycle = 0;

//constants and variables for glow plug
#define GLOW_PLUG_PIN 5
#define GLOW_PLUG_COUNTER_MAX 19
int glowPlug_dutyCycle = 0;

//constants and variables for serial communication
String incoming_command = "";

//variable for fuel pump mode
volatile int pumpvalue1=0;
volatile int pumpvalue2=0;
bool fuelPumpFlag = false;
#define FUEL_PUMP_REFRESH_RATE 250 //fuel pump will be updated every 250 ms


void setup(){
  
  //set pins as outputs
  pinMode(STARTER_MOTOR_PIN, OUTPUT);
  pinMode(GLOW_PLUG_PIN, OUTPUT);

  //intialize interrupts for PWM
  init_interrupts();

  //serial communication
  Serial.begin(115200);
  Serial.setTimeout(10); //sets the timeout of the serial read to 10ms

  //I2C communication
  I2c.begin();
  I2c.timeOut(100); //100 ms timeout
}


/*
 * this code will enable two arduino timer interrupts.
 * timer0 will not be changed
 * timer1 will interrupt at 532Hz
 * timer2 will interrupt at 40kHz
 */
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

/*
 * Timer0 ISR
 * This function runs every 1ms
 * 
 */
ISR(TIMER0_COMPA_vect){
  static int fuel_pump_counter = 0;
  fuel_pump_counter++;
  communication_timeout_counter++;
  if (fuel_pump_counter >= FUEL_PUMP_REFRESH_RATE){
    fuelPumpFlag = true; //sets a flag that will be used in the main loop
    fuel_pump_counter = 0;
  }
  else{
    fuelPumpFlag = false;
  }
  
  /********* NOTE: communication timeout is not actually implemented yet *********/
  if (communication_timeout_counter > COMMUNICATION_TIMEOUT){
    emergencyStopFlag = true;
  }
  else{
    emergencyStopFlag = false;
  }
}

/*
 * Timer1 ISR
 * This function runs 532 times per second.
 * It outputs the PWM for the glow plug.
 */
ISR(TIMER1_COMPA_vect){
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
  
/*
 * Timer2 ISR
 * This funtion runs 40k times per second
 * It outputs the PWM for the starter motor.
 */
ISR(TIMER2_COMPA_vect){
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
  /********* NOTE: Add a check here, similar to this one, to implement the communication timeout *********/
//  if (emergencyStopFlag){
//    //stop!!
//  }
//  else{

    if (Serial.available() > 0){ //if there is a command on the serial buffer
    incoming_command = Serial.readString(); //read incoming string
    String response = handle_command(incoming_command); //handle the incoming command(s)
    Serial.println(response); //print a response
    }
  
    if (fuelPumpFlag){ //if the fuel pump flag is set, update the fuel pump
      fuelPumpControl();
    }
    
//  }

}

/*
 * This funtion handles the incoming command buffer.
 * It will go through all the commands in the buffer
 * and handle each one, one at a time.
 */
String handle_command(String command){
  String response = ""; //start with an empty response
  while (command.length() > 1){ //continues until all commands have been handled
    
    command.trim(); //removes extra whitespace before and after command
    String commandType = command.substring(0,3); //the first 3 characters will be the command type (ex. RQR, SMD, etc)
    
    if (commandType == "SMD"){ //Starter motor duty cycle
      int requestedDutyCycle = command.substring(3,5).toInt();
      response += setStarterMotorDutyCycle(requestedDutyCycle) + "\n";
      command = command.substring(5);
      communication_timeout_counter = 0;
    }
    
    else if (commandType == "GPD"){ //Glow plug duty cycle
      int requestedDutyCycle = command.substring(3,5).toInt();
      response += setGlowPlugDutyCycle(requestedDutyCycle) + "\n";
      command = command.substring(5);
      communication_timeout_counter = 0;
    }
    
    else if (commandType == "END"){ //Shutoff
      response += "end\n";
      communication_timeout_counter = 0;
      emergencyStop();
      command = "";
    }
    
    else if (commandType == "RQT"){ //Request temperature
      int temp = requestTemperature();
      char tmp[5];
      sprintf(tmp, "%04d", temp);
      response += ("rqt" + String(tmp)) + "\n";
      command = command.substring(3);
      communication_timeout_counter = 0;
    }
    
    else if (commandType == "RQR"){ //Request RPM
      long rpm = requestRPM();
      char tmp[7];
      sprintf(tmp, "%06ld",rpm);
      response += ("rqr" + String(tmp)) + "\n";
      command = command.substring(3);
      communication_timeout_counter = 0;
    }

    else if (commandType== "RQE"){
      String identifier=identifyengine();
      response += (identifier + "\n");
      command = command.substring(3);
      communication_timeout_counter = 0;

    }
    
    else if (commandType == "FVP"){ //Fuel valve percent
      int requestedPercent = command.substring(3,6).toInt();
      response += fuelValve(requestedPercent) + "\n";
      command = command.substring(6);
      communication_timeout_counter = 0;
    }
    
    else if (commandType == "BVP"){ //Burner valve percent
      int requestedPercent = command.substring(3,6).toInt();
      response += burnerValve(requestedPercent) + "\n";
      command = command.substring(6);
      communication_timeout_counter = 0;
    }

    else if (commandType == "FPM"){ //Fuel pump mode
      int requestedPump1 = command.substring(3,6).toInt();
      int requestedPump2 =  command.substring(7,10).toInt();
      response += fuelPump(requestedPump1, requestedPump2) + "\n";
      command = command.substring(10);
      communication_timeout_counter = 0;
    }
    
    else { //invalid command
      response += "inv\n";
      command = command.substring(1);
    }
    
  }
  return response;
}

/*
 * This function turns off everything on the engine
 */
void emergencyStop(){
  fuelPump(0,0);
  setStarterMotorDutyCycle(0);
  setGlowPlugDutyCycle(0);
  delay(600); //delay 600 ms to avoid fuel buildup
  burnerValve(0);
  fuelValve(0);
}

/*
 * This function sets a global variable that is used
 * in the starter motor ISR to create different duty cycles
 */
String setStarterMotorDutyCycle(int dutyCycle){
  if (dutyCycle >= 0 && dutyCycle <= STARTER_MOTOR_COUNTER_MAX){
    starterMotor_dutyCycle = dutyCycle;
    String response = String("smd") + String(starterMotor_dutyCycle);
    return response;
  }
  else return "inv";
}

/*
 * This function sets a global variable that is used
 * in the glow plug ISR to create different duty cycles
 */
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
  I2c.read(ENGINE_ADDRESS, TEMPERATURE_REGISTER, 3); //read 3 bytes from the temperature register on the engine
  int byte1 = I2c.receive();
  int byte2 = I2c.receive();
  int byte3 = I2c.receive();
  int temperature = -1;
  if (byte1+byte2+byte3==245){
    temperature=byte1*1.0193+261*byte2-30.451; //Note: this integer multiplication may be slowing down the processor
  }
  return temperature;
}

long requestRPM(){
  //Prompt RPM
  I2c.read(ENGINE_ADDRESS, RPM_REGISTER, 3); //read 3 bytes from the RPM register on the engine
  long byte1 = I2c.receive();
  long byte2 = I2c.receive();
  long byte3 = I2c.receive();
  long rpm = -1;
  if (byte1+byte2+byte3==248){
    rpm=5.025*byte1+1300*byte2-33.439; //Note: this long multiplication may be slowing down the processor
  }
  return rpm;
}

String burnerValve(int percent){
   //Turn on burner valve
  uint8_t data[3] = {BURNER_VALVE_REGISTER, percent, 225-percent};
  I2c.write(ENGINE_ADDRESS, 0x01, data, 3); //write the 3 bytes in data to the engine address
  return "bvp" + String(percent);
}

String fuelValve(int percent){
  //Turn on fuel valve
  uint8_t data[3] = {FUEL_VALVE_REGISTER, percent, 226-percent};
  I2c.write(ENGINE_ADDRESS, 0x01, data, 3); //write the 3 bytes in data to the engine address
  return "fvp" + String(percent);
}

String fuelPump(int input1, int input2){
  pumpvalue1=input1;
  pumpvalue2=input2;
  return "fpm" + String(input1) + "," + String(input2);
}

void fuelPumpControl(){ 
  byte pumpByte1 = pumpvalue1;
  byte pumpByte2 = pumpvalue2;

  uint8_t data[5] = {0x1c, pumpByte1, 0x00, pumpByte2, 256 - ((0x1c+pumpByte1+pumpByte2) % 256)};
  I2c.write(PUMP_ADDRESS, 0x01, data, 5); //write the 5 bytes in data to the pump address
}

String identifyengine(){
  I2c.read(ENGINE_ADDRESS, IDENTIFY_REGISTER, 6); //read 6 bytes from identify register on engine
  int byte1 = I2c.receive();
  int byte2 = I2c.receive();
  int byte3 = I2c.receive();
  int byte4 = I2c.receive();
  int byte5 = I2c.receive();
  int byte6 = I2c.receive();
  return ("rqe"+String(byte1)+","+String(byte2)+","+String(byte3)+","+String(byte4)+","+String(byte5)+","+String(byte6));
   
}
