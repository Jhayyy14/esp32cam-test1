# ESP32 Camera Web Server with Edge Impulse AI Detection

This project combines the ESP32 Camera Web Server functionality with Edge Impulse AI inference for object detection on an AI Thinker ESP32-CAM board.

## Features

- **Web-based Camera Stream**: Live MJPEG video streaming through a web interface
- **Edge Impulse AI Detection**: Real-time object detection using Edge Impulse trained models
- **Visual Detection Overlay**: Detection results are overlaid on the video stream with bounding boxes and labels
- **AI Thinker ESP32-CAM Support**: Optimized for the popular AI Thinker ESP32-CAM module

## Hardware Requirements

- AI Thinker ESP32-CAM module with OV2640 camera
- ESP32-CAM board with PSRAM (required for higher resolution and inference)
- MicroSD card (optional)
- USB-to-serial adapter for programming

## Software Requirements

- Arduino IDE with ESP32 board support
- tets3_inferencing library (Edge Impulse generated library)
- ESP32 Camera library
- WiFi library

## Installation

1. **Install Arduino IDE and ESP32 Support**:
   - Install Arduino IDE
   - Add ESP32 board manager URL: `https://dl.espressif.com/dl/package_esp32_index.json`
   - Install ESP32 by Espressif Systems

2. **Install Required Libraries**:
   - The tets3_inferencing library should be in the same project directory
   - Other required libraries (WiFi, esp_camera) are included with ESP32 core

3. **Board Configuration**:
   - Board: "AI Thinker ESP32-CAM"
   - Partition Scheme: "Huge APP (3MB No OTA/1MB SPIFFS)"
   - PSRAM: "Enabled"

## Configuration

1. **WiFi Credentials**: Update the following lines in `ESP32CamWebServerAI.ino`:
   ```cpp
   const char* ssid = "YourWiFiSSID";
   const char* password = "YourWiFiPassword";
   ```

2. **Camera Model**: The code is configured for AI Thinker ESP32-CAM. If using a different board, modify the camera model definition in `ESP32CamWebServerAI.ino`:
   ```cpp
   #define CAMERA_MODEL_AI_THINKER
   ```

## Usage

1. **Upload the Code**:
   - Connect ESP32-CAM to computer via USB-to-serial adapter
   - Select the correct board and port in Arduino IDE
   - Upload the sketch

2. **Access the Web Interface**:
   - Open Serial Monitor to see the assigned IP address
   - Open a web browser and navigate to the displayed IP address
   - The interface provides:
     - Live camera stream at `/stream`
     - Single photo capture at `/capture`
     - Main interface at `/`

3. **View AI Detection Results**:
   - Detection results are displayed in real-time on the video stream
   - Bounding boxes and labels appear when objects are detected
   - Detection confidence scores are shown

## File Structure

```
ESP32CamWebServerAI/
├── ESP32CamWebServerAI.ino    # Main Arduino sketch
├── app_httpd.cpp              # Web server and streaming logic with AI integration
├── camera_pins.h              # Camera pin definitions for different boards
├── camera_index.h             # Web interface HTML (compressed)
└── README.md                  # This file
```

## Key Features Implemented

### Camera Integration
- Supports AI Thinker ESP32-CAM pin configuration
- PSRAM utilization for larger frame buffers
- Multiple pixel formats (JPEG, RGB565, RGB888)
- Adjustable frame size and quality

### Edge Impulse Integration
- Real-time inference on camera frames
- Configurable input resolution (320x240 default)
- Object detection with bounding boxes
- Support for multiple object classes

### Web Server Features
- MJPEG streaming endpoint
- Single photo capture
- Real-time detection overlay
- Cross-origin resource sharing (CORS) support

## Technical Details

### Camera Configuration
- Uses OV2640 camera sensor
- Frame size: QVGA (320x240) for inference, higher for streaming
- PSRAM allocation for efficient memory usage
- JPEG compression for web streaming

### AI Inference Pipeline
1. Capture frame from camera
2. Convert to RGB888 format
3. Resize/crop to model input size
4. Run Edge Impulse inference
5. Draw detection results
6. Convert back to JPEG for streaming

### Web Streaming
- Multipart HTTP streaming
- Real-time detection overlays
- Configurable frame rate
- LED flash support

## Troubleshooting

1. **Camera Init Failed**: Check camera connections and PSRAM configuration
2. **WiFi Connection Issues**: Verify SSID and password
3. **Low Frame Rate**: Reduce resolution or disable detection temporarily
4. **Out of Memory**: Ensure PSRAM is enabled and partition scheme allows enough space
5. **Detection Not Working**: Verify Edge Impulse library is properly installed

## Performance Notes

- Detection runs on smaller frames (320x240) for performance
- Higher resolution streaming is available when detection is disabled
- Frame rate depends on inference complexity and network conditions
- PSRAM is essential for stable operation

## License

This project combines code from ESP32 Camera examples and Edge Impulse libraries. Please refer to respective licenses.