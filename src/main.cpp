//* Template for using Squareline Studio ui output with
//*   Cheap Yellow Display ("CYD") (aka ESP32-2432S028R)
//* (for example https://www.aliexpress.us/item/3256805998556027.html)
//*
//* 




#include <Arduino.h>
#include <SPI.h>

/*Using LVGL with Arduino requires some extra steps:
 *Be sure to read the docs here: https://docs.lvgl.io/master/get-started/platforms/arduino.html  */

#include <lvgl.h>
#include <TFT_eSPI.h>
#include "ui.h"
#include <XPT2046_Touchscreen.h>
#include <Preferences.h> // include Preferences library for saving bale variables across reboots
// A library for interfacing with the touch screen
//
// Can be installed from the library manager (Search for "XPT2046")
// https://github.com/PaulStoffregen/XPT2046_Touchscreen
// ----------------------------
// Touch Screen pins
// ----------------------------

// The CYD touch uses some non default
// SPI pins for Touchscreen
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

#define BRIGHTNESS_ENABLED // Uncomment to enable brightness control

// Bale counting variables
int bale_count = 0;
int bale_count_year = 0;
int flake_count = 0;
int flake_count_prev1 = 0;  // Previous bale's flake count
int flake_count_prev2 = 0;  // Two bales ago flake count
Preferences preferences; // Preferences object for saving bale count across reboots

// Bales per hour tracking variables
unsigned long first_bale_time = 0;  // Time when first bale was detected (in milliseconds)
unsigned long last_bale_time = 0;   // Time when last bale was detected (in milliseconds)
int bales_in_session = 0;           // Number of bales counted in current session
float bales_per_hour = 0.0;         // Calculated bales per hour

// GPIO 35 for binary input sensor (from your old code)
#define BALE_SENSOR_PIN 35
// GPIO 22 for flake count sensor
#define FLAKE_SENSOR_PIN 22

SPIClass mySpi = SPIClass(VSPI); // critical to get touch working

XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

