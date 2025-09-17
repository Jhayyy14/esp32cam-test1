// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "esp32-hal-ledc.h"
#include "sdkconfig.h"
#include "camera_index.h"

// Include Edge Impulse headers
#include <tets3_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#endif

// External Edge Impulse functions
extern bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf);
extern int ei_camera_get_data(size_t offset, size_t length, float *out_ptr);
extern uint8_t *snapshot_buf;

// Enable Edge Impulse detection instead of face detection
#define CONFIG_EDGE_IMPULSE_ENABLED 1

#if CONFIG_LED_ILLUMINATOR_ENABLED

#define LED_LEDC_CHANNEL 2 // Using different ledc channel/timer than camera
#define CONFIG_LED_MAX_INTENSITY 255

int led_duty = 0;
bool isStreaming = false;

#endif

typedef struct
{
    httpd_req_t *req;
    size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

static void rgb_print(fb_data_t *fb, uint32_t color, const char *str)
{
    fb_gfx_print(fb, (fb->width - (strlen(str) * 14)) / 2, 10, color, str);
}

static int rgb_printf(fb_data_t *fb, uint32_t color, const char *format, ...)
{
    char loc_buf[64];
    char *temp = loc_buf;
    int len;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    len = vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
    va_end(copy);
    if (len >= sizeof(loc_buf))
    {
        temp = (char *)malloc(len + 1);
        if (temp == NULL)
        {
            return 0;
        }
    }
    vsnprintf(temp, len + 1, format, arg);
    va_end(arg);
    rgb_print(fb, color, temp);
    if (len > 64)
    {
        free(temp);
    }
    return len;
}

// Function to draw Edge Impulse detection boxes
static void draw_detection_boxes(fb_data_t *fb, ei_impulse_result_t *result)
{
#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    for (uint32_t i = 0; i < result->bounding_boxes_count; i++) {
        ei_impulse_result_bounding_box_t bb = result->bounding_boxes[i];
        if (bb.value == 0) {
            continue;
        }
        
        // Draw bounding box
        fb_gfx_drawFastHLine(fb, bb.x, bb.y, bb.width, 0x0000FF); // Top line - blue
        fb_gfx_drawFastHLine(fb, bb.x, bb.y + bb.height, bb.width, 0x0000FF); // Bottom line
        fb_gfx_drawFastVLine(fb, bb.x, bb.y, bb.height, 0x0000FF); // Left line
        fb_gfx_drawFastVLine(fb, bb.x + bb.width, bb.y, bb.height, 0x0000FF); // Right line
        
        // Draw label and confidence
        char label_text[64];
        snprintf(label_text, sizeof(label_text), "%s %.2f", bb.label, bb.value);
        fb_gfx_print(fb, bb.x + 2, bb.y + 2, 0xFF0000, label_text); // Red text
    }
#endif
}

#if CONFIG_LED_ILLUMINATOR_ENABLED
void enable_led(bool en)
{
    int duty = en ? led_duty : 0;
    if (en && isStreaming && (led_duty > CONFIG_LED_MAX_INTENSITY))
    {
        duty = CONFIG_LED_MAX_INTENSITY;
    }
    ledcWrite(LED_LEDC_CHANNEL, duty);
}
#endif

static esp_err_t bmp_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    int64_t fr_start = esp_timer_get_time();
#endif

#if CONFIG_LED_ILLUMINATOR_ENABLED
    enable_led(true);
    vTaskDelay(150 / portTICK_PERIOD_MS); // The LED needs to be turned on ~150ms before the call to esp_camera_fb_get()
    fb = esp_camera_fb_get();             // or it won't be visible in the frame. A better way to do this is needed.
    enable_led(false);
#else
    fb = esp_camera_fb_get();
