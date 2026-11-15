# Giao thức Truyền thông IoT (Communication Protocol) — FindBox

Hệ thống giao tiếp giữa vi điều khiển ESP32 và máy chủ Backend FindBox thông qua chuẩn **HTTP REST API (JSON)**.

Tất cả các request từ ESP32 đều phải đính kèm Header xác thực thiết bị:
```http
X-Device-Token: esp32_smart_locker_secret_token_2026
Content-Type: application/json
```

---

## 1. Heartbeat & Trạng thái Cửa Realtime
Gửi định kỳ mỗi 10 giây hoặc **ngay lập tức** khi cảm biến cửa phát hiện có sự thay đổi (mở hoặc đóng).

- **Endpoint:** `POST /api/device/heartbeat`
- **Request Body:**
```json
{
  "wifi_rssi": -58,
  "firmware_version": "v1.3.0",
  "slots": [
    { "slot_code": "A01", "door_status": "closed" },
    { "slot_code": "A02", "door_status": "open" }
  ]
}
```
- **Response (200 OK):**
```json
{
  "success": true,
  "message": "Heartbeat acknowledged",
  "server_time": "2026-11-16T00:30:00.000Z"
}
```

---

## 2. Polling Lệnh điều khiển từ Web Admin
ESP32 gửi request mỗi 1.5 giây để kiểm tra xem có lệnh mở khẩn cấp hoặc bảo trì từ Quản trị viên hay không.

- **Endpoint:** `GET /api/device/commands/next`
- **Response khi có lệnh (200 OK):**
```json
{
  "success": true,
  "has_command": true,
  "command": {
    "id": 105,
    "command_type": "unlock_slot",
    "slot_code": "A01",
    "servo_pin": 18
  }
}
```
- **Response khi không có lệnh (200 OK):**
```json
{
  "success": true,
  "has_command": false
}
```

---

## 3. Xác thực mã OTP mở tủ tại chỗ (Keypad)
Khi người dùng bấm mã 6 số và nhấn phím `#` trên bàn phím ma trận.

- **Endpoint:** `POST /api/device/verify-otp`
- **Request Body:**
```json
{
  "otp": "839201"
}
```
- **Response khi mã hợp lệ (200 OK):**
```json
{
  "success": true,
  "message": "OTP hợp lệ",
  "data": {
    "slot_code": "A01",
    "servo_pin": 18,
    "item_name": "Ví da màu đen"
  }
}
```
- **Response khi sai mã (400 Bad Request):**
```json
{
  "success": false,
  "message": "Mã OTP không chính xác hoặc đã hết hạn"
}
```

---

## 4. Tải cấu hình ngăn tủ khi khởi động
- **Endpoint:** `GET /api/device/config`
- **Response (200 OK):**
```json
{
  "success": true,
  "locker_code": "L-001",
  "slots": [
    { "slot_code": "A01", "hardware_enabled": true, "servo_pin": 18, "sensor_pin": 4 },
    { "slot_code": "A02", "hardware_enabled": true, "servo_pin": 19, "sensor_pin": 5 }
  ]
}
```