/*Change to your screen resolution*/
static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char *buf)
{
    Serial.printf(buf);
    Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

static unsigned long lastTouchTime = 0;
static bool touchProcessed = false;

// Brightness control variables
int current_brightness = 80;  // Start at 80%

// Function to update the bale count display on the UI
void updateBaleCountDisplay() {
    char count_buf[16];
    lv_snprintf(count_buf, sizeof(count_buf), "%d", bale_count);
    lv_label_set_text(uiCYD_BaleCount, count_buf);
}

// Function to update the yearly bale count display on the UI
void updateBaleCountYearDisplay() {
    char count_buf[16];
    lv_snprintf(count_buf, sizeof(count_buf), "%d", bale_count_year);
    lv_label_set_text(uiCYD_BaleCountYear, count_buf);
}

// Function to update the flake count display on the UI
void updateFlakeCountDisplay() {
    char count_buf[16];
    lv_snprintf(count_buf, sizeof(count_buf), "%d", flake_count);
    lv_label_set_text(uiCYD_FlakeCountCurrent, count_buf);
}

// Function to update the previous flake count displays on the UI
void updateFlakeCountPrev1Display() {
    char count_buf[16];
    lv_snprintf(count_buf, sizeof(count_buf), "%d", flake_count_prev1);
    lv_label_set_text(uiCYD_FlakeCountPrev1, count_buf);
}

void updateFlakeCountPrev2Display() {
    char count_buf[16];
    lv_snprintf(count_buf, sizeof(count_buf), "%d", flake_count_prev2);
    lv_label_set_text(uiCYD_FlakeCountPrev2, count_buf);
}

// Function to update the bales per hour display on the UI
void updateBalesPerHourDisplay() {
    char rate_buf[16];
    if (bales_per_hour == 0.0) {
        sprintf(rate_buf, "0");
    } else if (bales_per_hour < 10.0) {
        sprintf(rate_buf, "%.1f", bales_per_hour);
    } else {
        sprintf(rate_buf, "%.0f", bales_per_hour);
    }
    lv_label_set_text(uiCYD_BaleCountHour, rate_buf);
    
    Serial.print("Updated bales per hour display to: ");
    Serial.println(rate_buf);
}

// Function to calculate bales per hour based on current session
void calculateBalesPerHour() {
    Serial.print("Calculating bales per hour - Session bales: ");
    Serial.println(bales_in_session);
    
    if (bales_in_session <= 1) {
        // Need at least 2 bales to calculate a rate
        bales_per_hour = 0.0;
        Serial.println("Not enough bales for rate calculation");
    } else {
        unsigned long elapsed_time_ms = last_bale_time - first_bale_time;
        Serial.print("Elapsed time (ms): ");
        Serial.println(elapsed_time_ms);
        
        if (elapsed_time_ms > 0) {
            // Convert milliseconds to hours and calculate rate
            float elapsed_hours = elapsed_time_ms / 3600000.0;  // 3600000 ms = 1 hour
            bales_per_hour = (bales_in_session - 1) / elapsed_hours;  // -1 because we count intervals
            
            Serial.print("Elapsed hours: ");
            Serial.println(elapsed_hours, 6);
            Serial.print("Calculated rate: ");
            Serial.println(bales_per_hour);
        } else {
            bales_per_hour = 0.0;
            Serial.println("Zero elapsed time");
        }
    }
    updateBalesPerHourDisplay();
}

// Function to increment bale count - needs C linkage for ui_events.c
extern "C" {
void incrementBaleCount() {
    bale_count++;
    bale_count_year++;  // Also increment yearly count
    
    // Track timing for bales per hour calculation
    unsigned long current_time = millis();
    
    if (bales_in_session == 0) {
        // This is the first bale of the session
        first_bale_time = current_time;
        bales_in_session = 1;
        bales_per_hour = 0.0;  // Can't calculate rate with just one bale
    } else {
        // Subsequent bales
        bales_in_session++;
        last_bale_time = current_time;
        calculateBalesPerHour();
    }
    
    // Shift flake counts: prev2 <- prev1 <- current, then reset current to 0
    flake_count_prev2 = flake_count_prev1;
    flake_count_prev1 = flake_count;
    flake_count = 0;  // Reset current flake count for new bale
    
    updateBaleCountDisplay();
    updateBaleCountYearDisplay();
    updateFlakeCountDisplay();
    updateFlakeCountPrev1Display();
    updateFlakeCountPrev2Display();
    
    // Save the updated counts to preferences
    preferences.putUInt("bale_count", bale_count);
    preferences.putUInt("bale_count_year", bale_count_year);
    preferences.putUInt("flake_count", flake_count);
    preferences.putUInt("flake_prev1", flake_count_prev1);
    preferences.putUInt("flake_prev2", flake_count_prev2);
    
    Serial.print("Bale count incremented to: ");
    Serial.println(bale_count);
    Serial.print("Yearly bale count incremented to: ");
    Serial.println(bale_count_year);
    Serial.print("Bales in session: ");
    Serial.println(bales_in_session);
    Serial.print("Current rate: ");
    Serial.print(bales_per_hour);
    Serial.println(" bales/hour");
    Serial.print("Flake counts shifted - Current: ");
    Serial.print(flake_count);
    Serial.print(", Prev1: ");
    Serial.print(flake_count_prev1);
    Serial.print(", Prev2: ");
    Serial.println(flake_count_prev2);
    Serial.println("All counts saved to preferences");
}

void incrementFlakeCount() {
    flake_count++;
    updateFlakeCountDisplay();
    
    // Save the updated count to preferences
    preferences.putUInt("flake_count", flake_count);
    // Also save previous flake counts to ensure they're always current
    preferences.putUInt("flake_prev1", flake_count_prev1);
    preferences.putUInt("flake_prev2", flake_count_prev2);
    
    Serial.print("Flake count incremented to: ");
    Serial.println(flake_count);
}

// Function to reset the bales per hour session
void resetBalesPerHourSession() {
    bales_in_session = 0;
    bales_per_hour = 0.0;
    first_bale_time = 0;
    last_bale_time = 0;
    updateBalesPerHourDisplay();
    
    Serial.println("Bales per hour session reset");
}

void resetBaleCount() {
    bale_count = 0;
    updateBaleCountDisplay();
    
    // Reset the bales per hour session when bale count is reset
    resetBalesPerHourSession();
    
    // Save the reset count to preferences
    preferences.putUInt("bale_count", bale_count);
    
    Serial.println("Bale count reset to 0");
}

void resetBaleCountYear() {
    bale_count_year = 0;
    updateBaleCountYearDisplay();
    
    // Save the reset count to preferences
    preferences.putUInt("bale_count_year", bale_count_year);
    
    Serial.println("Yearly bale count reset to 0");
}

void resetFlakeCount() {
    flake_count = 0;
    flake_count_prev1 = 0;
    flake_count_prev2 = 0;
    updateFlakeCountDisplay();
    updateFlakeCountPrev1Display();
    updateFlakeCountPrev2Display();
    
    // Save the reset counts to preferences
    preferences.putUInt("flake_count", flake_count);
    preferences.putUInt("flake_prev1", flake_count_prev1);
    preferences.putUInt("flake_prev2", flake_count_prev2);
    
    Serial.println("All flake counts reset to 0");
    Serial.println("All flake counts saved to preferences");
}

// Debug function to verify preferences are working
void debugPreferences() {
    Serial.println("=== PREFERENCES DEBUG ===");
    Serial.print("Current flake_count: ");
    Serial.println(flake_count);
    Serial.print("Current flake_count_prev1: ");
    Serial.println(flake_count_prev1);
    Serial.print("Current flake_count_prev2: ");
    Serial.println(flake_count_prev2);
    
    // Read what's actually stored in preferences
    unsigned int stored_current = preferences.getUInt("flake_count", 999);
    unsigned int stored_prev1 = preferences.getUInt("flake_prev1", 999);
    unsigned int stored_prev2 = preferences.getUInt("flake_prev2", 999);
    
    Serial.print("Stored flake_count: ");
    Serial.println(stored_current);
    Serial.print("Stored flake_count_prev1: ");
    Serial.println(stored_prev1);
    Serial.print("Stored flake_count_prev2: ");
    Serial.println(stored_prev2);
    Serial.println("=========================");
}
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    uint16_t touchX, touchY;
    bool touched = (ts.tirqTouched() && ts.touched());
    
    if (!touched)
    {
        data->state = LV_INDEV_STATE_REL;
        touchProcessed = false; // Reset when touch is released
    }
    else
    {
        // Debounce touch input
        unsigned long currentTime = millis();
        if (currentTime - lastTouchTime < 100) { // 100ms debounce
            return;
        }
        
        TS_Point p = ts.getPoint();
        touchX = map(p.x, 200, 3700, 1, screenWidth);
        touchY = map(p.y, 240, 3800, 1, screenHeight);
        
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;

#ifdef BRIGHTNESS_ENABLED

        // Check for brightness control touch areas (top quadrants of screen)
        static unsigned long last_brightness_change = 0;
        
        if (touchY < screenHeight / 2 && currentTime - last_brightness_change > 200) {  // Top half and debounce
            if (touchX < screenWidth / 2) {
                // Top left - decrease brightness
                current_brightness -= 10;
                if (current_brightness < 0) current_brightness = 0;
            } else {
                // Top right - increase brightness
                current_brightness += 10;
                if (current_brightness > 100) current_brightness = 100;
            }
            
            // Update brightness
            int pwm_value = map(current_brightness, 0, 100, 0, 255);
            analogWrite(21, pwm_value);
            
            last_brightness_change = currentTime;
            Serial.print("Brightness changed to ");
            Serial.print(current_brightness);
            Serial.println("%");
        }
#endif // BRIGHTNESS_ENABLED

        if (!touchProcessed) {
            Serial.print("Touch at x: ");
            Serial.print(touchX);
            Serial.print(", y: ");
            Serial.println(touchY);
            
            lastTouchTime = currentTime;
            touchProcessed = true;
        }
    }
}

void setup()
{
    Serial.begin(115200); /* prepare for possible serial debug */

    String LVGL_Arduino = "Hello Arduino! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    Serial.println(LVGL_Arduino);
    Serial.println("I am LVGL_Arduino");

    // Open Preferences with bale-nums namespace
    preferences.begin("bale-nums", false);
    
    // Load saved bale count from preferences
    bale_count = preferences.getUInt("bale_count", 0);  // Default to 0 if no saved value
    Serial.print("Loaded bale count from preferences: ");
    Serial.println(bale_count);
    
    // Load saved yearly bale count from preferences
    bale_count_year = preferences.getUInt("bale_count_year", 0);  // Default to 0 if no saved value
    Serial.print("Loaded yearly bale count from preferences: ");
    Serial.println(bale_count_year);
    
    // Load saved flake count from preferences
    flake_count = preferences.getUInt("flake_count", 0);  // Default to 0 if no saved value
    Serial.print("Loaded flake count from preferences: ");
    Serial.println(flake_count);
    
    // Load saved previous flake counts from preferences
    flake_count_prev1 = preferences.getUInt("flake_prev1", 0);
    Serial.print("Loaded flake count prev1 from preferences: ");
    Serial.println(flake_count_prev1);
    
    flake_count_prev2 = preferences.getUInt("flake_prev2", 0);
    Serial.print("Loaded flake count prev2 from preferences: ");
    Serial.println(flake_count_prev2);

    lv_init();

#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

    mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS); /* Start second SPI bus for touchscreen */
    ts.begin(mySpi);                                                  /* Touchscreen init */
    ts.setRotation(3);                                                /* Landscape orientation */

    tft.begin();        /* TFT init */
    tft.setRotation(3); // Landscape orientation  1 =  CYC usb on right, 2 for vertical, 3 for usb on left
    tft.invertDisplay(1); // Fix inverted colors - if colors are still wrong, try tft.invertDisplay(0)

    // Initialize the backlight pin for PWM control and set initial brightness
    pinMode(21, OUTPUT);  // TFT_BL pin
    analogWrite(21, 204); // Set initial brightness to 80% (204/255)
    
    // Initialize GPIO 35 as input with pull-up resistor for sensor
    pinMode(BALE_SENSOR_PIN, INPUT_PULLUP);
    
    // Initialize GPIO 22 as input with pull-up resistor for flake sensor
    pinMode(FLAKE_SENSOR_PIN, INPUT_PULLUP);

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    /*Change the following line to your display resolution*/
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /*Initialize the (dummy) input device driver*/
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    /* Uncomment to create simple label */
    // lv_obj_t *label = lv_label_create( lv_scr_act() );
    // lv_label_set_text( label, "Hello Ardino and LVGL!");
    // lv_obj_align( label, LV_ALIGN_CENTER, 0, 0 );

    ui_init();

    // Update the bale count display with the loaded value
    updateBaleCountDisplay();
    
    // Update the yearly bale count display with the loaded value
    updateBaleCountYearDisplay();
    
    // Update the flake count display with the loaded value
    updateFlakeCountDisplay();
    
    // Update the previous flake count displays with the loaded values
    updateFlakeCountPrev1Display();
    updateFlakeCountPrev2Display();
    
    // Initialize the bales per hour display
    updateBalesPerHourDisplay();

    // Debug preferences to verify they're working
    debugPreferences();

    Serial.println("Setup done");
}

