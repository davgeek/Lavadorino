#include <Arduino.h>
#include <YA_FSM.h>
#include "Blinker.h"
#include <JC_Button.h>

const byte M_LEFT = 5;
const byte M_RIGHT = 4;
const byte V_FILL = 3;
const byte V_DRAIN = 2;
const byte BTN_1 = 6;
const byte LED_1 = 7;
const byte LED_2 = 8;
const byte LED_3 = 9;

#define CLOCKWISE 1
#define COUNTERCLOCKWISE 0

#define TRIAC_ON HIGH
#define TRIAC_OFF LOW
#define RELAY_ON LOW
#define RELAY_OFF HIGH

#define BLINK1_TIME 1000
#define BLINK2_TIME 250

#define MAX_STEPS 2
#define MIN_STEPS 0

YA_FSM fsm;

Blinker led1(LED_1, BLINK1_TIME);
Blinker led2(LED_2, BLINK1_TIME);
Blinker led3(LED_3, BLINK1_TIME);

Button btn(BTN_1);

bool readyToStartState = false;
int lastPositionSelected = 0;

// State 
enum State {IDLE, FILL, WASH, DRAIN, SPIN, PAUSE};
const char* stateName[] = { "IDLE", "FILL", "WASH", "DRAIN", "SPIN", "PAUSE" };
enum Input {
	StartIdle,
	ManualFill,
	ManualWash,
	ManualDrain,
	StartFill,
	StartDrain,
	StartWash,
	StartSpin,
	StartFillPause,
	StartWashPause,
	StartDrainPause,
	StartSpinPause
};

Input input;
State lastState;

#define FILL_TIME  300000L
#define DRAIN_TIME 180000L
#define WASH_TIME 420000L
#define SPIN_TIME 230000L

long cyclePreviousMillis = 0;
long lastStatePauseMillis = 0;

int motorDir = CLOCKWISE;
int motorCounter = 0;
int restCounter = 0;
bool motorState = false;

#define MOTOR_ON_DURATION 3
#define MOTOR_REST_DURATION 1

void turnAllOff() {
	digitalWrite(M_LEFT, TRIAC_OFF);
	digitalWrite(M_RIGHT, TRIAC_OFF);
	digitalWrite(V_FILL, TRIAC_OFF);
	digitalWrite(V_DRAIN, TRIAC_OFF);
}

/////////// STATE MACHINE FUNCTIONS //////////////////
void onEnteringIdle(){
	Serial.println(F("Entering IDLE..."));
	lastPositionSelected = 0;
	motorCounter = 0;
	motorState = false;
	restCounter = 0;
}

void onEnteringFill(){
	Serial.println(F("Entering Fill"));
	readyToStartState = false;
}

void onEnteringDrain(){
	Serial.println(F("Entering Drain"));
	readyToStartState = false;
}

void onEnteringWash(){
	Serial.println(F("Entering Wash"));
	readyToStartState = false;
}

void onEnteringSpin(){
	Serial.println(F("Entering Spin"));
}

void onLeavingIdle(){
	Serial.println(F("Leaving Idle"));
	digitalWrite(LED_1, LOW);
	digitalWrite(LED_2, LOW);
	digitalWrite(LED_3, LOW);
}

void onLeavingFill(){
	Serial.println(F("Leaving Fill"));
	led1.blink(false);
}

void onLeavingDrain(){
	Serial.println(F("Leaving Drain"));
	led3.blink(false);
}

void onLeavingWash(){
	Serial.println(F("Leaving Wash"));
	led2.blink(false);
}

void onLeavingSpin(){
	led3.blink(false);
	Serial.println(F("Leaving Spin"));
}

void onStateIdle() {
	turnAllOff();
}

void onStatePause() {
	turnAllOff();

	switch (lastState) {
		case FILL:
			led1.blink(false);
			led1.setTime(BLINK2_TIME);
			led1.blink(true);
			break;
		case WASH:
			led2.blink(false);
			led2.setTime(BLINK2_TIME);
			led2.blink(true);
			break;
		case DRAIN:
		case SPIN:
			led3.blink(false);
			led3.setTime(BLINK2_TIME);
			led3.blink(true);
			break;
	}
}

