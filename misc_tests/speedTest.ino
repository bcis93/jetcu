void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
}

String incomingString = "";   // for incoming serial data

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available() > 0) {
    // read the incoming byte:
    incomingString = Serial.readString();
    //Serial.println(incomingString);
    if (incomingString == "abc"){
      Serial.println("command received!!");
    } 
  }
  Serial.println("1234567890abcdef12");
  //delay(10);
}
