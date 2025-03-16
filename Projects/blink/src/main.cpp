#include <Arduino.h>
#include <TaskScheduler.h>
#include <PinChangeInterrupt.h>

// 가변저항 핀 설정
const int potpin = A1;

// LED 핀 번호 설정
const int red = 3;
const int yellow = 5;
const int green = 6; 

// 버튼 핀 번호 설정 (PCINT 사용 가능한 핀)
const int buttonRedMode = 8;     // PCINT0
const int buttonBlinkAll = 9;    // PCINT1
const int buttonToggleCycle = 10; // PCINT3

volatile bool isRedMode = false;
volatile bool isBlinkAll = false;
volatile bool isCycleRunning = true;
volatile bool buttonRedPressed = false;
volatile bool buttonBlinkPressed = false;
volatile bool buttonTogglePressed = false;

//LED 상태
bool redState = false;
bool yellowState = false;
bool greenState = false;
// 밝기 조절
int brightness = map(potpin, 0, 1023, 0, 255);  
// TaskScheduler 인스턴스 생성
Scheduler runner;

// LED 상태 전환 함수 선언
void turnOnGreen();
void turnOnYellowAfterRed();
void turnOnYellowAfterBlink();
void turnOnRed();
void blinkGreen();
void blinkAll();
void handleButtonRedMode();
void handleButtonBlinkAll();
void handleButtonToggleCycle();
void processSerialData();
void sendTrafficLightStatus();

int redTime = 2000, yellowTime = 500, greenTime = 2000;

// Task 생성
Task taskRed(2000, TASK_ONCE, &turnOnRed);
Task taskYellowAfterRed(500, TASK_ONCE, &turnOnYellowAfterRed);
Task taskGreen(2000, TASK_ONCE, &turnOnGreen);
Task taskBlinkGreen(333, 6, &blinkGreen);
Task taskYellowAfterBlink(500, TASK_ONCE, &turnOnYellowAfterBlink);
Task taskBlinkAll(1000, TASK_FOREVER, &blinkAll);
Task taskSendStatus(1000, TASK_FOREVER, &sendTrafficLightStatus);

void setup() {
    Serial.begin(9600); // 시리얼 모니터 시작
    pinMode(green, OUTPUT);
    pinMode(yellow, OUTPUT);
    pinMode(red, OUTPUT);

    pinMode(buttonRedMode, INPUT_PULLUP);
    pinMode(buttonBlinkAll, INPUT_PULLUP);
    pinMode(buttonToggleCycle, INPUT_PULLUP);

    // Task 등록
    runner.addTask(taskRed);
    runner.addTask(taskYellowAfterRed);
    runner.addTask(taskGreen);
    runner.addTask(taskBlinkGreen);
    runner.addTask(taskYellowAfterBlink);
    runner.addTask(taskBlinkAll);
    runner.addTask(taskSendStatus);

    // ✅ PCINT 인터럽트 등록
    attachPinChangeInterrupt(digitalPinToPCINT(buttonRedMode), handleButtonRedMode, FALLING);
    attachPinChangeInterrupt(digitalPinToPCINT(buttonBlinkAll), handleButtonBlinkAll, FALLING);
    attachPinChangeInterrupt(digitalPinToPCINT(buttonToggleCycle), handleButtonToggleCycle, FALLING);

    taskRed.enable();
    taskSendStatus.enable();
}

// 🔴 빨간불 켜기 (2초 후 노란불 전환)
void turnOnRed() {
    if (isRedMode || isBlinkAll || !isCycleRunning) return;
    analogWrite(green, 0);
    analogWrite(yellow, 0);
    analogWrite(red, 255);
    redState = true; yellowState = false; greenState = false;
    taskYellowAfterRed.restartDelayed(redTime); // 🔥 2초 후 노란불로 전환
}

// 🟡 빨간불 후 노란불 (0.5초 후 초록불로 전환)
void turnOnYellowAfterRed() {
    if (isRedMode || isBlinkAll || !isCycleRunning) return;
    analogWrite(green, 0);
    analogWrite(yellow, 255);
    analogWrite(red, 0);
    yellowState = true; redState = false; greenState = false;
    taskGreen.restartDelayed(yellowTime); // 🔥 0.5초 후 초록불로 전환
}