#endif

    if (!fb)
    {
        log_e("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/x-windows-bmp");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.bmp");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char ts[32];
    snprintf(ts, 32, "%ju.%06ju", (uintmax_t)fb->timestamp.tv_sec, (uintmax_t)fb->timestamp.tv_usec);
    httpd_resp_set_hdr(req, "X-Timestamp", (const char *)ts);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    bool converted = frame2bmp(fb, &buf, &buf_len);
    esp_camera_fb_return(fb);
    if (!converted)
    {
        log_e("BMP Conversion failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    res = httpd_resp_send(req, (const char *)buf, buf_len);
    free(buf);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    int64_t fr_end = esp_timer_get_time();
#endif
    log_i("BMP: %uB %ums", (uint32_t)(buf_len), (uint32_t)((fr_end - fr_start) / 1000));
    return res;
}

static size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len)
{
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if (!index)
    {
        j->len = 0;
    }
    if (httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK)
    {
        return 0;
    }
    j->len += len;
    return len;
}

static esp_err_t capture_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    int64_t fr_start = esp_timer_get_time();
#endif

#if CONFIG_LED_ILLUMINATOR_ENABLED
    enable_led(true);
    vTaskDelay(150 / portTICK_PERIOD_MS); // The LED needs to be turned on ~150ms before the call to esp_camera_fb_get()
    fb = esp_camera_fb_get();             // or it won't be visible in the frame. A better way to do this is needed.
    enable_led(false);
#else
    fb = esp_camera_fb_get();
#endif

    if (!fb)
    {
        log_e("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char ts[32];
    snprintf(ts, 32, "%ju.%06ju", (uintmax_t)fb->timestamp.tv_sec, (uintmax_t)fb->timestamp.tv_usec);
    httpd_resp_set_hdr(req, "X-Timestamp", (const char *)ts);

    size_t out_len, out_width, out_height;
    uint8_t *out_buf;
    bool s;
    size_t fb_len = 0;
    if (fb->format == PIXFORMAT_JPEG)
    {
        fb_len = fb->len;
        res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    }
    else
    {
        jpg_chunking_t jchunk = {req, 0};
        res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk) ? ESP_OK : ESP_FAIL;
        httpd_resp_send_chunk(req, NULL, 0);
        fb_len = jchunk.len;
    }
    esp_camera_fb_return(fb);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    int64_t fr_end = esp_timer_get_time();
#endif
    log_i("JPG: %uB %ums", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start) / 1000));
    return res;
}

static esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    struct timeval _timestamp;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char *part_buf[128];

#if CONFIG_EDGE_IMPULSE_ENABLED
    // Edge Impulse variables
    static bool detection_enabled = true;
    bool detected = false;
    ei_impulse_result_t result = { 0 };
    size_t out_len = 0, out_width = 0, out_height = 0;
    uint8_t *out_buf = NULL;
    bool s = false;
#endif

    static int64_t last_frame = 0;
    if (!last_frame)
    {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK)
    {
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "60");

#if CONFIG_LED_ILLUMINATOR_ENABLED
    isStreaming = true;
    enable_led(true);
