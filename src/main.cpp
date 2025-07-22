/*  Rui Santos & Sara Santos - Random Nerd Tutorials
    THIS EXAMPLE WAS TESTED WITH THE FOLLOWING HARDWARE:
    1) ESP32-2432S028R 2.8 inch 240×320 also known as the Cheap Yellow Display (CYD): https://makeradvisor.com/tools/cyd-cheap-yellow-display-esp32-2432s028r/
      SET UP INSTRUCTIONS: https://RandomNerdTutorials.com/cyd-lvgl/
    2) REGULAR ESP32 Dev Board + 2.8 inch 240x320 TFT Display: https://makeradvisor.com/tools/2-8-inch-ili9341-tft-240x320/ and https://makeradvisor.com/tools/esp32-dev-board-wi-fi-bluetooth/
      SET UP INSTRUCTIONS: https://RandomNerdTutorials.com/esp32-tft-lvgl/
    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/

/*  Install the "lvgl" library version 9.2 by kisvegabor to interface with the TFT Display - https://lvgl.io/
    *** IMPORTANT: lv_conf.h available on the internet will probably NOT work with the examples available at Random Nerd Tutorials ***
    *** YOU MUST USE THE lv_conf.h FILE PROVIDED IN THE LINK BELOW IN ORDER TO USE THE EXAMPLES FROM RANDOM NERD TUTORIALS ***
    FULL INSTRUCTIONS AVAILABLE ON HOW CONFIGURE THE LIBRARY: https://RandomNerdTutorials.com/cyd-lvgl/ or https://RandomNerdTutorials.com/esp32-tft-lvgl/   */
#include <lvgl.h>

/*  Install the "TFT_eSPI" library by Bodmer to interface with the TFT Display - https://github.com/Bodmer/TFT_eSPI
    *** IMPORTANT: User_Setup.h available on the internet will probably NOT work with the examples available at Random Nerd Tutorials ***
    *** YOU MUST USE THE User_Setup.h FILE PROVIDED IN THE LINK BELOW IN ORDER TO USE THE EXAMPLES FROM RANDOM NERD TUTORIALS ***
    FULL INSTRUCTIONS AVAILABLE ON HOW CONFIGURE THE LIBRARY: https://RandomNerdTutorials.com/cyd-lvgl/ or https://RandomNerdTutorials.com/esp32-tft-lvgl/   */
#include <TFT_eSPI.h>

// Install the "XPT2046_Touchscreen" library by Paul Stoffregen to use the Touchscreen - https://github.com/PaulStoffregen/XPT2046_Touchscreen - Note: this library doesn't require further configuration
#include <XPT2046_Touchscreen.h>

// Touchscreen pins
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

