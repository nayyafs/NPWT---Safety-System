#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <PID_v1_bc.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// BMP280 setup
Adafruit_BMP280 bmp;
const int pumpPin = 26;
double setpoint = 800.0;

double input, output;
double kp = 8, ki = 0.5, kd = 1.0;
PID myPID(&input, &output, &setpoint, kp, ki, kd, REVERSE);

const int minOutput = 60;

// PWM settings for ESP32
const int pwmChannel = 0;
const int pwmFreq = 5000;
const int pwmResolution = 8;

// TFT and Touchscreen setup
TFT_eSPI tft = TFT_eSPI();

// Button pins
#define BUTTON_1 34
#define BUTTON_2 35
#define LED_RED 17
#define LED_GREEN 16
#define buzzer 5

// Touchscreen pins
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// Button states
int lastState_1 = LOW;
int currentState_1;
int lastState_2 = LOW;
int currentState_2;
bool pumpState = false;
bool greenState = false;
bool lockState = false;
bool isBuzzing = false;
unsigned long previousMillis = 0;
const long interval = 500;

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// Screen dimensions
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

// Button dimensions and positions
#define BUTTON_WIDTH 60
#define BUTTON_HEIGHT 40
#define BUTTON_SPACING 20

// Define colors
#define BACKGROUND TFT_WHITE
#define BUTTON_COLOR TFT_BLUE
#define TEXT_COLOR TFT_BLACK
#define VALUE_COLOR TFT_NAVY
#define TIMER_COLOR TFT_RED

// Pressure control
int targetPressure = 125;
const int pressureStep = 5;
const int minPressure = 50;
const int maxPressure = 200;

// Timer variables
unsigned long timerDuration = 0;
unsigned long startTime = 0;
bool timerRunning = false;
int timerValue = 30; // Default timer value in seconds

// Button coordinates
struct Button {
    int x;
    int y;
    int width;
    int height;
    const char* text;
};

// Define buttons
#define BUTTON_SPACING 10
#define ROW_Y 180

Button minusBtn = {50, ROW_Y, BUTTON_WIDTH, BUTTON_HEIGHT, "-"};
Button plusBtn = {50 + BUTTON_WIDTH + BUTTON_SPACING, ROW_Y, BUTTON_WIDTH, BUTTON_HEIGHT, "+"};
Button timerMinusBtn = {50 + 2 * (BUTTON_WIDTH + BUTTON_SPACING), ROW_Y, BUTTON_WIDTH, BUTTON_HEIGHT, "-"};
Button timerPlusBtn = {50 + 3 * (BUTTON_WIDTH + BUTTON_SPACING), ROW_Y, BUTTON_WIDTH, BUTTON_HEIGHT, "+"};
Button startBtn = {50 + 4 * (BUTTON_WIDTH + BUTTON_SPACING), ROW_Y, BUTTON_WIDTH * 1.5, BUTTON_HEIGHT, "Start"};

// Scaling factors
#define X_SCALING 1.05
#define Y_SCALING 0.65
#define X_OFFSET 25
#define Y_OFFSET 3

void drawButton(Button btn, uint16_t color) {
    tft.fillRoundRect(btn.x, btn.y, btn.width, btn.height, 5, color);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.drawCentreString(btn.text, btn.x + btn.width/2, btn.y + btn.height/4, 1);
}

void drawInterface() {
    tft.fillScreen(BACKGROUND);
    
    // Draw title
    tft.setTextColor(TEXT_COLOR);
    tft.setTextSize(2);
    tft.drawCentreString("NPWT Control", SCREEN_WIDTH/2, 20, 1);
    
    // Draw pressure section title
    tft.drawString("Pressure:", 50, 80, 1);
    
    // Draw pressure value
    tft.setTextColor(VALUE_COLOR);
    tft.setTextSize(3);
    String pressureText = String(targetPressure) + " mmHg";
    tft.drawString(pressureText, 180, 80, 1);
    
    // Draw timer section title
    tft.setTextColor(TEXT_COLOR);
    tft.setTextSize(2);
    tft.drawString("Timer:", 50, 140, 1);
    
    // Draw timer value
    updateTimerDisplay();
    
    // Draw all buttons
    drawButton(minusBtn, BUTTON_COLOR);
    drawButton(plusBtn, BUTTON_COLOR);
    drawButton(timerMinusBtn, BUTTON_COLOR);
    drawButton(timerPlusBtn, BUTTON_COLOR);
    drawButton(startBtn, timerRunning ? TFT_RED : TFT_GREEN);
}