#endif

    while (true)
    {
#if CONFIG_EDGE_IMPULSE_ENABLED
        detected = false;
#endif

        fb = esp_camera_fb_get();
        if (!fb)
        {
            log_e("Camera capture failed");
            res = ESP_FAIL;
        }
        else
        {
            _timestamp.tv_sec = fb->timestamp.tv_sec;
            _timestamp.tv_usec = fb->timestamp.tv_usec;

#if CONFIG_EDGE_IMPULSE_ENABLED
            if (!detection_enabled || fb->width > 400)
            {
#endif
                if (fb->format != PIXFORMAT_JPEG)
                {
                    bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                    esp_camera_fb_return(fb);
                    fb = NULL;
                    if (!jpeg_converted)
                    {
                        log_e("JPEG compression failed");
                        res = ESP_FAIL;
                    }
                }
                else
                {
                    _jpg_buf_len = fb->len;
                    _jpg_buf = fb->buf;
                }
#if CONFIG_EDGE_IMPULSE_ENABLED
            }
            else
            {
                // Run Edge Impulse inference on smaller frames
                out_len = fb->width * fb->height * 3;
                out_width = fb->width;
                out_height = fb->height;
                out_buf = (uint8_t*)malloc(out_len);
                
                if (!out_buf) {
                    log_e("out_buf malloc failed");
                    res = ESP_FAIL;
                } else {
                    // Convert frame to RGB888
                    s = fmt2rgb888(fb->buf, fb->len, fb->format, out_buf);
                    esp_camera_fb_return(fb);
                    fb = NULL;
                    
                    if (!s) {
                        free(out_buf);
                        log_e("To rgb888 failed");
                        res = ESP_FAIL;
                    } else {
                        fb_data_t rfb;
                        rfb.width = out_width;
                        rfb.height = out_height;
                        rfb.data = out_buf;
                        rfb.bytes_per_pixel = 3;
                        rfb.format = FB_BGR888;

                        // Run Edge Impulse inference
                        snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
                        
                        if (snapshot_buf != nullptr) {
                            // Copy the RGB888 data to snapshot buffer and resize if needed
                            size_t copy_len = out_len;
                            size_t max_len = EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE;
                            if (copy_len > max_len) copy_len = max_len;
                            memcpy(snapshot_buf, out_buf, copy_len);
                            
                            ei::signal_t signal;
                            signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
                            signal.get_data = &ei_camera_get_data;

                            EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
                            if (err == EI_IMPULSE_OK) {
                                detected = (result.bounding_boxes_count > 0);
                                if (detected) {
                                    draw_detection_boxes(&rfb, &result);
                                }
                            }
                            free(snapshot_buf);
                            snapshot_buf = nullptr;
                        }

                        // Convert back to JPEG
                        s = fmt2jpg(out_buf, out_len, out_width, out_height, PIXFORMAT_RGB888, 90, &_jpg_buf, &_jpg_buf_len);
                        free(out_buf);
                        if (!s) {
                            log_e("fmt2jpg failed");
                            res = ESP_FAIL;
                        }
                    }
                }
            }
#endif
        }
        
        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if (res == ESP_OK)
        {
            size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if (fb)
        {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        }
        else if (_jpg_buf)
        {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if (res != ESP_OK)
        {
            break;
        }
        int64_t fr_end = esp_timer_get_time();

        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;

#if CONFIG_EDGE_IMPULSE_ENABLED
        log_i("MJPG: %uB %ums (%.1ffps), %s", (uint32_t)(_jpg_buf_len),
              (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time, detected ? "DETECTED" : "");
#else
        log_i("MJPG: %uB %ums (%.1ffps)", (uint32_t)(_jpg_buf_len),
              (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time);
#endif
    }

#if CONFIG_LED_ILLUMINATOR_ENABLED
    isStreaming = false;
    enable_led(false);
#endif

    return res;
}

static esp_err_t parse_get(httpd_req_t *req, char **obuf)
{
    char *buf = NULL;
    size_t buf_len = 0;

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = (char *)malloc(buf_len);
        if (!buf)
        {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            *obuf = buf;
            return ESP_OK;
        }
        free(buf);
    }
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL)
    {
        const size_t index_html_gz_len = sizeof(index_html_gz);
        return httpd_resp_send(req, (const char *)index_html_gz, index_html_gz_len);
    }
    else
    {
        log_e("Camera sensor not found");
        return httpd_resp_send_500(req);
    }
}

void startCameraServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
#if CONFIG_IDF_TARGET_ESP32S3
    config.server_port += esp_random() % 63532;
#endif

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = NULL};

    httpd_uri_t capture_uri = {
        .uri = "/capture",
        .method = HTTP_GET,
        .handler = capture_handler,
        .user_ctx = NULL};

    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL};

    httpd_uri_t bmp_uri = {
        .uri = "/bmp",
        .method = HTTP_GET,
        .handler = bmp_handler,
        .user_ctx = NULL};

    log_i("Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &capture_uri);
        httpd_register_uri_handler(camera_httpd, &bmp_uri);
    }

    config.server_port += 1;
    config.ctrl_port += 1;
    log_i("Starting stream server on port: '%d'", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}

#if CONFIG_LED_ILLUMINATOR_ENABLED
void setupLedFlash(int pin)
{
    ledcSetup(LED_LEDC_CHANNEL, 5000, 8);
    ledcAttachPin(pin, LED_LEDC_CHANNEL);
}
#else
void setupLedFlash(int pin)
{
    log_i("LED flash is disabled -> CONFIG_LED_ILLUMINATOR_ENABLED = 0");
}
#endif