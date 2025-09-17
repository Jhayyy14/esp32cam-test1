# ESP32 Camera Web Server AI - Usage Examples

This file demonstrates the expected output and behavior of the ESP32 Camera Web Server with Edge Impulse AI Detection.

## Serial Output Example

When the ESP32-CAM starts up, you should see output similar to this in the Serial Monitor:

```
ESP32 Camera Web Server with Edge Impulse AI Detection
Camera model: AI_THINKER selected
Initializing camera...
Camera initialized successfully
PSRAM found: 4MB
WiFi connecting to: Yohan
WiFi connected!
IP address: 192.168.1.100
Edge Impulse Inferencing Demo
Starting web server on port: 80
Starting stream server on port: 81
Camera Ready! Use 'http://192.168.1.100' to connect
```

## Web Interface Behavior

### Main Interface (http://192.168.1.100)
- Displays live camera feed
- Shows detection results overlaid on video
- Control buttons for camera settings

### Stream Endpoint (http://192.168.1.100/stream)
- MJPEG video stream
- Real-time detection boxes and labels
- Frame rate information

### Capture Endpoint (http://192.168.1.100/capture)
- Single JPEG photo capture
- Includes detection results if objects are detected

## Detection Output Examples

### Object Detection (Console Output)
```
MJPG: 15234B 45ms (22.2fps), DETECTED
Edge Impulse Inference Results:
  - person (0.89) [x: 120, y: 50, width: 180, height: 220]
  - car (0.76) [x: 45, y: 180, width: 90, height: 65]

MJPG: 14890B 42ms (23.8fps)
MJPG: 15100B 46ms (21.7fps), DETECTED
Edge Impulse Inference Results:
  - person (0.92) [x: 125, y: 48, width: 175, height: 225]
```

### Visual Detection Results
- Blue bounding boxes around detected objects
- Red text labels with confidence scores
- Real-time overlay on video stream

## Performance Characteristics

### Typical Frame Rates
- **Without AI Detection**: 25-30 fps at QVGA resolution
- **With AI Detection**: 15-20 fps at QVGA resolution  
- **Higher Resolutions**: 5-10 fps depending on resolution and detection complexity

### Memory Usage
- **PSRAM Required**: Yes, for frame buffers and inference
- **Heap Usage**: ~200KB for inference, ~150KB for web server
- **Frame Buffer**: 2 buffers in PSRAM for smooth streaming

### Network Performance
- **Local Network**: 15-25 fps sustained
- **WiFi Range**: Affects frame rate and latency
- **Multiple Clients**: Frame rate decreases with more viewers

## Troubleshooting Common Issues

### Camera Issues
```
Camera init failed with error 0x20001
Solution: Check camera connections and PSRAM configuration
```

### Memory Issues  
```
out_buf malloc failed
Solution: Ensure PSRAM is enabled and sufficient heap memory
```

### WiFi Issues
```
WiFi connection timeout
Solution: Check SSID/password and signal strength
```

### Detection Issues
```
Edge Impulse inference failed
Solution: Verify model is properly compiled and linked
```

## API Endpoints

### GET /
Main interface with live camera feed and controls

### GET /stream  
MJPEG video stream with detection overlays
- Content-Type: multipart/x-mixed-replace
- Real-time detection results

### GET /capture
Single photo capture with detection results
- Content-Type: image/jpeg
- Includes X-Timestamp header

### GET /bmp
Bitmap format image capture
- Content-Type: image/x-windows-bmp

## Configuration Options

### Camera Settings
```cpp
// Frame size options
FRAMESIZE_QQVGA    // 160x120
FRAMESIZE_QVGA     // 320x240 (recommended for AI)
FRAMESIZE_CIF      // 352x288
FRAMESIZE_VGA      // 640x480
FRAMESIZE_SVGA     // 800x600
```

### AI Detection Settings
```cpp
// Model input resolution
EI_CLASSIFIER_INPUT_WIDTH   // 320 (default)
EI_CLASSIFIER_INPUT_HEIGHT  // 240 (default)

// Detection threshold (in model)
DETECTION_THRESHOLD         // 0.5 (typical)
```

### Network Settings
```cpp
// Server ports
HTTP_SERVER_PORT = 80       // Main interface
STREAM_SERVER_PORT = 81     // Video stream
```

## Expected File Structure

After compilation, the project should include:
- ESP32CamWebServerAI.ino (main sketch)
- app_httpd.cpp (web server implementation)
- camera_pins.h (pin definitions)
- camera_index.h (web interface)
- tets3_inferencing library files
- Edge Impulse SDK files

## Power Consumption

### Typical Power Usage
- **Idle (WiFi connected)**: ~180mA
- **Streaming only**: ~220-250mA  
- **Streaming + AI detection**: ~280-320mA
- **Peak during inference**: ~400mA

Recommend 5V 2A power supply for stable operation.