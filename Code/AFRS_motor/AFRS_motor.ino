#include <AccelStepper.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// === Pin Definitions ===
#define M1_STEP_PIN 0    // Reel motor step
#define M1_DIR_PIN  1    // Reel motor direction

#define M2_STEP_PIN 2    // Carriage motor step
#define M2_DIR_PIN  3    // Carriage motor direction

#define START_SW_PIN 6    // Home limit switch
#define END_SW_PIN  5    // End limit switch

// Wi-Fi credentials
const char* ssid = "";
const char* password = "";

WiFiServer motorServer(3333);  // Open port 3333
WiFiClient activeClient;       // Store the current client

String recvBuffer = "";

AccelStepper reelMotor(AccelStepper::DRIVER, M1_STEP_PIN, M1_DIR_PIN);
AccelStepper carriageMotor(AccelStepper::DRIVER, M2_STEP_PIN, M2_DIR_PIN);

// === Motor Configuration ===
const int MOTOR_STEPS = 200;                    // 200 steps/rev for NEMA 17
const int MICROSTEPPING = 8;                    // Set on driver board (MS1/MS2 jumpers)
const float DISTANCE_PER_REVOLUTION = 2.0;      // mm per revolution (best to do a revolution by hand and measure distance and input it here)
const float STEPS_PER_MM = (MOTOR_STEPS * MICROSTEPPING) / DISTANCE_PER_REVOLUTION;

// === Speed Settings ===
const float REEL_RPM = 40;    // Reel motor speed
float CARRIAGE_RPM = 100;     // Carriage motor speed (will be dynamically set)
#define FORWARD -1            // In my setup the carriage is moving forward is in negative speed, meaning away from the motor, that why we need to invert the movement in tracking

// System state management
enum SystemState {
  IDLE,
  SYSTEM_CALIBRATION,
  REEL_CALIBRATION,
  REELING
};
SystemState currentSystemState = IDLE;

enum SystemCalibrationState {
  FIND_START,
  FIND_END
};
SystemCalibrationState systemCalibrationState = FIND_START;

// Position tracking
float railLength = 0;
float currentPositionMM = 0;
float targetPosition = 0;
bool isMovingToEnd = false;

// Gap values
float reelGapStartPosition = -1;
float reelGapEndPosition = -1;

bool starterSet = false;  // Is start gap position set?
bool endSet = false;      // Is start gap position set?

void setup() {
  Serial.begin(115200);

  Serial.println("Motor ESP Connecting");
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nMotor ESP connected. IP: " + WiFi.localIP().toString());

  // FIXME : Perhaps use UDP communication instead maybe it will not block the ESP32C3 as much
  motorServer.begin();  // Start TCP server

  pinMode(START_SW_PIN, INPUT_PULLUP);
  pinMode(END_SW_PIN, INPUT_PULLUP);

  // Configure reel motor
  reelMotor.setMaxSpeed((REEL_RPM * MOTOR_STEPS * MICROSTEPPING) / 60.0);
  reelMotor.setSpeed(-reelMotor.maxSpeed());  // To reel the line in toward the fishing reel, i need the fishing reel to move anti-clockwise
  reelMotor.setMinPulseWidth(50);

  // Configure carriage motor
  carriageMotor.setMaxSpeed((CARRIAGE_RPM * MOTOR_STEPS * MICROSTEPPING) / 60.0);
  carriageMotor.setSpeed(carriageMotor.maxSpeed());
  carriageMotor.setMinPulseWidth(50);
}

void loop() {
  // Accept new clients
  if (!activeClient || !activeClient.connected()) {
    activeClient = motorServer.available();
  }

  if (activeClient && activeClient.connected() && activeClient.available()) {
    while (activeClient.available()) {
      char c = activeClient.read();

      if (c == '\n') {
        recvBuffer.trim();  // Remove trailing \r or whitespace
        if (recvBuffer.length() > 0) {
          handleCommand(recvBuffer, activeClient);
        }
        recvBuffer = "";  // Reset for next command
      } else {
        recvBuffer += c;
        // Optional: protect against overflow
        if (recvBuffer.length() > 200) recvBuffer = "";  // Reset if too long
      }
    }
  }

  // Main state machine
  switch (currentSystemState) {
    case IDLE: break;
    case REELING: runReeling(); break;
    case REEL_CALIBRATION: runCalibration(); break;
    case SYSTEM_CALIBRATION:
      if (systemCalibrationState == FIND_START) runFindStart();
      else if (systemCalibrationState == FIND_END) runFindEnd();
      break;
  }
}

