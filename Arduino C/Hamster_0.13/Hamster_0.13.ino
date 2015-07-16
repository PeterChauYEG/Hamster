/* Hamster 0.1 created by Peter Chau
   June 5, 2015
   
   Hardware: Arduino Uno, TI DRV8833 Dual H-Bridge Motor Driver, HC-SR04 Ultra01 + Ultrasonic Range Finder
   
   Hamster checks if ultraSensor sees anything 30 cm in front of it, and quickly rotates right if it does. Otherwise, Hamster drives forward slowly.
   It's scared of things!
    
   To do:
   Make Hamster look around then determine the best direction to drive
   Install compass
   Make hamster drive staight
   Add On/Off switch and function
   Add On/Off LED indicator
   Add exit AAN if a probability threshold is reached
   Add mode switch
   Create lower limit for probabilities
        
   Log: 

   7/6/2015
   Installed DVR8833 library
   Mapped dutyCycle to motorSpeed
   I seem to have fired my Arduino UNO. Computer won't recognize it however when plugged into the wall via USB, the board works fine.

   7/7/2015
   Created Breadboard Diagram
   Created Schematic

   7/8/2015
   Arduino Uno now functioning - Window's driver needed to be installed. Gave it a fresh battery charge aswell.
   Updated Breadboard and schematic - was missing Uno VIN.

   7/9/2015
   Create DC Motor drive function. void driveTrain(int instruction, int dutyCycle, int period)
   Implemented driveTrain()
   Created better serial messages
   Reworked ultrasonic rangefinder pinging as a state function
   ultrasonic rangefind needs to be calibrated. wtf. it's slow.
   ok now the robot works but is really dumb lol. it runs into stuff... lag time... momentum... go and :speed up ping

   7/10/2015
   Ultrasonic rangefinder working well, however there is lag between when it detects an object and when it takes action.
      Calibrated Sensor
   Poor wheel alignment
   Implemented a status LED

   7/12/2015
   Added a Feedforward Neural Network for obstacle avoidance
   USB drive on computer mangled again.... Have thought out robot more. Very happy with current results.

   7/13/2015
   The issue was the USB cable. It is now fixed!
   Changed neural network adjustments to small decreases and increases (from 100% to 1%, and 33% to 0.33%)
   Add max training attempts (50)
      Add exit if n training attempts reached
   Add EEPROM library
      Update probabilities to EEPROM
      Nevermind... blew max writes on mem 0 already. Fyuck that. Will wait for bluetooth communications
   Now using 5 differant actions (stop, forwards, backwards, rotate right, rotate left)
   Add active Feedforward Back Proprogation Neutal Network
      Add "training mode" 
      Add LED to indication which mode Hamster is in
 */
 
#include <NewPing.h> // Include NewPing to handle ultraSensor data
#include <DRV8833.h> // Include DRV8833 Dual Motor Driver Carrier - Pololu  

/* Ultrasonic Rangefinder constants and variables */
const int ultraSensorTriggerPin = 12;  // Arduino pin tied to trigger pin on the ultrasonic sensor.
const int ultraSensorEchoPin = 13;  // Arduino pin tied to echo pin on the ultrasonic sensor.
const int maxDistance = 100;
const int pingSpeed = 50; // 50ms between pings
const int safeZone = 40; // 30cm between Hamster and any object
long pingTimer = 0; // will store last time ping occurred
unsigned int ultraSensorRaw; // will store raw ultrasensor range finder distance
int ultraSensorCM[1]; // will stored distance from object in cm

/* Drive Train constant and variables */
const int rightMotor1 = 11;   //PWM control Right Motor -
const int rightMotor2 = 10;   //PWM control Right Motor +
const int leftMotor1 = 5;  //PWM control Left Motor +
const int leftMotor2 = 6;  //PWM control Left Motor -
int driveInstruction = random(3); // Random initial command
int dutyCycle = 0; // 0% duty cycle PWM
int motorSpeed;

/* Status LED constants and variables */
const int statusLED[] = {4,2,1}; // Array for Status LED with pins for red, green, and blue
int status = 0;

/* Neural Network constants and variables */
const int modeButton = 8; // Momentary switch for putting Hamster in learn mode
const int modeLED = 9; // LED to indicate what mode we are in
int modeState = LOW; // Set inital state to drive mode (learn mode off)
int lastModeState = LOW; // Previous mode
float probability[] = {0.200, 0.200, 0.200, 0.200, 0.200}; // Set equal initial probabilities 
int maxAttempts = 100; // Set max attempts to learn
int learningAttempts = 0; // Set initial attempts to learn
long lastDebounceTime = 0;  // the last time the output pin was toggled
long debounceDelay = 250;    // the debounce time; increase if the output flickers

