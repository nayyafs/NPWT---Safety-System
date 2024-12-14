#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

TFT_eSPI tft = TFT_eSPI();

// Touchscreen pins
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

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
int currentPressure = 125;
const int pressureStep = 5;
const int minPressure = 50;
const int maxPressure = 200;

// Timer variables
unsigned long timerDuration = 0;
unsigned long startTime = 0;
bool timerRunning = false;
int timerValue = 30; // Default timer value in seconds

// LED Pin
const int LED_PIN = 19;

// Button coordinates
struct Button {
    int x;
    int y;
    int width;
    int height;
    const char* text;
};

// Define buttons (adjust these coordinates based on your calibration)
#define BUTTON_SPACING 10

#define ROW_Y 180

Button minusBtn = {50, ROW_Y, BUTTON_WIDTH, BUTTON_HEIGHT, "-"};
Button plusBtn = {50 + BUTTON_WIDTH + BUTTON_SPACING, ROW_Y, BUTTON_WIDTH, BUTTON_HEIGHT, "+"};
Button timerMinusBtn = {50 + 2 * (BUTTON_WIDTH + BUTTON_SPACING), ROW_Y, BUTTON_WIDTH, BUTTON_HEIGHT, "-"};
Button timerPlusBtn = {50 + 3 * (BUTTON_WIDTH + BUTTON_SPACING), ROW_Y, BUTTON_WIDTH, BUTTON_HEIGHT, "+"};
Button startBtn = {50 + 4 * (BUTTON_WIDTH + BUTTON_SPACING), ROW_Y, BUTTON_WIDTH * 1.5, BUTTON_HEIGHT, "Start"};

/*Button minusBtn = {120, 180, BUTTON_WIDTH, BUTTON_HEIGHT, "-"};
Button plusBtn = {220, 180, BUTTON_WIDTH, BUTTON_HEIGHT, "+"};
Button timerMinusBtn = {120, 250, BUTTON_WIDTH, BUTTON_HEIGHT, "-"};
Button timerPlusBtn = {220, 250, BUTTON_WIDTH, BUTTON_HEIGHT, "+"};
Button startBtn = {320, 250, BUTTON_WIDTH*1.5, BUTTON_HEIGHT, "Start"};*/

// buzzer
const int buzzer = 18;
bool isBuzzing = false;

// Scaling factors (adjust these values)
#define X_SCALING 1.05  // Reduce this to "compress" horizontal spread
#define Y_SCALING 0.65  // Reduce this to "compress" vertical spread

// Offset values (positive values move right/down, negative values move left/up)
#define X_OFFSET 25   // Adjust this to shift touch horizontally
#define Y_OFFSET 3  // Adjust this to shift touch vertically

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
    String pressureText = String(currentPressure) + " mmHg";
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
    // Clear timer area
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
            digitalWrite(LED_PIN, LOW);
        }
    } else {
        String timeText = String(timerValue) + " sec";
        tft.drawString(timeText, 180, 140, 1);
    }
}

bool isButtonPressed(Button btn, int x, int y) {
    return (x >= btn.x && x <= (btn.x + btn.width) &&
            //y >= (btn.y - 85) && y <= (btn.y + btn.height - 85));
            y >= (btn.y) && y <= (btn.y + btn.height));
}

void updatePressure(int change) {
    int newPressure = currentPressure + change;
    if (newPressure >= minPressure && newPressure <= maxPressure) {
        currentPressure = newPressure;
        tft.fillRect(180, 80, 200, 40, BACKGROUND);
        tft.setTextColor(VALUE_COLOR);
        tft.setTextSize(3);
        String pressureText = String(currentPressure) + " mmHg";
        tft.drawString(pressureText, 180, 80, 1);
    }
}

float scaleFromCenter(float value, float center, float scaling) {
    // Calculate distance from center and apply scaling
    float distance = value - center;
    return center + (distance * scaling);
}

void setup() {
    Serial.begin(115200);
    
    // Initialize pins
    pinMode(LED_PIN, OUTPUT);
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
}

void loop() {
    // Debug touch coordinates
    if (touchscreen.tirqTouched() && touchscreen.touched()) {
        TS_Point p = touchscreen.getPoint();
        
        // Print raw and mapped coordinates
        Serial.print("Raw X = "); Serial.print(p.x);
        Serial.print(", Raw Y = "); Serial.println(p.y);
        
        //int mappedX = map(p.x, 3422, 494, 1, 480);
        //int mappedY = map(p.y, 3098, 715, 1, 320);

        float mappedX = map(p.x, 3822, 305, 1, 480);  // Using your previous values
        float mappedY = map(p.y, 3775, 376, 1, 320);
        
        // Apply scaling from center
        float screenCenterX = 240;  // Screen center X
        float screenCenterY = 160;  // Screen center Y
        
        mappedX = scaleFromCenter(mappedX, screenCenterX, X_SCALING);
        mappedY = scaleFromCenter(mappedY, screenCenterY, Y_SCALING);
        
        // Apply offsets
        mappedX += X_OFFSET;
        mappedY += Y_OFFSET;

        int touchX = mappedX;
        int touchY = mappedY;
        
        
        //Serial.print("Mapped X = "); Serial.print(touchX);
        //Serial.print(", Mapped Y = "); Serial.println(touchY);
        //Serial.print("Mapped X = "); Serial.print(mappedX);
        //Serial.print("Mapped Y = "); Serial.print(mappedY);
        
        // Handle button presses
        if (isButtonPressed(minusBtn, touchX, touchY)) {
            updatePressure(-pressureStep);
            Serial.println("Minus pressed");
        }
        else if (isButtonPressed(plusBtn, touchX, touchY)) {
            updatePressure(pressureStep);
            Serial.println("Plus pressed");
        }
        else if (isButtonPressed(timerMinusBtn, touchX, touchY) && !timerRunning) {
            if (timerValue > 5) timerValue -= 5;
            updateTimerDisplay();
            Serial.println("Timer minus pressed");
        }
        else if (isButtonPressed(timerPlusBtn, touchX, touchY) && !timerRunning) {
            if (timerValue < 3600) timerValue += 5;
            updateTimerDisplay();
            Serial.println("Timer plus pressed");
        }
        else if (isButtonPressed(startBtn, touchX, touchY)) {
            if (!timerRunning) {
                timerRunning = true;
                startTime = millis();
                timerDuration = timerValue * 1000;
                digitalWrite(LED_PIN, HIGH);
                Serial.println("Timer started");
            } else {
                timerRunning = false;
                digitalWrite(LED_PIN, LOW);
                Serial.println("Timer stopped");
            }
            drawButton(startBtn, timerRunning ? TFT_RED : TFT_GREEN);
        }
        delay(200); // Debounce
    }
    
    // Update timer display if running
    if (timerRunning) {
        static unsigned long lastUpdate = 0;
        if (millis() - lastUpdate >= 1000) {
            updateTimerDisplay();
            lastUpdate = millis();
        }
    }
}
