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

// Pressure control
int targetPressure = 125; // Starting pressure in mmHg
const int pressureStep = 5; // Increment/decrement by 5 mmHg
const int minPressure = 50;
const int maxPressure = 200;

// Button coordinates
struct Button {
    int x;
    int y;
    int width;
    int height;
    const char* text;
};

// Define buttons
Button minusBtn = {160, 180, BUTTON_WIDTH, BUTTON_HEIGHT, "-"};
Button plusBtn = {260, 180, BUTTON_WIDTH, BUTTON_HEIGHT, "+"};

// buzzer
const int buzzer = 18;
bool isBuzzing = false;
unsigned long previousMillis = 0;
const long interval = 500;

// led
const int ledRed = 5;
const int ledGreen = 19;
bool pump_state = true;

// Function to draw a button
void drawButton(Button btn, uint16_t color) {
    tft.fillRoundRect(btn.x, btn.y, btn.width, btn.height, 5, color);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(3);
    tft.drawCentreString(btn.text, btn.x + btn.width/2, btn.y + btn.height/4, 1);
}

// Function to draw the main interface
void drawInterface() {
    // Clear screen
    tft.fillScreen(BACKGROUND);
    
    // Draw title
    tft.setTextColor(TEXT_COLOR);
    tft.setTextSize(2);
    tft.drawCentreString("NPWT Control", SCREEN_WIDTH/2, 20, 1);
    
    // Draw pressure value
    tft.setTextColor(VALUE_COLOR);
    tft.setTextSize(3);
    String pressureText = String(targetPressure) + " mmHg";
    tft.drawCentreString(pressureText, SCREEN_WIDTH/2, 80, 1);
    
    // Draw buttons
    drawButton(minusBtn, BUTTON_COLOR);
    drawButton(plusBtn, BUTTON_COLOR);
}

// Function to check if a point is within a button
bool isButtonPressed(Button btn, int x, int y) {
    return (x >= (btn.x) && x <= (btn.x + btn.width) &&
            y >= (btn.y - 85) && y <= (btn.y + btn.height - 85));
}

// Function to update pressure value
void updatePressure(int change) {
    int newPressure = targetPressure + change;
    if (newPressure >= minPressure && newPressure <= maxPressure) {
        targetPressure = newPressure;
        // Update pressure display
        tft.fillRect(140, 70, 200, 40, BACKGROUND); // Clear previous value
        tft.setTextColor(VALUE_COLOR);
        tft.setTextSize(3);
        String pressureText = String(targetPressure) + " mmHg";
        tft.drawCentreString(pressureText, SCREEN_WIDTH/2, 80, 1);
    }
}

void control_buzzer(int rt_pressure){
  int threshold = rt_pressure-targetPressure;
  if(threshold > 10){
    pump_state = false;
    isBuzzing = true;
    digitalWrite(ledRed, HIGH);
    digitalWrite(ledGreen, LOW);
    }
  else if(threshold < -10){
    pump_state = true;
    isBuzzing = false;
    noTone(buzzer);
    digitalWrite(ledRed, HIGH);
    }
  else{
    pump_state = true;
    isBuzzing = false;
    noTone(buzzer);
    digitalWrite(ledRed, LOW);
   }
   
  if (isBuzzing) {                    // If isBuzzing is true, play the tone sequence
    unsigned long currentMillis = millis();
    if(currentMillis - previousMillis >= interval){
      previousMillis = currentMillis;
      tone(buzzer, 1046.5);
      delay(100);
      tone(buzzer, 1108.73);
      delay(100);
      tone(buzzer, 1174.66);
      delay(100);
      tone(buzzer, 1244.51);
      delay(100);
      tone(buzzer, 1318.51);
      delay(100);
      noTone(buzzer);
    }
  }
  
  }

void setup() {
    Serial.begin(115200);
    
    // Initialize touchscreen
    touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touchscreen.begin(touchscreenSPI);
    touchscreen.setRotation(1);
    
    // Initialize display
    tft.init();
    tft.setRotation(1);

    pinMode(buzzer, OUTPUT);
    pinMode(ledRed, OUTPUT);
    pinMode(ledGreen, OUTPUT);

    // Draw initial interface
    drawInterface();

}

void loop() {
    if(Serial.available()>0){
      String bmp_pressure = Serial.readString(); //ini dihapus aja kalo udah ada bmp
      int rt_pressure = bmp_pressure.toInt(); // rt_pressure ambil dari nilai bmp, rt = real time
      control_buzzer(rt_pressure);
      }

    if(pump_state){  //pump state: kondisi pompa. kalo false: pompa mati
      digitalWrite(ledGreen, HIGH);
      }
    else{
      digitalWrite(ledGreen, LOW);
      }
    
    if (touchscreen.tirqTouched() && touchscreen.touched()) {
        TS_Point p = touchscreen.getPoint();
        
        // Using calibrated mapping values
        int touchX = map(p.x, 400, 3600, 1, 480);  // Adjusted X mapping
        int touchY = map(p.y, 400, 3600, 1, 320);  // Adjusted Y mapping
        
        // Debug - print touch coordinates
        Serial.print("X = ");
        Serial.print(touchX);
        Serial.print(" | Y = ");
        Serial.println(touchY);
        
        if (isButtonPressed(minusBtn, touchX, touchY)) {
            updatePressure(pressureStep);
            delay(200); // Debounce
        }
        else if (isButtonPressed(plusBtn, touchX, touchY)) {
            updatePressure(-pressureStep);
            delay(200); // Debounce
        }
    }
}