void onStateFill() {
	digitalWrite(V_FILL, TRIAC_ON);
	led1.setTime(BLINK1_TIME);
	led1.blink(true);
}

void onStateWash() {
	digitalWrite(V_FILL, TRIAC_OFF);
	led2.setTime(BLINK1_TIME);
	led2.blink(true);
}

void onStateDrain() {
	digitalWrite(M_LEFT, TRIAC_OFF);
	digitalWrite(M_RIGHT, TRIAC_OFF);
	digitalWrite(V_DRAIN, TRIAC_ON);
	led3.setTime(BLINK1_TIME);
	led3.blink(true);
}

void onStateSpin() {
	digitalWrite(M_LEFT, TRIAC_OFF);
	digitalWrite(V_DRAIN, TRIAC_ON);
	digitalWrite(M_RIGHT, TRIAC_ON);
	led3.blink(true);
}

void setupStateMachine() {
	fsm.AddState(stateName[IDLE], onEnteringIdle, onStateIdle, onLeavingIdle);
	fsm.AddState(stateName[FILL], FILL_TIME, onEnteringFill, onStateFill, onLeavingFill);
	fsm.AddState(stateName[WASH], WASH_TIME, onEnteringWash, onStateWash, onLeavingWash);
	fsm.AddState(stateName[DRAIN], DRAIN_TIME, onEnteringDrain, onStateDrain, onLeavingDrain);
	fsm.AddState(stateName[SPIN], SPIN_TIME, onEnteringSpin, onStateSpin, onLeavingSpin);
	fsm.AddState(stateName[PAUSE], nullptr, onStatePause, nullptr);

	fsm.AddTransition(IDLE, FILL, [](){return input == Input::ManualFill && readyToStartState;});
	fsm.AddTransition(IDLE, WASH, [](){return input == Input::ManualWash && readyToStartState;});
	fsm.AddTransition(IDLE, DRAIN, [](){return input == Input::ManualDrain && readyToStartState;});
	fsm.AddTransition(FILL, WASH, [](){return input == Input::StartWash;});
	fsm.AddTransition(WASH, DRAIN, [](){return input == Input::StartDrain;});
	fsm.AddTransition(DRAIN, SPIN, [](){return input == Input::StartSpin;});
	fsm.AddTransition(SPIN, IDLE, [](){return input == Input::StartIdle;});
	// fsm.AddTransition(FILL, PAUSE, [](){return input == Input::StartFillPause;});
	// fsm.AddTransition(WASH, PAUSE, [](){return input == Input::StartWashPause;});
	// fsm.AddTransition(DRAIN, PAUSE, [](){return input == Input::StartDrainPause;});
	// fsm.AddTransition(SPIN, PAUSE, [](){return input == Input::StartSpinPause;});
	// fsm.AddTransition(PAUSE, FILL, [](){return input == Input::StartFill && lastState == FILL;});
	// fsm.AddTransition(PAUSE, WASH, [](){return input == Input::StartWash && lastState == WASH;});
	// fsm.AddTransition(PAUSE, DRAIN, [](){return input == Input::StartDrain && lastState == DRAIN;});
	// fsm.AddTransition(PAUSE, SPIN, [](){return input == Input::StartSpin && lastState == SPIN;});
}

void checkPause(Input input, State state) {
	btn.read();

	if (btn.pressedFor(3000)) {
		Serial.println("PAUSE");
		input = input;
		lastState = state;
		lastStatePauseMillis = millis();
	}
}

void ledControl(bool led1State, bool led2State, bool led3State) {
	digitalWrite(LED_1, led1State);
	digitalWrite(LED_2, led2State);
	digitalWrite(LED_3, led3State);
}

void setup() {
	pinMode(M_LEFT, OUTPUT);
	pinMode(M_RIGHT, OUTPUT);
	pinMode(V_FILL, OUTPUT);
	pinMode(V_DRAIN, OUTPUT);
	pinMode(LED_1, OUTPUT);
	pinMode(LED_2, OUTPUT);
	pinMode(LED_3, OUTPUT);
	pinMode(BTN_1, INPUT_PULLUP);

	digitalWrite(M_LEFT, TRIAC_OFF);
	digitalWrite(M_RIGHT, TRIAC_OFF);
	digitalWrite(V_FILL, TRIAC_OFF);
	digitalWrite(V_DRAIN, TRIAC_OFF);

	btn.begin();

	Serial.begin(115200);
	Serial.println(F("Starting Lavadorino...\n"));
	setupStateMachine();

	// Initial state
	Serial.print(F("Active state: "));
	Serial.println(stateName[fsm.GetState()]);

}