// 🟢 초록불 켜기 (2초 후 깜빡이기 시작)
void turnOnGreen() {
    if (isRedMode || isBlinkAll || !isCycleRunning) return;
    analogWrite(green, 255);
    analogWrite(yellow, 0);
    analogWrite(red, 0);
    greenState = true; redState = false; yellowState = false;
    taskBlinkGreen.restartDelayed(greenTime); // 🔥 2초 후 초록불 깜빡이기 시작
}

// 🟢 초록불 깜빡이기 (총 3회 깜빡임)
void blinkGreen() {
    if (isRedMode || isBlinkAll || !isCycleRunning) return;
    static bool state = false;
    analogWrite(green, state ? 255 : 0);
    state = !state;
    redState = false; yellowState = false; greenState = state;
    if (taskBlinkGreen.isLastIteration()) {
        taskYellowAfterBlink.restartDelayed(500); // 🔥 깜빡임 종료 후 노란불
    }
}

// 🟡 초록불 깜빡임 후 노란불 (0.5초 후 빨간불)
void turnOnYellowAfterBlink() {
    if (isRedMode || isBlinkAll || !isCycleRunning) return;
    analogWrite(green, 0);
    analogWrite(yellow, 255);
    analogWrite(red, 0);
    yellowState = true; redState = false; greenState = false;
    taskRed.restartDelayed(yellowTime); // 🔥 0.5초 후 빨간불 시작
}


// 🔄 모든 LED 깜빡이기
void blinkAll() {
    static bool state = false;
    state = !state;

    int potValue = analogRead(A1);  
    int brightness = map(potValue, 0, 1023, 0, 255);  // 🔥 밝기 조절 반영

    analogWrite(green, state ? brightness : 0);
    analogWrite(yellow, state ? brightness : 0);
    analogWrite(red, state ? brightness : 0);
    redState = state; yellowState = state; greenState = state;
}



// 🛑 PCINT 인터럽트 핸들러 (버튼 1: 빨간불 모드)
void handleButtonRedMode() {
    Serial.println("Button Red Mode Pressed");
    buttonRedPressed = true;
}

// 🛑 PCINT 인터럽트 핸들러 (버튼 2: 모든 LED 깜빡이기)
void handleButtonBlinkAll() {
    Serial.println("Button Blink All Pressed");
    buttonBlinkPressed = true;
}

// 🛑 PCINT 인터럽트 핸들러 (버튼 3: 기본 기능 ON/OFF)
void handleButtonToggleCycle() {
    Serial.println("Button Toggle Cycle Pressed");
    buttonTogglePressed = true;
}

// ✅ 모든 Task를 중지하는 함수
void stopAllTasks() {
    runner.disableAll();
    taskRed.disable();
    taskYellowAfterRed.disable();
    taskGreen.disable();
    taskBlinkGreen.disable();
    taskYellowAfterBlink.disable();
    taskBlinkAll.disable();

    // LED 초기화
    digitalWrite(green, LOW);
    digitalWrite(yellow, LOW);
    digitalWrite(red, LOW);
}

