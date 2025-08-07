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

    lv_init();

#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

    mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS); /* Start second SPI bus for touchscreen */
    ts.begin(mySpi);                                                  /* Touchscreen init */
    ts.setRotation(1);                                                /* Landscape orientation */

    tft.begin();        /* TFT init */
    tft.setRotation(1); // Landscape orientation  1 =  CYC usb on right, 2 for vertical
    tft.invertDisplay(1); // Fix inverted colors - if colors are still wrong, try tft.invertDisplay(0)

    // Initialize the backlight pin for PWM control and set initial brightness
    pinMode(21, OUTPUT);  // TFT_BL pin
    analogWrite(21, 204); // Set initial brightness to 80% (204/255)

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

    Serial.println("Setup done");
}

void loop()
{
    lv_timer_handler(); /* let the GUI do its work */
    delay(5);
}