void runFindStart() {
  // Force the carriage motor to find the start switch by moving it positive direction, meaning toward the motor
  if(carriageMotor.speed() <= 0){ 
    carriageMotor.setSpeed(carriageMotor.maxSpeed());
  }

  carriageMotor.runSpeed();

  // 
  if (digitalRead(START_SW_PIN) == LOW) {
    carriageMotor.stop();
    carriageMotor.setSpeed(0);
    currentPositionMM = 0;  // Start from zero
    carriageMotor.setCurrentPosition(0);
    carriageMotor.setSpeed(carriageMotor.maxSpeed());  // Always move forward from here
    systemCalibrationState = FIND_END;
  }
}

void runFindEnd() {

  // Force the carriage motor to find the end switch by moving it negative direction, meaning away from the motor
  if(carriageMotor.speed() >= 0){
    carriageMotor.setSpeed(carriageMotor.maxSpeed() * FORWARD);
  }

  if (carriageMotor.runSpeed()) {
    currentPositionMM += (1.0 / STEPS_PER_MM); // Calculate the new position when the motor actually does steps
  }

  if (digitalRead(END_SW_PIN) == LOW) {
    carriageMotor.stop();
    carriageMotor.setSpeed(0);

    railLength = currentPositionMM; // Assign the maximum length the motor can travel
    targetPosition = railLength;

    currentSystemState = IDLE;
  }
}

void runReeling() {
  
  if(reelGapStartPosition < 0 || reelGapEndPosition < 0){
    // TODO : Maybe throw an error for the user
    stopMotors();
    return;
  }

  if (isWithinRange(currentPositionMM, reelGapStartPosition, reelGapEndPosition)) {

    if(reelMotor.speed() == 0){
      reelMotor.setSpeed(-reelMotor.maxSpeed());
    }
    reelMotor.runSpeed();
  }

  // === Oscillate effect === 
  if (!isMovingToEnd) {
    if (runMoveToPosition(reelGapStartPosition)) {
      isMovingToEnd = true;
    }
  } else {
    if (runMoveToPosition(reelGapEndPosition)) {
      isMovingToEnd = false;
    }
  }
}

void runCalibration() {
  runMoveToPosition(targetPosition);
}

bool runMoveToPosition(float position) {

  // If it want's to move out of bounds, stop the motors, maybe it will damage the setup
  if(position < 0.0f || position > railLength){
    stopMotors();
    return false;
  }

  float distance = abs(currentPositionMM - position);
  float margin = 0.01f;

  if (distance <= margin) {
    carriageMotor.setSpeed(0);
    carriageMotor.stop();
    return true;
  }

  int direction = sign(position - currentPositionMM);
  int previousDirection = (carriageMotor.speed() / FORWARD) / carriageMotor.maxSpeed();

  if (previousDirection != direction) {
    carriageMotor.setSpeed(carriageMotor.maxSpeed() * direction * FORWARD);
  }

  if (carriageMotor.runSpeed()) {
    currentPositionMM += (carriageMotor.speed() > 0) ? (-1.0 / STEPS_PER_MM) : (1.0 / STEPS_PER_MM);

    if (distance <= margin) {
      carriageMotor.setSpeed(0);
      carriageMotor.stop();
      return true;
    }
  }

  return false;
}

void stopMotors(){
    currentSystemState = IDLE;
    carriageMotor.stop();
    carriageMotor.setSpeed(0);
    reelMotor.stop();
    reelMotor.setSpeed(0);
}