void updateTimerDisplay() {
    if (lockState) {
        tft.fillRect(0, 0, SCREEN_WIDTH, 50, TFT_WHITE);
        tft.setTextColor(TIMER_COLOR, TFT_WHITE);
        tft.setTextSize(3);
        
        if (timerRunning) {
            unsigned long remainingTime = (timerDuration - (millis() - startTime)) / 1000;
            if (remainingTime > 0) {
                String timeText = "Time: " + String(remainingTime) + " sec";
                tft.drawCentreString(timeText, SCREEN_WIDTH/2, 10, 1);
            } else {
                timerRunning = false;
                tft.drawCentreString("Time's up!", SCREEN_WIDTH/2, 10, 1);
                greenState = false;
                pumpState = false;
                digitalWrite(LED_RED, LOW);
                tone(buzzer, 2000, 2000);
            }
            tft.fillRoundRect(SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 - 30, 300, 60, 10, TFT_WHITE);
            tft.drawRoundRect(SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 - 30, 300, 60, 10, TFT_RED);
            tft.drawCentreString("Screen Locked", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 20, 1);
        }
    } else {
        tft.fillRect(180, 140, 200, 30, BACKGROUND);
        tft.setTextColor(TIMER_COLOR);
        tft.setTextSize(2);
        
        if (timerRunning) {
            unsigned long remainingTime = (timerDuration - (millis() - startTime)) / 1000;
            if (remainingTime > 0) {
                String timeText = String(remainingTime) + " sec";
                tft.drawString(timeText, 180, 140, 1);
            } else {
                timerRunning = false;
                tft.drawString("Time's up!", 180, 140, 1);
                greenState = false;
                pumpState = false;
                digitalWrite(LED_RED, LOW);
                tone(buzzer, 2000, 2000);
            }
        } else {
            String timeText = String(timerValue) + " sec";
            tft.drawString(timeText, 180, 140, 1);
        }
    }
}

void showLockMessage() {
    for(int y = 0; y < SCREEN_HEIGHT; y += 4) {
        for(int x = 0; x < SCREEN_WIDTH; x += 4) {
            tft.drawPixel(x, y, TFT_LIGHTGREY);
        }
    }
    
    tft.setTextColor(TFT_RED, TFT_WHITE);
    tft.setTextSize(3);
    tft.fillRoundRect(SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 - 30, 300, 60, 10, TFT_WHITE);
    tft.drawRoundRect(SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 - 30, 300, 60, 10, TFT_RED);
    tft.drawCentreString("Screen Locked", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 20, 1);

    if (timerRunning) {
        tft.fillRect(0, 0, SCREEN_WIDTH, 50, TFT_WHITE);
        tft.setTextColor(TIMER_COLOR, TFT_WHITE);
        tft.setTextSize(3);
        unsigned long remainingTime = (timerDuration - (millis() - startTime)) / 1000;
        if (remainingTime > 0) {
            String timeText = "Time: " + String(remainingTime) + " sec";
            tft.drawCentreString(timeText, SCREEN_WIDTH/2, 10, 1);
        } else {
            tft.drawCentreString("Time's up!", SCREEN_WIDTH/2, 10, 1);
        }
    }
}

void hideLockMessage() {
    drawInterface();
}

bool isButtonPressed(Button btn, int x, int y) {
    return (x >= btn.x && x <= (btn.x + btn.width) &&
            y >= (btn.y) && y <= (btn.y + btn.height));
}

void updatePressure(int change) {
    int newPressure = targetPressure + change;
    if (newPressure >= minPressure && newPressure <= maxPressure) {
        targetPressure = newPressure;
        tft.fillRect(180, 80, 200, 40, BACKGROUND);
        tft.setTextColor(VALUE_COLOR);
        tft.setTextSize(3);
        String pressureText = String(targetPressure) + " mmHg";
        tft.drawString(pressureText, 180, 80, 1);
    }
}

float scaleFromCenter(float value, float center, float scaling) {
    float distance = value - center;
    return center + (distance * scaling);
}

void startTimer() {
    if (!timerRunning) {
        timerRunning = true;
        startTime = millis();
        timerDuration = timerValue * 1000;
        Serial.println("Timer started");
        drawButton(startBtn, TFT_RED);
    }
}

void stopTimer() {
    if (timerRunning) {
        timerRunning = false;
        digitalWrite(LED_GREEN, LOW);
        Serial.println("Timer stopped");
        drawButton(startBtn, TFT_GREEN);
    }
}

void control_buzzer(int rt_pressure){
  int threshold = rt_pressure - targetPressure;
  if(threshold > 10){
    greenState = false;
    isBuzzing = true;
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
  } else if(threshold < -10){
    greenState = true;
    isBuzzing = false;
    noTone(buzzer);
    digitalWrite(LED_RED, HIGH);
  } else {
    greenState = true;
    isBuzzing = false;
    noTone(buzzer);
    digitalWrite(LED_RED, LOW);
  }
   
  if (isBuzzing) {
    unsigned long currentMillis = millis();
    if(currentMillis - previousMillis >= interval){
      previousMillis = currentMillis;
      tone(buzzer, 4186);
      delay(100);
      tone(buzzer, 4434.92);
      delay(100);
      tone(buzzer, 4698.63);
      delay(100);
      tone(buzzer, 4978);
      delay(100);
      tone(buzzer, 5274);
      delay(100);
      noTone(buzzer);
    }
  }
}

