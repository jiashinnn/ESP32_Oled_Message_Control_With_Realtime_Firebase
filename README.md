# ESP32 OLED Message Control

A complete end-to-end IoT noticeboard system using an ESP32, OLED display, EEPROM, and Firebase Realtime Database.  Staff can log in to a web dashboard to update the message, and the ESP32 will display the latest text on its OLED in real time.

---

## 📦 Features

- **Persistent Wi-Fi & Device ID** stored in EEPROM  
- **Access-Point Configuration Portal** for entering SSID, password, and device ID  
- **Firebase Authentication** (email/password) and Realtime Database integration  
- **OLED Display** (SSD1306) shows:
  - Device ID
  - Current message
- **Responsive Web Dashboard** (HTML/CSS/JS) for staff to login/register, then update message  

---

## 🔧 Hardware

- ESP32 Dev Board  
- SSD1306 0.91″ OLED Display (I²C)  
- (Optional) Push-button on GPIO0 for forcing AP mode / clearing EEPROM  
- Wires and breadboard  

---

## 🖥️ Software Components

1. **ESP32 Firmware**  
   - `esp32_dht_oled_eeprom_firebase.ino`  
   - Handles Wi-Fi/EERPOM/Firebase/OLED  
2. **Web Dashboard** (in the `dashboard/` folder)  
   - `index.html` (Home)  
   - `login.html`, `register.html`, `reset-password.html`  
   - `firebase.js` (contains your Firebase config keys)
   - `dashboard.html`

---

## 📸 Screenshots

### Configuration Portal  
<details>
  <summary>Click to expand</summary>

  ![image](https://github.com/user-attachments/assets/44234089-7a54-4f05-bd1a-f86922e7de25)


  *Enter SSID, password & Device ID in the captive-portal UI.*  
</details>

### Web Dashboard  
<details>
  <summary>Click to expand</summary>

| **Page & Description**                   | **Screenshot**                                    |
| ---------------------------------------- | ------------------------------------------------- |
| **Home**<br><sub>`index.html` landing page</sub>            | ![Home Page](https://github.com/user-attachments/assets/0a09337b-026b-46b7-b126-89586d29d4f4)                   |
| **Login**<br><sub>User login form</sub>                    | ![Login Page](https://github.com/user-attachments/assets/2409abb3-515d-4250-aa03-ca7ec1f84760)                  |
| **Register**<br><sub>New user sign-up</sub>                | ![Register Page](https://github.com/user-attachments/assets/30826594-a5b8-4e32-a4f6-132d18d3d735)            |
| **Reset Password**<br><sub>Password recovery</sub>         | ![Reset Password](https://github.com/user-attachments/assets/ab677e84-ad0a-4737-bbd1-a213d513fd95)     |
| **Dashboard**<br><sub>Message editor & settings</sub>     | ![Dashboard](https://github.com/user-attachments/assets/da6fc280-3c72-4e42-8db9-be844bcb851c)              |

  *Staff login page and real-time message editor.*  
</details>

---

## 🚀 Getting Started

### 1. Clone this repository

```bash
git clone https://github.com/<your-user>/ESP32_Oled_Message_Control.git
cd ESP32_Oled_Message_Control
```
>Or add it via GitHub Desktop as a new local repo.

### 2. Firebase Setup
1. Go to the Firebase Console
2. Create a project (e.g. esp-oled-demo)
3. Enable Email/Password sign-in under Authentication → Sign-in Method
4. Create a Realtime Database, set read/write rules for authenticated users:
```
{
  "rules": {
    "102": {
      "oled": {
        ".read":  true,
        ".write": "auth != null"
      }
    }
  }
}
```
5. Copy your web config (`apiKey`, `authDomain`, `databaseURL`, etc.) into `firebase.js`:
```
export const firebaseConfig = {
  apiKey: "AIzaSy…",
  authDomain: "esp-oled-demo.firebaseapp.com",
  databaseURL: "https://esp-oled-demo.firebaseio.com",
  // …
};
```

### 3. Prepare the Web Dashboard
- Open index.html in your browser to register/login.
- After login, you’ll see a simple UI to update the message.

### 4. Wire & Configure the ESP32
1. Connections
- **OLED SDA** → `GPIO21`
- **OLED SCL** → `GPIO22`
- **OLED VCC** → `3.3V`
- **OLED GND** → `GND`
2. Fill in `esp32_dht_oled_eeprom_firebase.ino`
```
#define API_KEY       "AIzaSy…"
#define DATABASE_URL  "https://esp-oled-demo.firebaseio.com"
// initial user login:
auth.user.email    = "email";
auth.user.password = "password";
```
3. Upload Firmware
- Open the .ino in Arduino IDE
- Select Board > ESP32 Dev Module and correct COM port
- Click Upload

### 5. First Boot & Configuration
- On first boot (or if no SSID stored), ESP32 creates an AP called ESP32_Config.
- Connect your phone/laptop to ESP32_Config, open http://192.168.4.1
- You’ll see a modern UI showing current SSID/Pass/DevID, form to update them, and a network scan.
- Enter your home/office Wi-Fi credentials and a Device ID (e.g. 102), click Save Changes.
- ESP32 will reboot, join your Wi-Fi, then authenticate with Firebase and begin polling /102/oled.

---

## 📜 License
> by Lim Jia Shin
> STTHK3113 Sensor-Based Systems