// 🎛 버튼 처리 (인터럽트 후 실행)
void processButtons() {
    noInterrupts();
    bool redPressed = buttonRedPressed;
    bool blinkPressed = buttonBlinkPressed;
    bool togglePressed = buttonTogglePressed;
    buttonRedPressed = false;
    buttonBlinkPressed = false;
    buttonTogglePressed = false;
    interrupts();

    // 🔴 버튼 1: 빨간불 모드 (토글)
    if (redPressed) {
        isRedMode = !isRedMode;
        isBlinkAll = false;
        isCycleRunning = !isRedMode;

        stopAllTasks();

        if (isRedMode) {
            Serial.println("Emergency Mode: Red LED ON");
            digitalWrite(red, HIGH);
        } else {
            Serial.println("Exiting Emergency Mode, restarting cycle...");
            taskRed.restartDelayed(0);
        }
    }

    // 🔄 버튼 2: 모든 LED 깜빡이기 (토글)

    if (blinkPressed) {
    isBlinkAll = !isBlinkAll;
    isRedMode = false;
    isCycleRunning = !isBlinkAll;

    stopAllTasks();

    if (isBlinkAll) {
        Serial.println("Blinking Mode: All LEDs blinking...");
        taskBlinkAll.enable();
    } else {
        Serial.println("Stopping blinking, resuming normal cycle...");
        taskBlinkAll.disable();

        // 🔥 LED 완전 OFF (analogWrite 사용)
        analogWrite(red, 0);
        analogWrite(yellow, 0);
        analogWrite(green, 0);

        taskRed.restartDelayed(0);
    }
}



    // 🔘 버튼 3: 기본 기능 ON/OFF
    if (togglePressed) {
        isCycleRunning = !isCycleRunning;

        stopAllTasks();

        if (isCycleRunning) {
            Serial.println("Cycle ON: Restarting...");
            taskRed.restartDelayed(0);
        } else {
            Serial.println("Cycle OFF: Stopping...");
        }
    }

    // 📩 버튼 상태 전송
    sendTrafficLightStatus();
}

void sendTrafficLightStatus() {
    int mode = isRedMode ? 1 : (isBlinkAll ? 2 : (isCycleRunning ? 0 : 3));
    brightness = map(analogRead(potpin), 0, 1023, 0, 255);

    Serial.print(mode);
    Serial.print(",");
    Serial.print(isBlinkAll ? 1 : 0);
    Serial.print(",");
    Serial.print(isCycleRunning ? 1 : 0);
    Serial.print(",");
    Serial.print(redState ? 1 : 0);
    Serial.print(",");
    Serial.print(yellowState ? 1 : 0);
    Serial.print(",");
    Serial.print(greenState ? 1 : 0);
    Serial.print(",");
    Serial.println(brightness);
}

void loop() {
    processButtons();
    runner.execute();

    int potValue = analogRead(A1);  
    int brightness = map(potValue, 0, 1023, 0, 255);  


    if (isBlinkAll) return; // BlinkAll 모드에서는 아래 코드 실행 X

    if (digitalRead(red) == HIGH) {
        analogWrite(red, brightness);
        redState = true;
    } else {
        analogWrite(red, 0);
        redState = false;
    }

    if (digitalRead(yellow) == HIGH) {
        analogWrite(yellow, brightness);
        yellowState = true;
    } else {
        analogWrite(yellow, 0);
        yellowState = false;
    }

    if (digitalRead(green) == HIGH) {
        analogWrite(green, brightness);
        greenState = true;
    } else {
        analogWrite(green, 0);
        greenState = false;
    }
    sendTrafficLightStatus();
    delay(100); // 시리얼 전송 주기
    processSerialData();
}


// 📩 시리얼 데이터를 받아 신호등 시간 업데이트
void processSerialData() {
    if (Serial.available()) {
        String data = Serial.readStringUntil('\n'); // 개행 문자까지 읽기
        data.trim(); // 앞뒤 공백 제거

        Serial.print("Received data: "); // 디버깅 로그
        Serial.println(data);

        int newRed, newYellow, newGreen;
        if (sscanf(data.c_str(), "%d,%d,%d", &newRed, &newYellow, &newGreen) == 3) {
            redTime = newRed;
            yellowTime = newYellow;
            greenTime = newGreen;

            Serial.println("updated!");
            Serial.print("Red Time: "); Serial.println(redTime);
            Serial.print("Yellow Time: "); Serial.println(yellowTime);
            Serial.print("Green Time: "); Serial.println(greenTime);

            // 🔥 현재 Task의 Interval 변경
            taskRed.setInterval(newRed);
            taskYellowAfterRed.setInterval(newYellow);
            taskGreen.setInterval(newGreen);
            if (taskRed.isEnabled()) {
                taskRed.restartDelayed(0);
                } 
            if (taskYellowAfterRed.isEnabled()) {
                taskYellowAfterRed.restartDelayed(0);
                } 
            if (taskGreen.isEnabled()) {
                taskGreen.restartDelayed(0);
                }
            
            } else {
            Serial.println("데이터 파싱 실패!");
        }
    }
}