void setup() {
    Serial.begin(115200);
    
    // Initialize physical buttons
    pinMode(BUTTON_1, INPUT);
    pinMode(BUTTON_2, INPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(buzzer, OUTPUT);
    
    // Initialize touchscreen
    touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touchscreen.begin(touchscreenSPI);
    touchscreen.setRotation(1);
    
    // Initialize display
    tft.init();
    tft.setRotation(1);
    
    // Draw initial interface
    drawInterface();

    // Configure PWM
    pinMode(pumpPin, OUTPUT);
    
    if (!bmp.begin(0x76)) {
        Serial.println("Could not find BMP280 sensor!");
        while (1);
    }

    myPID.SetMode(AUTOMATIC);
    myPID.SetOutputLimits(minOutput, 255);
    myPID.SetSampleTime(100);
}

void loop() {
    // Read pressure from BMP280
    input = bmp.readPressure() / 100.0F; // Read pressure in hPa

    // Convert pressure to mmHg
    float pressure_mmHg = input * 0.750062;

    // Print pressure value to Serial Monitor in mmHg
    Serial.print("Pressure = ");
    Serial.print(pressure_mmHg, 2); // Print pressure in mmHg
    Serial.println(" mmHg");

    // Activate buzzer safety system ONLY if treatment has started (pump is on)
    //int rt_pressure = 0;

    if (pumpState) {
        int rt_pressure = pressure_mmHg; // Use the converted pressure value
        control_buzzer(rt_pressure);
    } 

    // Update pressure display on TFT with the TARGET pressure (setpoint)
    tft.fillRect(180, 80, 200, 40, BACKGROUND);
    tft.setTextColor(VALUE_COLOR);
    tft.setTextSize(3);
    String pressureText = String(targetPressure) + " mmHg"; // Display target pressure
    tft.drawString(pressureText, 180, 80, 1);

    // Rest of the PID and pump control logic (still uses hPa for calculations)
    double error = input - setpoint;
    if (error > 10) {
        output = minOutput + (kp * error);
        if (output > 255) output = 255;
    } else {
        myPID.Compute();
    }

    // Use analogWrite instead of ledcWrite
    analogWrite(pumpPin, output);

    // Handle physical buttons
    currentState_1 = digitalRead(BUTTON_1);
    
    if (lastState_1 == HIGH && currentState_1 == LOW) {
        if (!pumpState) {
            digitalWrite(LED_GREEN, HIGH);
            pumpState = true;
            greenState = true;
            //control_buzzer(rt_pressure);
            startTimer();
        } else  {
            digitalWrite(LED_GREEN, LOW);
            //digitalWrite(LED_RED, LOW);
            //noTone(buzzer);
            pumpState = false;
            greenState = false;
            stopTimer();
        }
      
        Serial.print("Pump state: ");
        Serial.print(pumpState); 
        Serial.println("\t");
    }
    lastState_1 = currentState_1;

    currentState_2 = digitalRead(BUTTON_2);
    if (lastState_2 == HIGH && currentState_2 == LOW) {
        lockState = !lockState;
        
        if (lockState) {
            showLockMessage();
        } else {
            hideLockMessage();
        }
        
        Serial.print("Lock state: ");
        Serial.print(lockState);
        Serial.println("\t");
    }
    lastState_2 = currentState_2;

    if(greenState){
        digitalWrite(LED_GREEN, HIGH);
    } else {
        digitalWrite(LED_GREEN, LOW);
    }

    if (!lockState) {
        if (touchscreen.tirqTouched() && touchscreen.touched()) {
            TS_Point p = touchscreen.getPoint();
            
            float mappedX = map(p.x, 3822, 305, 1, 480);
            float mappedY = map(p.y, 3775, 376, 1, 320);
            
            float screenCenterX = 240;
            float screenCenterY = 160;
            
            mappedX = scaleFromCenter(mappedX, screenCenterX, X_SCALING) + X_OFFSET;
            mappedY = scaleFromCenter(mappedY, screenCenterY, Y_SCALING) + Y_OFFSET;

            int touchX = mappedX;
            int touchY = mappedY;
            
            if (isButtonPressed(minusBtn, touchX, touchY)) {
                updatePressure(-pressureStep);
            }
            else if (isButtonPressed(plusBtn, touchX, touchY)) {
                updatePressure(pressureStep);
            }
            else if (isButtonPressed(timerMinusBtn, touchX, touchY) && !timerRunning) {
                if (timerValue > 5) timerValue -= 5;
                updateTimerDisplay();
            }
            else if (isButtonPressed(timerPlusBtn, touchX, touchY) && !timerRunning) {
                if (timerValue < 3600) timerValue += 5;
                updateTimerDisplay();
            }
            else if (isButtonPressed(startBtn, touchX, touchY)) {
                if (!timerRunning) {
                    startTimer();
                } else {
                    stopTimer();
                }
            }
            delay(200);
        }
    }
    
    if (timerRunning) {
        static unsigned long lastUpdate = 0;
        if (millis() - lastUpdate >= 1000) {
            updateTimerDisplay();
            lastUpdate = millis();
        }
    }

    delay(100);
}
