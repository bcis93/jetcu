#include <Wire.h>
#define ENGINE_ADDRESS 0x01
#define PUMP_ADDRESS 0x02 //currently not used...

//timer setup for timer0, timer1, and timer2.
//For arduino uno or any board with ATMEL 328/168.. diecimila, duemilanove, lilypad, nano, mini...

//this code will enable two arduino timer interrupts.
//timer0 will not be changed
//timer1 will interrupt at 266Hz
//timer2 will interrupt at 20kHz



//constants and variables for starter motor
#define STARTER_MOTOR_PIN 6
#define STARTER_MOTOR_DUTY_CYCLE 3
#define STARTER_MOTOR_COUNTER_MAX 9
int starterMotor_dutyCycle = 0;

//constants and variables for glow plug
#define GLOW_PLUG_PIN 5
#define GLOW_PLUG_DUTY_CYCLE 5
#define GLOW_PLUG_COUNTER_MAX 9
int glowPlug_dutyCycle = 0;

//constants and variables for serial communication
String incoming_command = "";

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
  Wire.setClock(35000);
}

void init_interrupts(){
  cli();//stop interrupts

  //set timer0 interrupt at 266Hz
//  TCCR0A = 0;// set entire TCCR2A register to 0
//  TCCR0B = 0;// same for TCCR2B
//  TCNT0  = 0;//initialize counter value to 0
//  // set compare match register for 266hz increments
//  OCR0A = 234;// = (16*10^6) / (266*256) - 1 (must be <256)
//  // turn on CTC mode
//  TCCR0A |= (1 << WGM01);
//  // Set CS02 bit for 256 prescaler
//  TCCR0B |= (1 << CS02);   
//  // enable timer compare interrupt
//  TIMSK0 |= (1 << OCIE0A);

  //set timer1 interrupt at 266Hz
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 1hz increments
  OCR1A = 7518;// = (16*10^6) / (266*8) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS11 for 8 prescaler
  TCCR1B |= (1 << CS11);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);

  //set timer2 interrupt at 20kHz
  TCCR2A = 0;// set entire TCCR2A register to 0
  TCCR2B = 0;// same for TCCR2B
  TCNT2  = 0;//initialize counter value to 0
  // set compare match register for 20khz increments
  OCR2A = 100;// = (16*10^6) / (20000*8) - 1 (must be <256)
  // turn on CTC mode
  TCCR2A |= (1 << WGM21);
  // Set CS21 bit for 8 prescaler
  TCCR2B |= (1 << CS21);   
  // enable timer compare interrupt
  TIMSK2 |= (1 << OCIE2A);


  sei();//allow interrupts
}

//ISR(TIMER0_COMPA_vect){//timer0 interrupt 266Hz for glow plug
//    static int counter = 0;
//    if (counter < GLOW_PLUG_DUTY_CYCLE){
//      digitalWrite(GLOW_PLUG_PIN, HIGH );
//      counter++;
//    }
//    else if (counter < GLOW_PLUG_COUNTER_MAX){
//      digitalWrite(GLOW_PLUG_PIN, LOW);
//      counter++;
//    }
//    else{
//      counter = 0;
//    }
//}

ISR(TIMER1_COMPA_vect){//timer1 interrupt 10Hz prints serial data
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
  
ISR(TIMER2_COMPA_vect){//timer2 interrupt 20kHz for starter motor
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
  //cli();//stop interrupts
  if (Serial.available() > 0){
    
    incoming_command = Serial.readString();
    String response = handle_command(incoming_command);
    Serial.println(response);
    
  }
  //sei();//enable interrupts
}

String handle_command(String command){
  // handle commands here
  String commandType = command.substring(0,3);
  if (commandType == "SMD"){
    int requestedDutyCycle = command.substring(3).toInt();
    String response = setStarterMotorDutyCycle(requestedDutyCycle);
    return response;
  }
  else if (commandType == "GPD"){
    int requestedDutyCycle = command.substring(3).toInt();
    String response = setGlowPlugDutyCycle(requestedDutyCycle);
    return response;
  }
  else if (commandType == "END"){
    setStarterMotorDutyCycle(0);
    setGlowPlugDutyCycle(0);
    return "end";
  }
  else if (commandType == "RQT"){
    int temp = requestTemperature();
    return "rqt" + temp;
  }
  else if (commandType == "RQR"){
    long rpm = requestRPM();
    return "rpm" + rpm;
  }
  else return "inv";
}

String setStarterMotorDutyCycle(int dutyCycle){
  if (dutyCycle >= 0 && dutyCycle <= 10){
    starterMotor_dutyCycle = dutyCycle;
    String response = String("smd") + String(starterMotor_dutyCycle);
    return response;
  }
  else return "inv";
}

String setGlowPlugDutyCycle(int dutyCycle){
  if (dutyCycle >= 0 && dutyCycle <= 10){
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
    //Output temperature to serial
    if (byte1+byte2+byte3==248){
      rpm=5.025*byte1+1300*byte2-33.439;
      //Serial.print(rpm);
      //Serial.println(" RPM");
    }
  }
  Wire.endTransmission();
  return rpm;
}

void burnerValve(int percent){
   //Turn on burner valve
  Wire.beginTransmission(ENGINE_ADDRESS);
  Wire.write(0x01);Wire.write(0x1f);Wire.write(percent);Wire.write(225-percent);
  Wire.endTransmission();
}

void fuelValve(int percent){
  //Turn on fuel valve
  Wire.beginTransmission(ENGINE_ADDRESS);
  Wire.write(0x01);Wire.write(0x1e);Wire.write(percent);Wire.write(226-percent);
  Wire.endTransmission();
}

