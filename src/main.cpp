#include <Arduino.h>
#include <YA_FSM.h>

const byte M_POWER = 5;
const byte M_DIR = 4;
const byte V_FILL = 3;
const byte V_DRAIN = 2;
const byte BTN_CALL = 6;

#define CLOCKWISE HIGH
#define ANTICLOCKWISE LOW
#define RELAY_ON LOW
#define RELAY_OFF HIGH

YA_FSM fsm;

// State 
enum State {IDLE, FILL, WASH, DRAIN, SPIN};
const char* stateName[] = { "IDLE", "FILL", "WASH", "DRAIN", "SPIN" };
enum Input {StartIdle, StartFill, StartDrain, StartWash, StartSpin};
Input input;

#define FILL_TIME  3000
#define DRAIN_TIME 5000
#define WASH_TIME 60000
#define SPIN_TIME 5000

int motorDir = CLOCKWISE;
long dirPreviousMillis = 0;
long dirInterval = 3000;

int motorPower = RELAY_OFF;
long powerPreviousMillis = 0;
long powerInterval = dirInterval - 1000;

long everySecondPreviousMillis = 0;

/////////// STATE MACHINE FUNCTIONS //////////////////

void onEnteringIdle(){
	Serial.println(F("Idling..."));
}

void onEnteringFill(){
	Serial.println(F("Fill ON"));
}

void onEnteringDrain(){
	Serial.println(F("Drain ON"));
}

void onEnteringWash(){
	Serial.println(F("Wash ON"));
}

void onEnteringSpin(){
	Serial.println(F("Spin ON"));
}

void onLeavingIdle(){
	Serial.println(F("Idle out \n"));
}

void onLeavingFill(){
	Serial.println(F("Fill OFF\n"));
}

void onLeavingDrain(){
	Serial.println(F("Drain OFF\n"));
}

void onLeavingWash(){
	Serial.println(F("Wash OFF\n"));
}

void onLeavingSpin(){
	Serial.println(F("Spin OFF\n"));
}

bool checkButton(){
	static bool oldButton;
	bool but = !digitalRead(BTN_CALL);
	delay(40);	// simple debounce button
	if( but != oldButton){
		oldButton = but;
		return true && !oldButton;
	}
	return false;
}

void onStateIdle() {
	// turn off all
	digitalWrite(M_POWER, RELAY_OFF);
	digitalWrite(M_DIR, CLOCKWISE);
	digitalWrite(V_FILL, RELAY_OFF);
	digitalWrite(V_DRAIN, RELAY_OFF);
}

void onStateFill() {
	digitalWrite(V_FILL, RELAY_ON);
}

void onStateWash() {
	digitalWrite(V_FILL, RELAY_OFF);
	//digitalWrite(M_POWER, RELAY_ON);
}

void onStateDrain() {
	digitalWrite(M_POWER, RELAY_OFF);
	digitalWrite(M_DIR, CLOCKWISE);
	digitalWrite(V_DRAIN, RELAY_ON);
}

void onStateSpin() {
	digitalWrite(M_POWER, RELAY_OFF);
	digitalWrite(M_DIR, CLOCKWISE);
	digitalWrite(V_DRAIN, RELAY_ON);
	digitalWrite(M_POWER, RELAY_ON);
}

void setupStateMachine() {
	fsm.AddState(stateName[IDLE], 0, onEnteringIdle, onStateIdle, onLeavingIdle);
	fsm.AddState(stateName[FILL], FILL_TIME, onEnteringFill, onStateFill, onLeavingFill);
	fsm.AddState(stateName[WASH], WASH_TIME, onEnteringWash, onStateWash, onLeavingWash);
	fsm.AddState(stateName[DRAIN], DRAIN_TIME, onEnteringDrain, onStateDrain, onLeavingDrain);
	fsm.AddState(stateName[SPIN], SPIN_TIME, onEnteringSpin, onStateSpin, onLeavingSpin);

	fsm.AddTransition(IDLE, FILL, checkButton);
	fsm.AddTransition(FILL, WASH, [](){return input == Input::StartWash;});
	fsm.AddTransition(WASH, DRAIN, [](){return input == Input::StartDrain;});
	fsm.AddTransition(DRAIN, SPIN, [](){return input == Input::StartSpin;});
	fsm.AddTransition(SPIN, IDLE, [](){return input == Input::StartIdle;});
}

void setup() {
	pinMode(M_POWER, OUTPUT);
	pinMode(M_DIR, OUTPUT);
	pinMode(V_FILL, OUTPUT);
	pinMode(V_DRAIN, OUTPUT);
	pinMode(BTN_CALL, INPUT_PULLUP);

	digitalWrite(M_POWER, RELAY_OFF);
	digitalWrite(M_DIR, CLOCKWISE);
	digitalWrite(V_FILL, RELAY_OFF);
	digitalWrite(V_DRAIN, RELAY_OFF);

	Serial.begin(115200);
	Serial.println(F("Starting State Machine...\n"));
	setupStateMachine();

	// Initial state
	Serial.print(F("Active state: "));
	Serial.println(stateName[fsm.GetState()]);
}

void loop() {
	unsigned long currentMillis = millis();

  	// Update State Machine	(true is state changed)
	if(fsm.Update()){
		Serial.print(F("Active state: "));
		Serial.println(stateName[fsm.GetState()]);
	}

	// check if back to idle
	if(fsm.GetState() == FILL) {
		if(millis() - fsm.GetEnteringTime(FILL) > FILL_TIME){
			input = Input::StartWash;	
		}	
	}
	
	if(fsm.GetState() == WASH) {
		if(millis() - fsm.GetEnteringTime(WASH) > WASH_TIME){
			input = Input::StartDrain;
		}

		// motor dir cycle stuff
		if(currentMillis - everySecondPreviousMillis > 1000L) {
			everySecondPreviousMillis = currentMillis;
			if (motorPower != digitalRead(M_POWER)) {
				digitalWrite(M_POWER, motorPower);
			}
			if (motorPower == RELAY_OFF) {
				digitalWrite(M_DIR, motorDir);
				motorPower = RELAY_ON;
			}
		}
		if(currentMillis - dirPreviousMillis > dirInterval) {
			dirPreviousMillis = currentMillis;
			if (motorDir == CLOCKWISE) {
				motorDir = ANTICLOCKWISE;
			} else {
				motorDir = CLOCKWISE;
			}
			motorPower = RELAY_OFF;
		}
	}

	if(fsm.GetState() == DRAIN) {
		if(millis() - fsm.GetEnteringTime(DRAIN) > DRAIN_TIME){
			input = Input::StartSpin;
		}	
	}

	if(fsm.GetState() == SPIN) {
		if(millis() - fsm.GetEnteringTime(SPIN) > SPIN_TIME){
			input = Input::StartIdle;	
		}	
	}
}