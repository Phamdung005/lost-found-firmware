# Sơ đồ chân Phần cứng (Pinout Specification) — FindBox ESP32

## 1. Tổng quan phân bổ chân ESP32 DevKit V1 (30 Chân)

| Chân ESP32 | Hướng (I/O) | Thiết bị kết nối | Chức năng chi tiết |
| :---: | :---: | :--- | :--- |
| **GPIO 18** | Output | **Servo Ngăn A01** | Xung PWM điều khiển góc quay mở/khóa chốt A01 |
| **GPIO 19** | Output | **Servo Ngăn A02** | Xung PWM điều khiển góc quay mở/khóa chốt A02 |
| **GPIO 4** | Input | **Cảm biến từ A01** | Phát hiện đóng/mở cửa ngăn A01 (Kéo nội trở PULLUP) |
| **GPIO 5** | Input | **Cảm biến từ A02** | Phát hiện đóng/mở cửa ngăn A02 (Kéo nội trở PULLUP) |
| **GPIO 21** | I/O | **LCD 1602 (I2C SDA)** | Đường truyền dữ liệu I2C |
| **GPIO 22** | Output | **LCD 1602 (I2C SCL)** | Xung nhịp đồng hồ I2C |
| **GPIO 13** | Output | **Keypad ROW 1** | Quét hàng 1 của bàn phím ma trận |
| **GPIO 12** | Output | **Keypad ROW 2** | Quét hàng 2 của bàn phím ma trận |
| **GPIO 14** | Output | **Keypad ROW 3** | Quét hàng 3 của bàn phím ma trận |
| **GPIO 27** | Output | **Keypad ROW 4** | Quét hàng 4 của bàn phím ma trận |
| **GPIO 26** | Input | **Keypad COL 1** | Đọc tín hiệu cột 1 (1, 4, 7, *) |
| **GPIO 25** | Input | **Keypad COL 2** | Đọc tín hiệu cột 2 (2, 5, 8, 0) |
| **GPIO 33** | Input | **Keypad COL 3** | Đọc tín hiệu cột 3 (3, 6, 9, #) |
| **GPIO 32** | Input | **Keypad COL 4** | Đọc tín hiệu cột 4 (A, B, C, D) |

---

## 2. Lưu ý An toàn Điện & Chống Nhiễu

1. **Nguồn cấp Servo độc lập:**
   - Động cơ Servo SG90 tiêu thụ dòng đỉnh (stall current) lên đến 500mA - 800mA khi kích hoạt kéo chốt.
   - Khi chạy 2 Servo cùng lúc, tổng dòng có thể vượt quá 1.5A. Tuyệt đối **không lấy nguồn 5V trực tiếp từ chân VIN của ESP32** để nuôi Servo vì sẽ kích hoạt bảo vệ sụt áp (*Brownout reset*) khiến ESP32 khởi động lại liên tục.
   - Khuyên dùng: Sử dụng Adapter 5V 2A–3A riêng hoặc module hạ áp LM2596/Buck 5V, **nối chung cực âm (GND)** giữa nguồn ngoài và ESP32.

2. **Chân I2C (LCD 1602):**
   - ESP32 mặc định sử dụng chân GPIO 21 (SDA) và GPIO 22 (SCL). Module chuyển đổi I2C PCF8574 đã tích hợp sẵn trở kéo lên (Pull-up resistors) nên có thể cắm trực tiếp.

