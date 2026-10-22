#ifndef CONFIG_H
#define CONFIG_H

// Cấu hình Wi-Fi
#define WIFI_SSID "Your_WiFi_Name"
#define WIFI_PASSWORD "Your_WiFi_Password"

// Cấu hình API Backend FindBox (Thay <YOUR_PC_IP> bằng IP máy tính chạy backend, ví dụ: 192.168.1.50)
#define BACKEND_BASE_URL "http://192.168.1.50:3000/api/device"
#define DEVICE_TOKEN "esp32_smart_locker_secret_token_2026"

// Cấu hình GPIO điều khiển Servo SG90
#define SERVO_A01_PIN 18  // Ngăn A01
#define SERVO_A02_PIN 19  // Ngăn A02

// Cấu hình GPIO Cảm biến cửa từ (Magnetic Reed Switch)
#define SENSOR_A01_PIN 22 // Cảm biến cửa A01
#define SENSOR_A02_PIN 23 // Cảm biến cửa A02

// Góc mở và khóa Servo
#define SERVO_LOCK_ANGLE 0
#define SERVO_UNLOCK_ANGLE 90
#define SERVO_AUTO_LOCK_DELAY_MS 5000 // Tự động khóa lại sau 5 giây

// Chu kỳ Polling và Heartbeat
#define HEARTBEAT_INTERVAL_MS 10000   // Gửi heartbeat mỗi 10 giây
#define COMMAND_POLL_INTERVAL_MS 1500 // Kiểm tra lệnh mỗi 1.5 giây

#endif
