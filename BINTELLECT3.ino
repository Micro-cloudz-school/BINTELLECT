#include <BINTELLECT-muffin3_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include <ESP32Servo.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>


// 🔹 Replace with your WiFi credentials
const char* ssid = "****";
const char* password = "********";


// 🔹 Replace with your Supabase info
const char* supabaseUrl = "https://trdowkfrxtlryouwssci.supabase.co/rest/v1/************";
const char* supabaseKey = "********"



bool runCode = true;        // FIXED
Servo myServo;              // FIXED


/*
 * Freenove ESP32-WROVER-E + OV3660 Camera Pin Mapping
 */
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      21
#define SIOD_GPIO_NUM      26
#define SIOC_GPIO_NUM      27


#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       19
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM        5
#define Y2_GPIO_NUM        4


#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22


/* Camera buffer settings */
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS 320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS 240
#define EI_CAMERA_FRAME_BYTE_SIZE 3


static bool debug_nn = false;
static bool is_initialised = false;
uint8_t *snapshot_buf;


/*
 * Camera configuration
 */
static camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,


    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,


    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,


    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    //.pixel_format = PIXFORMAT_JPEG,
    .pixel_format = PIXFORMAT_RGB565, //Changed from PIXFORMAT_JPEG to reduce chances of artifacts created by conversion
    .frame_size = FRAMESIZE_QVGA,
    .jpeg_quality = 12,
    .fb_count = 1,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY
};


/*
 * Forward declarations
 */
bool ei_camera_init(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf);
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr);


/*
 * Setup
 */
void setup() {
    Serial.begin(115200);
    myServo.attach(33);
    myServo.write(75);
    while (!Serial);


    Serial.println("Edge Impulse Inferencing Demo (Freenove OV3660)");


    if (!ei_camera_init()) {
        Serial.println("Camera init failed!");
    } else {
        Serial.println("Camera initialized.");
    }


    ei_sleep(2000);


      WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");


  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }


  Serial.println("\nConnected!");


}


/*
 * Main loop
 */
void loop() {


    if (runCode) {


        if (ei_sleep(5) != EI_IMPULSE_OK) return;


        snapshot_buf = (uint8_t*)malloc(
            EI_CAMERA_RAW_FRAME_BUFFER_COLS *
            EI_CAMERA_RAW_FRAME_BUFFER_ROWS *
            EI_CAMERA_FRAME_BYTE_SIZE
        );


        if (!snapshot_buf) {
            Serial.println("ERR: Failed to allocate snapshot buffer!");
            return;
        }


        ei::signal_t signal;
        signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
        signal.get_data = &ei_camera_get_data;


        if (!ei_camera_capture(EI_CLASSIFIER_INPUT_WIDTH,
                               EI_CLASSIFIER_INPUT_HEIGHT,
                               snapshot_buf)) {
            Serial.println("Failed to capture image");
            free(snapshot_buf);
            return;
        }


        ei_impulse_result_t result = {0};
        EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);


        if (err != EI_IMPULSE_OK) {
            Serial.printf("ERR: Failed to run classifier (%d)\n", err);
            free(snapshot_buf);
            return;
        }


        ei_printf("Predictions:\n");
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            ei_printf("  Index %d: ", ix);
            ei_printf("%s\t", ei_classifier_inferencing_categories[ix]);
            ei_printf("Confidence: %.2f\n", result.classification[ix].value);
        }


        // === SERVO LOGIC (UNCHANGED) ===
        if(result.classification[0].value >= 0.73){
            myServo.attach(33);
            myServo.write(0);
            Serial.println("Bread");
            Serial.println(String(result.classification[0].value * 100, 2));
            delay(500);
            myServo.write(75);
            sendData("Bread", result.classification[0].value*100);
        }
        else if(result.classification[1].value >= 0.73){
            myServo.attach(33);
            myServo.write(170);
            Serial.println("Coffee Pod");
            Serial.println(String(result.classification[1].value * 100, 2));
            delay(500);
            myServo.write(75);
            sendData("Coffee Pod", result.classification[1].value*100);
        }
        else if(result.classification[2].value >= 0.73){
            myServo.attach(33);
            myServo.write(0);
            Serial.println("Lemon");
            Serial.println(String(result.classification[2].value * 100, 2));
            delay(500);
            myServo.write(75);
            sendData("Lemon", result.classification[2].value*100);
        }
        else if(result.classification[3].value >= 0.73){
            myServo.attach(33);
            myServo.write(170);
            Serial.println("Mint Container");
            Serial.println(String(result.classification[3].value * 100, 2));
            delay(500);
            myServo.write(75);
            delay(1500);
            sendData("Mint Container", result.classification[3].value*100);
        }
        else if(result.classification[4].value >= 0.73){
            myServo.detach();
        }


        free(snapshot_buf);
    }


    // === SERIAL COMMANDS ===
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        input.toLowerCase();


        if (input == "stop") {
            runCode = false;
        }
        else if (input == "open") {
            myServo.attach(33);
            Serial.println("Opening bin. Type 'start' or 'close' to resume operations");
            myServo.write(180);
            runCode = false;
        }
        else if (input == "start") {
            runCode = true;
        }
        else if (input == "close") {
            myServo.write(75);
            runCode = true;
        }
        else if (input == "restart"){
            runCode = false;
            delay(1000);
            runCode = true;
        }
    }
}


/*
 * Provide pixel data to Edge Impulse
 */
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr) {
    size_t pixel_ix = offset * 3;
    size_t out_ix = 0;


    while (length--) {
        out_ptr[out_ix++] =
            (snapshot_buf[pixel_ix + 2] << 16) |
            (snapshot_buf[pixel_ix + 1] << 8) |
            snapshot_buf[pixel_ix];
        pixel_ix += 3;
    }


    return 0;
}


/*
 * Camera init
 */
bool ei_camera_init(void) {
    if (is_initialised) return true;


    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: 0x%x\n", err);
        return false;
    }


    sensor_t *s = esp_camera_sensor_get();
    if (s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1);
        s->set_brightness(s, 1);
        s->set_saturation(s, 0);
    }


    is_initialised = true;
    return true;
}


/*
 * Capture + convert + resize
 */
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
    if (!is_initialised) return false;


    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return false;


    bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_RGB565, snapshot_buf);
    esp_camera_fb_return(fb);


    if (!converted) return false;


    if (img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS ||
        img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS) {
        ei::image::processing::crop_and_interpolate_rgb888(
            out_buf,
            EI_CAMERA_RAW_FRAME_BUFFER_COLS,
            EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
            out_buf,
            img_width,
            img_height);
    }


    return true;
}


void sendData(String label, float confidence) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;


    http.begin(supabaseUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", supabaseKey);
    http.addHeader("Authorization", "Bearer " + String(supabaseKey));
    http.addHeader("Prefer", "return=minimal");


    // JSON payload
    String json = "{";
    json += "\"trash_label\":\"" + label + "\",";
    json += "\"confidence\":" + String(confidence);
    json += "}";


    int httpResponseCode = http.POST(json);


    Serial.print("Response code: ");
    Serial.println(httpResponseCode);


    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}