void loop()
{
    lv_timer_handler(); /* let the GUI do its work */
    
    // Read bale sensor state and update display (matching old code behavior)
    static bool last_sensor_state = true;  // Start with HIGH (pull-up default)
    bool current_sensor_state = digitalRead(BALE_SENSOR_PIN);
    
    // Update bale sensor display and count when state changes
    if (current_sensor_state != last_sensor_state) {
        if (current_sensor_state == LOW) {  // Sensor triggered (active LOW)
            Serial.println("Bale sensor triggered: ON");
        } else {
            Serial.println("Bale sensor state: OFF");
            
            // Increment counter when sensor goes from ON (LOW) to OFF (HIGH) - end of detection
            incrementBaleCount();
            Serial.println("Bale detected by sensor!");
        }
        last_sensor_state = current_sensor_state;
    }
    
    // Read flake sensor state and update display
    static bool last_flake_sensor_state = true;  // Start with HIGH (pull-up default)
    bool current_flake_sensor_state = digitalRead(FLAKE_SENSOR_PIN);
    
    // Update flake sensor display and count when state changes
    if (current_flake_sensor_state != last_flake_sensor_state) {
        if (current_flake_sensor_state == LOW) {  // Sensor triggered (active LOW)
            Serial.println("Flake sensor triggered: ON");
        } else {
            Serial.println("Flake sensor state: OFF");
            
            // Increment counter when sensor goes from ON (LOW) to OFF (HIGH) - end of detection
            incrementFlakeCount();
            Serial.println("Flake detected by sensor!");
        }
        last_flake_sensor_state = current_flake_sensor_state;
    }
    
    delay(5);
}