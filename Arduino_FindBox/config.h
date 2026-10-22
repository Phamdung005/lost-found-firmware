#ifndef CONFIG_H
#define CONFIG_H

// Cấu hình Wi-Fi
#define WIFI_SSID "NguyenVanChien"
#define WIFI_PASSWORD "99999999"

// Cấu hình API Backend FindBox (Thay bằng URL Backend Render của bạn khi nạp code)
#define BACKEND_BASE_URL "https://lost-found-app-ehkf.onrender.com/api/device"
#define DEVICE_TOKEN "esp32_smart_locker_secret_token_2026"

// Cấu hình GPIO điều khiển Servo SG90 (Mở chốt khóa)
#define SERVO_A01_PIN 18  // Ngăn A01
#define SERVO_A02_PIN 19  // Ngăn A02

// Cấu hình GPIO Cảm biến cửa từ (Chuyển sang chân 4 và 5 để nhường chân 21, 22 cho I2C LCD)
#define SENSOR_A01_PIN 4  // Cảm biến cửa A01
#define SENSOR_A02_PIN 5  // Cảm biến cửa A02

// Cấu hình Màn hình LCD 1602 I2C
#define LCD_I2C_ADDR 0x3F   // Địa chỉ I2C thông dụng (0x27 hoặc 0x3F)
#define LCD_COLS 16
#define LCD_ROWS 2
#define LCD_SDA_PIN 21     // Chân SDA mặc định ESP32
#define LCD_SCL_PIN 22     // Chân SCL mặc định ESP32

// Cấu hình Bàn phím Ma trận Keypad 4x4
// 4 Chân Hàng (Rows: R1, R2, R3, R4)
#define KEYPAD_ROW_1 13
#define KEYPAD_ROW_2 12
#define KEYPAD_ROW_3 14
#define KEYPAD_ROW_4 27

// 4 Chân Cột (Cols: C1, C2, C3, C4)
#define KEYPAD_COL_1 26
#define KEYPAD_COL_2 25
#define KEYPAD_COL_3 33
#define KEYPAD_COL_4 32

// Góc mở và khóa Servo (Mở 90 độ, Khóa 0 độ)
#define SERVO_LOCK_ANGLE 0
#define SERVO_UNLOCK_ANGLE 90
#define SERVO_AUTO_LOCK_DELAY_MS 5000 // (Dùng khi không có cảm biến đóng)

// Chu kỳ Polling và Heartbeat
#define HEARTBEAT_INTERVAL_MS 10000   // Gửi heartbeat mỗi 10 giây
#define COMMAND_POLL_INTERVAL_MS 1500 // Kiểm tra lệnh mỗi 1.5 giây

#endif
