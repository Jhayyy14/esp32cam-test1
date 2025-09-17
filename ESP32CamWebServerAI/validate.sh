#!/bin/bash

# ESP32 Camera Web Server AI - Code Validation Script
# This script validates the code structure and identifies potential issues

echo "=== ESP32 Camera Web Server AI - Code Validation ==="
echo

# Check if all required files exist
echo "1. Checking required files..."
files=(
    "ESP32CamWebServerAI.ino"
    "app_httpd.cpp" 
    "camera_pins.h"
    "camera_index.h"
    "README.md"
)

for file in "${files[@]}"; do
    if [ -f "$file" ]; then
        echo "✓ $file exists"
    else
        echo "✗ $file missing"
    fi
done
echo

# Check for required includes
echo "2. Checking critical includes..."
includes=(
    "#include \"esp_camera.h\""
    "#include <WiFi.h>"
    "#include <tets3_inferencing.h>"
)

for include in "${includes[@]}"; do
    if grep -q "$include" ESP32CamWebServerAI.ino; then
        echo "✓ Found: $include"
    else
        echo "✗ Missing: $include"
    fi
done
echo

# Check for AI Thinker camera model
echo "3. Checking camera model configuration..."
if grep -q "#define CAMERA_MODEL_AI_THINKER" ESP32CamWebServerAI.ino; then
    echo "✓ AI_THINKER camera model selected"
else
    echo "✗ AI_THINKER camera model not selected"
fi
echo

# Check for Edge Impulse constants
echo "4. Checking Edge Impulse constants..."
constants=(
    "EI_CAMERA_RAW_FRAME_BUFFER_COLS"
    "EI_CAMERA_RAW_FRAME_BUFFER_ROWS"
    "EI_CAMERA_FRAME_BYTE_SIZE"
)

for constant in "${constants[@]}"; do
    if grep -q "$constant" ESP32CamWebServerAI.ino; then
        echo "✓ Found: $constant"
    else
        echo "✗ Missing: $constant"
    fi
done
echo

# Check for critical functions
echo "5. Checking critical function definitions..."
functions=(
    "bool ei_camera_init"
    "bool ei_camera_capture"
    "int ei_camera_get_data"
    "void startCameraServer"
)

for function in "${functions[@]}"; do
    if grep -q "$function" ESP32CamWebServerAI.ino; then
        echo "✓ Found: $function"
    else
        echo "✗ Missing: $function"
    fi
done
echo

# Check app_httpd.cpp for Edge Impulse integration
echo "6. Checking Edge Impulse integration in web server..."
if grep -q "CONFIG_EDGE_IMPULSE_ENABLED" app_httpd.cpp; then
    echo "✓ Edge Impulse integration enabled"
else
    echo "✗ Edge Impulse integration not found"
fi

if grep -q "run_classifier" app_httpd.cpp; then
    echo "✓ Found run_classifier call"
else
    echo "✗ Missing run_classifier call"
fi

if grep -q "draw_detection_boxes" app_httpd.cpp; then
    echo "✓ Found detection box drawing"
else
    echo "✗ Missing detection box drawing"
fi
echo

# Check for potential issues
echo "7. Checking for potential issues..."

# Check for proper memory management
if grep -q "malloc.*snapshot_buf" app_httpd.cpp && grep -q "free.*snapshot_buf" app_httpd.cpp; then
    echo "✓ Proper memory management for snapshot_buf"
else
    echo "⚠ Potential memory leak in snapshot_buf"
fi

# Check for proper camera frame buffer handling
if grep -q "esp_camera_fb_return" app_httpd.cpp; then
    echo "✓ Camera frame buffer properly returned"
else
    echo "⚠ Camera frame buffer may not be properly released"
fi

# Check WiFi credentials placeholder
if grep -q "Yohan" ESP32CamWebServerAI.ino; then
    echo "⚠ WiFi credentials still using placeholder values"
else
    echo "✓ WiFi credentials configured"
fi
echo

# Code statistics
echo "8. Code statistics..."
echo "Lines in main sketch: $(wc -l < ESP32CamWebServerAI.ino)"
echo "Lines in web server: $(wc -l < app_httpd.cpp)"
echo "Total includes: $(grep -c "#include" ESP32CamWebServerAI.ino)"
echo "Function definitions: $(grep -c "void\|bool\|int.*(" ESP32CamWebServerAI.ino)"
echo

echo "=== Validation Complete ==="
echo
echo "Key Features Implemented:"
echo "• ESP32-CAM web server with live streaming"
echo "• Edge Impulse AI inference integration"  
echo "• Real-time detection overlay on video stream"
echo "• AI Thinker ESP32-CAM board support"
echo "• PSRAM utilization for performance"
echo "• Configurable detection and streaming"
echo
echo "Next Steps:"
echo "1. Install Arduino IDE with ESP32 support"
echo "2. Copy tets3_inferencing library to Arduino libraries"
echo "3. Configure WiFi credentials"
echo "4. Select 'AI Thinker ESP32-CAM' board with PSRAM enabled"
echo "5. Upload and test on hardware"