float calculateCarriageRPM(float reelRPM, float gapLengthMM) {
  float timeForReelRevs = 4.0f * 60.0f / reelRPM;
  float mmPerSec = gapLengthMM / timeForReelRevs;
  float carriageRevsPerSec = mmPerSec / DISTANCE_PER_REVOLUTION;
  return carriageRevsPerSec * 60.0f;
}

bool isWithinRange(float currentPosition, float start, float end) {
  return currentPosition >= start && currentPosition <= end;
}

float sign(float value) {
  return float((value > 0) - (value < 0));
}

String stateToString(SystemState systemState) {
  switch(systemState) {
    case IDLE: return "IDLE";
    case REELING: return "REELING";
    case REEL_CALIBRATION: return "REEL_CALIBRATION";
    case SYSTEM_CALIBRATION: return "SYSTEM_CALIBRATION";
    default: return "UNKNOWN";
  }
}

void handleCommand(const String& cmd, WiFiClient& client) {
  if (cmd.startsWith("starter-position-input")) {
    float value;
    int scanned = sscanf(cmd.c_str(), "starter-position-input %f", &value);
    if (scanned == 1) {
      reelGapStartPosition = value;
      starterSet = true;

      if (starterSet && endSet) {
        float gap = reelGapEndPosition - reelGapStartPosition;
        float newCarriageRPM = calculateCarriageRPM(REEL_RPM, gap);
        CARRIAGE_RPM = newCarriageRPM;
        float maxSpeed = (newCarriageRPM * MOTOR_STEPS * MICROSTEPPING) / 60.0;
        carriageMotor.setMaxSpeed(maxSpeed);
      }

      client.println("OK");
    } else {
      client.println("Invalid starter-position-input command");
    }

  } else if (cmd.startsWith("end-position-input")) {
    float value;
    int scanned = sscanf(cmd.c_str(), "end-position-input %f", &value);
    if (scanned == 1) {
      reelGapEndPosition = value;
      endSet = true;

      if (starterSet && endSet) {
        float gap = reelGapEndPosition - reelGapStartPosition;
        float newCarriageRPM = calculateCarriageRPM(REEL_RPM, gap);
        CARRIAGE_RPM = newCarriageRPM;
        float maxSpeed = (newCarriageRPM * MOTOR_STEPS * MICROSTEPPING) / 60.0;
        carriageMotor.setMaxSpeed(maxSpeed);
      }

      client.println("OK");
    } else {
      client.println("Invalid end-position-input command");
    }
  } else if (cmd == "status") {
    StaticJsonDocument<200> doc;
    doc["position"] = currentPositionMM;
    doc["state"] = stateToString(currentSystemState);
    doc["railLength"] = railLength;
    doc["starterSet"] = starterSet;
    doc["endSet"] = endSet;
    String json;
    serializeJson(doc, json);
    client.println(json);

  } else if (cmd == "calibrate") {
    currentSystemState = SYSTEM_CALIBRATION;
    systemCalibrationState = FIND_START;
    client.println("System calibration started");

  } else if (cmd == "reel_cal") {
    if(currentSystemState != REEL_CALIBRATION){
      currentSystemState = REEL_CALIBRATION;
      
    } else {
      currentSystemState = IDLE;
    }

    if(starterSet && endSet){
      targetPosition = (reelGapEndPosition - reelGapStartPosition) / 2.0f;
    }else {
      targetPosition = 0;
    }

    client.println("Reel calibration mode");

  } else if (cmd == "start") {
    if (starterSet && endSet) {
      currentSystemState = REELING;
      client.println("Started reeling");
    } else {
      client.println("Error: Gap not set");
    }

  } else if (cmd == "stop") {
    stopMotors();
    client.println("Stopped");
  } else if (cmd.startsWith("move")) {
    float value;
    int scanned = sscanf(cmd.c_str(), "move %f", &value);
    if (scanned == 1) {
      targetPosition = value;
      currentSystemState = REEL_CALIBRATION;  // Use existing logic to move to target
      client.println("OK");
    } else {
      client.println("Invalid end-position-input command");
    }

  }else {
    client.println("Unknown command");
  }
}

