#include <TimerOne.h>

#define READINGS_PER_SECONDS 1
#define TIMER_LENGTH (1000000/READINGS_PER_SECONDS)

String incoming_command = "";  //used for receiving commands

void setup(){
  Serial.begin(115200);
  Timer1.initialize(TIMER_LENGTH); // set the length of the timer
  Timer1.attachInterrupt(report_data_ISR); // attach the service routine here
}
 
void loop(){
  // Main code loop
  // TODO: Put your regular (non-ISR) logic here
  if (Serial.available() > 0){
    Timer1.detachInterrupt(); //momentarily disable interrupts
    incoming_command = Serial.readString();
    String response = handle_command(incoming_command);
    Serial.println(response);
    Timer1.attachInterrupt(report_data_ISR); //reenable interrupts
  }
}

String handle_command(String command){
  // handle commands here
  if (command == "abc"){
    return "command received";
  }
  return "";
}
 
/// --------------------------
/// Custom ISR Timer Routine
/// --------------------------
void report_data_ISR(){
    Serial.println("sample test data");
}