DRV8833 driver = DRV8833(); // Create an instance of the DRV8833:
NewPing ultraSensor(ultraSensorTriggerPin, ultraSensorEchoPin, maxDistance); // NewPing setup of pins and maximum distance.

void setup(){
  Serial.begin(9600);             // Start Serial connection
  pinMode(modeButton, INPUT); // Initialize the pushbutton pin as an input
/* Attach the motors to the input pins: */
  driver.attachMotorA(rightMotor1, rightMotor2);
  driver.attachMotorB(leftMotor1, leftMotor2);
/* Attach status LED to output pins */
  for(int x = 0; x <3; x++) {
  pinMode(statusLED[x], OUTPUT);
  }
  pinMode(modeLED, OUTPUT); 
  Serial.println("H A M S T E R V0.123\n");
  statusLed(0); // Set status LED to Ready (green)
}

void loop(){

/* For setting Hamster's modes: Learning or Normal */  
  int reading = digitalRead(modeButton); // Read state of mode button
  if (reading != lastModeState){
    lastDebounceTime = millis(); // Reset debouncing timer if switch is changed due to noise or excessive pressing
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != modeState){
      modeState = reading;  // Make sure the reading has been the same for a period then set mode
    }
  }
  
/* Measure the distanace to closest object */
  unsigned long currentMillis = millis(); //record current time
  if(currentMillis - pingTimer > pingSpeed) { // save the last time you pinged
  pingTimer += pingSpeed; // update time since last ping
  statusLed(2); // Object avoidance status (light red)
  ultraSensorRaw = ultraSensor.ping(); // Send ping, get ping time in microseconds (uS).
  ultraSensorCM[0] = ultraSensorRaw / US_ROUNDTRIP_CM; // Convert ping time to distance in cm and print result (0 = outside set distance range)
  }
/* Hamster checks if ultraSensor sees anything 20 cm in front of it, and rotates right if it does. Otherwise, Hamster drives forward.*/
  if(ultraSensorCM[0] > 0 && ultraSensorCM[0] <= safeZone){     
/* Print to Serial ultraSensorCM */
    Serial.print("Distance to Closest Object: ");
    Serial.print(ultraSensorCM[0]); 
    Serial.println("cm");

/* Feedforward Neural Network with Back Proprogation */
    driveInstruction = weightedRandom(probability); // Use probability to pick an action and perform it
    dutyCycle = 75;
    driveTrain(driveInstruction, dutyCycle);   
      
/* Set to Learning Mode if mode button is pressed*/
    if(modeState == HIGH) {
/* Use Neural Network if Hamster tried to learn less than max attempts limit */
    if ( learningAttempts < maxAttempts){  
    digitalWrite(modeLED, HIGH); // Turn on Learning Mode LED
    Serial.println("Learning Mode");
    ultraSensorRaw = ultraSensor.ping(); // Ping and save it to ultraSensorCM[1]
    ultraSensorCM[1] = ultraSensorRaw / US_ROUNDTRIP_CM; 
    
    if (ultraSensorCM[1] == ultraSensorCM[0]){ // Check if action was successful based on change in distance
      for(int x = 0; x < 5; x++) {  // Decrease drive instruction's probability
        if (x == driveInstruction) {
          probability[x] = probability[x] - 0.01;
        } else {
          probability[x] = probability[x] + 0.0025;
        }
        status = 2; // Object avoidance status (blue)
      }
    }
    else if (ultraSensorCM[1] < ultraSensorCM[0]) {
      for(int x = 0; x < 5; x++) {  // Decrease drive instruction's probability
        if (x == driveInstruction) {
          probability[x] = probability[x] - 0.01;
        } else {
          probability[x] = probability[x] + 0.0025;
        }
        status = 2; // Object avoidance status (blue)
      }      
    }
    else { 
      for(int x = 0; x < 5; x++) {  // Increase drive instruction's probability
        if (x == driveInstruction) {
          probability[x] = probability[x] + 0.01;
        } else {
          probability[x] = probability[x] - 0.0025;
        }
        status = 3; // Object avoidance status (purple)        
      }      
    }
    Serial.println("Probability: "); // Print current drive train probabilities
    for (int x = 0; x < 5; x++){
    Serial.print(probability[x]);
    Serial.print("\t");    
    }
    Serial.print("\t(Stop, Forward, Backwards, Rotate Right, Rotate Left)\n");
    
    Serial.print("Learning Attempts: "); // Print current attempts
    Serial.print(learningAttempts);
    Serial.print("\n");   
    learningAttempts++;
    }
    }
  } else {
    driveTrain(1, 75); // Go forwards at 75% duty cycle
    status = 0; // Ready status (green)
  }
  
  statusLed(status);
  Serial.println("--------------------------------------------\n");
  
  if(modeState == LOW || learningAttempts >= maxAttempts){
      digitalWrite(modeLED, LOW); // Turn off Learning Mode LED
    }
  lastModeState = reading; // update last mode state
  
} // loop() end

