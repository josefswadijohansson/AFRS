#include <AccelStepper.h>

// === Pin Definitions ===
#define M1_STEP_PIN 0    // Reel motor step
#define M1_DIR_PIN  1    // Reel motor direction

#define M2_STEP_PIN 2    // Carriage motor step
#define M2_DIR_PIN  3    // Carriage motor direction

#define HOME_SW_PIN 6    // Home limit switch
#define END_SW_PIN  5    // End limit switch

AccelStepper reelMotor(AccelStepper::DRIVER, M1_STEP_PIN, M1_DIR_PIN);
AccelStepper carriageMotor(AccelStepper::DRIVER, M2_STEP_PIN, M2_DIR_PIN);

float RAIL_LENGTH = 50;   // 100mm linear rail
float CENTER_POSITION = RAIL_LENGTH / 2.0f; // Center position

// === Motor Configuration ===
const int MOTOR_STEPS = 200;       // 200 steps/rev for NEMA 17
const int MICROSTEPPING = 8;       // Set on driver board (MS1/MS2 jumpers)
const float SCREW_LEAD = 4.0;      // mm per revolution (T8 screw)
const float CALIBRATION_FACTOR = 2.0;
const float STEPS_PER_MM = ((MOTOR_STEPS * MICROSTEPPING) / (SCREW_LEAD / CALIBRATION_FACTOR));

// === Speed Settings ===
const float REEL_RPM = 150;        // Reel motor speed
const float CARRIAGE_RPM = 100;    // Carriage motor speed

// State management
enum State {
  IDLE,
  HOMING, 
  FIND_END, 
  RUNNING
};
State currentState = HOMING;

// Manual distance tracking
long targetSteps = 0;
long stepsMoved = 0;
// Position tracking
float currentPositionMM = 0;  // Tracks actual position in mm

float desiredPosition = 25;
bool bounceBool = false;

void setup() {
  Serial.begin(115200);

  // Configure limit switches
  pinMode(HOME_SW_PIN, INPUT_PULLUP);
  pinMode(END_SW_PIN, INPUT_PULLUP);
  
  Serial.begin(115200);
  Serial.println("Starting motors...");

  // Configure reel motor (constant speed)
  reelMotor.setMaxSpeed((REEL_RPM * MOTOR_STEPS * MICROSTEPPING) / 60.0);
  reelMotor.setSpeed(-reelMotor.maxSpeed());  // Negative for reverse
  reelMotor.setMinPulseWidth(50);             // 50µs pulse width

  // Configure carriage motor (constant speed)
  carriageMotor.setMaxSpeed((CARRIAGE_RPM * MOTOR_STEPS * MICROSTEPPING) / 60.0);
  carriageMotor.setSpeed(carriageMotor.maxSpeed());  // Positive direction
  carriageMotor.setMinPulseWidth(50);                // 50µs pulse width
}

void loop() {
  // State machine for carriage
  switch(currentState) {
    case HOMING:
      runHoming();
      break;
      
    case FIND_END:
      runFindEnd();
      break;
      
    case RUNNING:
      //reelMotor.runSpeed();
      if(bounceBool == false){
        if(runMoveToPosition(desiredPosition - 0 )){
          bounceBool = true;
        }
      }
      else {
        if(runMoveToPosition(desiredPosition - 20.0)){
          bounceBool = false;
        }
      }
      break;
  }
}

void runHoming(){
    // Move carriage in positive direction
    if (carriageMotor.runSpeed()) {
      stepsMoved++;
    }

  if (digitalRead(HOME_SW_PIN) == LOW) {  // Switch pressed (LOW with pullup)
    carriageMotor.stop();             // Stop motor
    carriageMotor.setSpeed(0);        // Set speed to zero
    
    stepsMoved = 0;
    // Reset position tracking
    currentPositionMM = 0;
    carriageMotor.setCurrentPosition(0);

    // Calculate steps needed to reach center
    targetSteps = CENTER_POSITION * STEPS_PER_MM;

    carriageMotor.setSpeed(-carriageMotor.maxSpeed());  // Positive direction
        
    currentState = FIND_END;
  }
}

void runFindEnd() {
  // Run carriage at constant speed
  if (carriageMotor.runSpeed()) {
    stepsMoved++;
    // Update position based on direction
    currentPositionMM += (carriageMotor.speed() > 0) ? 
                         (1.0 / STEPS_PER_MM) : 
                         (-1.0 / STEPS_PER_MM);
    
    // Print position every 100ms
    // static unsigned long lastPrint = 0;
    // if (millis() - lastPrint > 100) {
    //   lastPrint = millis();
    //   Serial.print("Position: ");
    //   Serial.print(currentPositionMM);
    //   Serial.println(" mm");
    // }
  }

  if (digitalRead(END_SW_PIN) == LOW) {  // Switch pressed (LOW with pullup)
    currentState = RUNNING;

    // Stop carriage motor completely
    carriageMotor.stop();
    carriageMotor.setSpeed(0);

    // Print final position
    Serial.print("Final position: ");
    Serial.print(currentPositionMM);

    RAIL_LENGTH = abs(currentPositionMM);
    CENTER_POSITION = RAIL_LENGTH / 2.0f;
  }
}

bool runMoveToPosition(float position){

  float distance = abs(abs(currentPositionMM) - position);
  float margin = 0.1f;

  // Print position every 100ms
  // static unsigned long lastPrint = 0;
  // if (millis() - lastPrint > 100) {
  //   lastPrint = millis();
  //   Serial.print("Position: ");
  //   Serial.print(abs(currentPositionMM));
  //   Serial.println(" mm");
  //   Serial.print("Desired Position: ");
  //   Serial.print(position);
  //   Serial.println(" mm");
  //   Serial.println(distance);
  // }

  // Calculate direction
  if(distance > margin){
    int direction = sign(position - abs(currentPositionMM));

    int previousDirection = (carriageMotor.speed() / -1.0f) / carriageMotor.maxSpeed();
    if(previousDirection != direction){
      carriageMotor.setSpeed(carriageMotor.maxSpeed() * direction * -1.0f);
    }
  }

  // Run carriage at constant speed
  if (carriageMotor.runSpeed()) {
    stepsMoved++;
    // Update position based on direction
    currentPositionMM += (carriageMotor.speed() > 0) ? 
                         (1.0 / STEPS_PER_MM) : 
                         (-1.0 / STEPS_PER_MM);

    if (distance <= margin) {
      // Stop carriage motor completely
      carriageMotor.setSpeed(0);
      carriageMotor.stop();
      return true;
    }
  }

  if (distance <= margin) {
    // Stop carriage motor completely
    carriageMotor.setSpeed(0);
    carriageMotor.stop();
    return true;
  }

  return false;
}

bool withinRange(float value, float target, float margin) {
    return (value >= (target - margin)) && (value <= (target + margin));
}

float sign(float value) {
  return float((value>0)-(value<0));
}