void loop() {
	unsigned long currentMillis = millis();
	
	if(fsm.GetState() == IDLE) {

		btn.read();

		if (btn.wasReleased()) {
			lastPositionSelected++;
		} else if (btn.pressedFor(1500)) {
			readyToStartState = true;
		}

		if (lastPositionSelected > MAX_STEPS) {
			lastPositionSelected = MIN_STEPS;
		} else if (lastPositionSelected < MIN_STEPS) {
			lastPositionSelected = MAX_STEPS;
		}

		switch (lastPositionSelected) {
			case 0:
				ledControl(HIGH, LOW, LOW);
				input = Input::ManualFill;
				break;
			case 1:
				ledControl(LOW, HIGH, LOW);
				input = Input::ManualWash;
				break;
			case 2:
				ledControl(LOW, LOW, HIGH);
				input = Input::ManualDrain;
				break;
		}
	}

	if(fsm.GetState() == PAUSE) {
		btn.read();

		if (btn.pressedFor(3000)) {
			Serial.println("UNPAUSE");
			switch (lastState) {
				case WASH:
					input = Input::StartWash;
					break;
				case FILL:
					input = Input::StartFill;
					break;
				case DRAIN:
					input = Input::StartDrain;
					break;
				case SPIN:
					input = Input::StartSpin;
					break;
			}
		}
	}

	if(fsm.GetState() == FILL) {
		// checkPause(Input::StartFillPause, FILL);
		if(millis() - fsm.GetEnteringTime(FILL) > FILL_TIME){
			input = Input::StartWash;	
		}
	}
	
	if(fsm.GetState() == WASH) {
		// checkPause(Input::StartWashPause, WASH);
		if(millis() - fsm.GetEnteringTime(WASH) > WASH_TIME){
			input = Input::StartDrain;
		}

		// motor dir cycle stuff
		if(currentMillis - cyclePreviousMillis > 1000) {
			cyclePreviousMillis = currentMillis;
			
			if (motorCounter <= MOTOR_ON_DURATION && restCounter == MOTOR_REST_DURATION) {
				digitalWrite(M_LEFT, TRIAC_OFF);
				digitalWrite(M_RIGHT, TRIAC_OFF);
				Serial.println("Encendido");
				if (motorDir == CLOCKWISE) {
					digitalWrite(M_RIGHT, TRIAC_ON);
				} else {
					digitalWrite(M_LEFT, TRIAC_ON);
				}
				motorState = true;
				restCounter = 0;
			}

			if (motorCounter == MOTOR_ON_DURATION) {
				digitalWrite(M_RIGHT, TRIAC_OFF);
				digitalWrite(M_LEFT, TRIAC_OFF);
				Serial.println("Apagado");
				motorState = false;
				if (motorDir == CLOCKWISE) {
					motorDir = COUNTERCLOCKWISE;
					Serial.println("CW");
				} else {
					Serial.println("CCW");
					motorDir = CLOCKWISE;
				}
				
				motorCounter = 0;
			}

			if (!motorState) {
				restCounter++;
			}

			motorCounter++;
		}
	}

	if(fsm.GetState() == DRAIN) {
		// checkPause(Input::StartDrainPause, DRAIN);
		if(millis() - fsm.GetEnteringTime(DRAIN) > DRAIN_TIME){
			input = Input::StartSpin;
		}	
	}

	if(fsm.GetState() == SPIN) {
		// checkPause(Input::StartSpinPause, SPIN);
		if(millis() - fsm.GetEnteringTime(SPIN) > SPIN_TIME){
			input = Input::StartIdle;	
		}	
	}

	// Update State Machine	(true is state changed)
	if(fsm.Update()){
		Serial.print(F("Active state: "));
		Serial.println(stateName[fsm.GetState()]);
	}
}