/* Drive Train for 2 motors on opposite sides */
void driveTrain(int instruction, int dutyCycle){
  motorSpeed = map(dutyCycle, 0, 100, 0, 255); // Map motorSpeed (255 - 0) to dutyCycle (0 - 100)
  
/* Drive Train data */
  Serial.print("Duty Cycle: ");
  Serial.print(dutyCycle); 
  Serial.print("%\n");

  switch(instruction){
   case 0: // Stop
    Serial.println("Stop\n");
    driver.motorAStop();
    driver.motorBStop();
    break;
   case 1: // Forward
    Serial.println("Forward\n");
    driver.motorAForward(motorSpeed);
    driver.motorBForward(motorSpeed);
    break;
   case 2: // Backwards
    Serial.println("Backward\n");
    driver.motorAReverse(motorSpeed);
    driver.motorBReverse(motorSpeed);
    break; 
   case 3: // Rotate right
    Serial.println("Rotate Right\n");
    driver.motorAReverse(motorSpeed);
    driver.motorBForward(motorSpeed);
    break;
   case 4: // Rotate left
    Serial.println("Rotate Left\n");
    driver.motorAForward(motorSpeed);
    driver.motorBReverse(motorSpeed);
    break;     
  }
  return;
}

/* Controls status LED*/
void statusLed(int status){
  switch(status){
  case 0: // set to LED green to indicate status
  digitalWrite(statusLED[1], LOW);
  digitalWrite(statusLED[0], LOW); 
  if(digitalRead(statusLED[2]) != HIGH) {
    digitalWrite(statusLED[2], HIGH);
  }
  break;  
  case 1: // set to LED red to indicate status
  digitalWrite(statusLED[2], LOW);
  digitalWrite(statusLED[1], LOW); 
  if(digitalRead(statusLED[0]) != HIGH) {
    digitalWrite(statusLED[0], HIGH);
  }
  break;

  case 2: // set to LED blue to indicate status
  digitalWrite(statusLED[0], LOW);
  digitalWrite(statusLED[2], LOW); 
  if(digitalRead(statusLED[1]) != HIGH) {
    digitalWrite(statusLED[1], HIGH);
  }
  break;

  case 3: // set to LED purple to indicate status
  digitalWrite(statusLED[1], LOW); 
  if(digitalRead(statusLED[2]) != HIGH) {
    digitalWrite(statusLED[2], HIGH);
  }
  if(digitalRead(statusLED[0]) != HIGH) {
    digitalWrite(statusLED[0], HIGH);
  }
  break; 
  case 4: // set to LED light blue to indicate status
  digitalWrite(statusLED[0], LOW); 
  if(digitalRead(statusLED[2]) != HIGH) {
    digitalWrite(statusLED[2], HIGH);
  }
  if(digitalRead(statusLED[1]) != HIGH) {
    digitalWrite(statusLED[1], HIGH);
  }
  break; 
  case 5: // set to LED light red to indicate status
  digitalWrite(statusLED[2], LOW); 
  if(digitalRead(statusLED[0]) != HIGH) {
    digitalWrite(statusLED[0], HIGH);
  }
  if(digitalRead(statusLED[1]) != HIGH) {
    digitalWrite(statusLED[1], HIGH);
  }
  break; 
  }
}

/* Weighted Random Choice function  */
int weightedRandom(float* weights){
  float seed = random(10); // seed the countdown
  float choice = seed * 0.10; // reduce the seed and save into countdown
  for (int x = 0; x < 5; x++){ // minus each probability from choice 
    choice -= weights[x]; 
    if (choice < 0){ 
      return x; // return weight index
    }
  }
}