// Touchscreen coordinates: (x, y) and pressure (z)
int x, y, z;

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// If logging is enabled, it will inform the user about what is happening in the library
void log_print(lv_log_level_t level, const char * buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

// Get the Touchscreen data
void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data) {
  // Checks if Touchscreen was touched, and prints X, Y and Pressure (Z)
  if(touchscreen.tirqTouched() && touchscreen.touched()) {
    // Get Touchscreen points
    TS_Point p = touchscreen.getPoint();
    // Calibrate Touchscreen points with map function to the correct width and height
    x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
    y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
    z = p.z;

    data->state = LV_INDEV_STATE_PRESSED;

    // Set the coordinates
    data->point.x = x;
    data->point.y = y;

    // Print Touchscreen info about X, Y and Pressure (Z) on the Serial Monitor
    /* Serial.print("X = ");
    Serial.print(x);
    Serial.print(" | Y = ");
    Serial.print(y);
    Serial.print(" | Pressure = ");
    Serial.print(z);
    Serial.println();*/
  }
  else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

int btn1_count = 0;
static lv_obj_t * counter_label;
static lv_obj_t * sensor_label;

// GPIO 35 for binary input sensor
#define SENSOR_PIN 35

// Callback that is triggered when btn1 is clicked
static void event_handler_btn1(lv_event_t * e) {
  lv_event_code_t code = lv_event_get_code(e);
  if(code == LV_EVENT_CLICKED) {
    btn1_count++;
    char count_buf[16];
    lv_snprintf(count_buf, sizeof(count_buf), "Count: %d", btn1_count);
    lv_label_set_text(counter_label, count_buf);
    LV_LOG_USER("Button clicked %d", (int)btn1_count);
  }
}

// Callback that is triggered when btn2 is clicked/toggled (Reset button)
static void event_handler_btn2(lv_event_t * e) {
  lv_event_code_t code = lv_event_get_code(e);
  if(code == LV_EVENT_CLICKED) {
    btn1_count = 0;
    lv_label_set_text(counter_label, "Count: 0");
    LV_LOG_USER("Counter reset to 0");
  }
}

static lv_obj_t * slider_label;
// Callback that adjusts screen brightness and displays the current slider value
static void slider_event_callback(lv_event_t * e) {
  lv_obj_t * slider = (lv_obj_t*) lv_event_get_target(e);
  int brightness_value = lv_slider_get_value(slider);
  
  // Map slider value (0-100) to PWM value (0-255)
  int pwm_value = map(brightness_value, 1, 100, 1, 255);
  
  // Set the brightness using PWM on the backlight pin
  analogWrite(21, pwm_value);  // TFT_BL pin
  
  char buf[16];
  lv_snprintf(buf, sizeof(buf), "Brightness: %d%%", brightness_value);
  lv_label_set_text(slider_label, buf);
  lv_obj_align_to(slider_label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
  LV_LOG_USER("Brightness changed to %d%% (PWM: %d)", brightness_value, pwm_value);
}

void lv_create_main_gui(void) {
  // Create a text label aligned center on top ("Bale Counter")
  lv_obj_t * text_label = lv_label_create(lv_screen_active());
  lv_label_set_long_mode(text_label, LV_LABEL_LONG_WRAP);    // Breaks the long lines
  lv_label_set_text(text_label, "Bale Counter");
  lv_obj_set_width(text_label, 150);    // Set smaller width to make the lines wrap
  lv_obj_set_style_text_align(text_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(text_label, LV_ALIGN_CENTER, 0, -100);

  // Create counter display label
  counter_label = lv_label_create(lv_screen_active());
  lv_label_set_text(counter_label, "Count: 0");
  lv_obj_set_style_text_align(counter_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(counter_label, LV_ALIGN_CENTER, 0, -80);

  // Create sensor status label
  sensor_label = lv_label_create(lv_screen_active());
  lv_label_set_text(sensor_label, "Sensor: off");
  lv_obj_set_style_text_align(sensor_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(sensor_label, LV_ALIGN_CENTER, 0, -60);

  lv_obj_t * btn_label;
  // Create a Button (btn1) - positioned to the left
  lv_obj_t * btn1 = lv_button_create(lv_screen_active());
  lv_obj_add_event_cb(btn1, event_handler_btn1, LV_EVENT_ALL, NULL);
  lv_obj_align(btn1, LV_ALIGN_CENTER, -60, -10);  // Move left by 60 pixels
  lv_obj_remove_flag(btn1, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_set_size(btn1, 100, 40);  // Set explicit size
  // Set button color to blue with stronger styling
  lv_obj_set_style_bg_color(btn1, lv_color_hex(0x0080FF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(btn1, lv_color_hex(0x0066CC), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn1, LV_OPA_COVER, LV_PART_MAIN);

  btn_label = lv_label_create(btn1);
  lv_label_set_text(btn_label, "Button");
  lv_obj_center(btn_label);
  lv_obj_set_style_text_color(btn_label, lv_color_white(), LV_PART_MAIN);

  // Create a Reset button (btn2) - positioned to the right
  lv_obj_t * btn2 = lv_button_create(lv_screen_active());
  lv_obj_add_event_cb(btn2, event_handler_btn2, LV_EVENT_ALL, NULL);
  lv_obj_align(btn2, LV_ALIGN_CENTER, 60, -10);   // Move right by 60 pixels
  lv_obj_set_size(btn2, 100, 40);  // Set explicit size
  // Set button color to blue with stronger styling
  lv_obj_set_style_bg_color(btn2, lv_color_hex(0x0080FF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(btn2, lv_color_hex(0x0066CC), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn2, LV_OPA_COVER, LV_PART_MAIN);

  btn_label = lv_label_create(btn2);
  lv_label_set_text(btn_label, "Reset");
  lv_obj_center(btn_label);
  lv_obj_set_style_text_color(btn_label, lv_color_white(), LV_PART_MAIN);
  
  // Create a slider aligned in the center bottom of the TFT display
  lv_obj_t * slider = lv_slider_create(lv_screen_active());
  lv_obj_align(slider, LV_ALIGN_CENTER, 0, 50);  // Moved up since buttons are now side by side
  lv_obj_add_event_cb(slider, slider_event_callback, LV_EVENT_VALUE_CHANGED, NULL);
  lv_slider_set_range(slider, 0, 100);
  lv_slider_set_value(slider, 80, LV_ANIM_OFF); // Set initial brightness to 80%
  lv_obj_set_style_anim_duration(slider, 2000, 0);
  // Set slider color to blue with stronger styling
  lv_obj_set_style_bg_color(slider, lv_color_hex(0x0080FF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(slider, lv_color_hex(0x0080FF), LV_PART_KNOB | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);

  // Create a label below the slider to display the current brightness value
  slider_label = lv_label_create(lv_screen_active());
  lv_label_set_text(slider_label, "Brightness: 80%");
  lv_obj_align_to(slider_label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
}

void setup() {
  String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.begin(115200);
  Serial.println(LVGL_Arduino);
  
  // Initialize the backlight pin for PWM control
  pinMode(21, OUTPUT);  // TFT_BL pin
  analogWrite(21, 204); // Set initial brightness to 80% (204/255)
  
  // Initialize GPIO 35 as input with pull-up resistor
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  
  // Start LVGL
  lv_init();
  // Register print function for debugging
  lv_log_register_print_cb(log_print);

  // Start the SPI for the touchscreen and init the touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  // Set the Touchscreen rotation in landscape mode
  // Note: in some displays, the touchscreen might be upside down, so you might need to set the rotation to 0: touchscreen.setRotation(0);
  touchscreen.setRotation(2);

  // Create a display object
  lv_display_t * disp;
  // Initialize the TFT display using the TFT_eSPI library
  disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
    
  // Initialize an LVGL input device object (Touchscreen)
  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  // Set the callback function to read Touchscreen input
  lv_indev_set_read_cb(indev, touchscreen_read);

  // Function to draw the GUI (text, buttons and sliders)
  lv_create_main_gui();
}

void loop() {
  lv_task_handler();  // let the GUI do its work
  lv_tick_inc(5);     // tell LVGL how much time has passed
  
  // Read sensor state and update display
  static bool last_sensor_state = false;
  bool current_sensor_state = digitalRead(SENSOR_PIN);
  
  // Update sensor display if state changed
  if (current_sensor_state != last_sensor_state) {
    if (current_sensor_state == LOW) {  // Assuming active LOW (pulled to ground when triggered)
      lv_label_set_text(sensor_label, "Sensor: on");
      LV_LOG_USER("Sensor triggered: ON");
    } else {
      lv_label_set_text(sensor_label, "Sensor: off");
      LV_LOG_USER("Sensor state: OFF");
    }
    last_sensor_state = current_sensor_state;
  }
  
  delay(5);           // let this time pass
}