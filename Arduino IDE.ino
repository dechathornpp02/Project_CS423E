#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

const char* ssid = "ชื่อ WIFI ของตัวเอง"; 
const char* password = "รหัส WIFI ของตัวเอง";
String cloud_url = "https://script.google.com/macros/s/... /exec";

#define TRIG_PIN 26
#define ECHO_PIN 18
#define LASER_PIN 4
#define BUZZER_PIN 23
#define SERVO_PIN 13
#define VIB_PIN 27
#define LDR_PIN 32 
#define IR_PIN 19  

Adafruit_SSD1306 display(128, 64, &Wire, -1);
Servo myServo;
WebServer server(80);

volatile float global_dist = 0.0;
volatile bool door_open_trigger = false;
String global_event = "NORMAL";
int people_count = 0;      
bool last_ir_state = HIGH; 
volatile bool system_armed = true; 

volatile bool vibration_flag = false;
void IRAM_ATTR detectVibration() {
  vibration_flag = true; 
}

void cloudTask(void * pvParameters) {
  for(;;) {
    if (WiFi.status() == WL_CONNECTED) {
      int latched_vib = (global_event.indexOf("VIBRATION") >= 0) ? 1 : 0;

      String json = "{\"device\":\"Museum_Gate\",\"distance\":" + String(global_dist) + 
                    ",\"vibration\":" + String(latched_vib) + 
                    ",\"event\":\"" + global_event + "\"" +
                    ",\"people\":" + String(people_count) + "}";
      HTTPClient http;
      http.begin(cloud_url);
      http.addHeader("Content-Type", "text/plain");
      http.POST(json);
      http.end();
      if(global_event != "NORMAL") global_event = "NORMAL";
    }
    vTaskDelay(10000 / portTICK_PERIOD_MS); 
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(); Wire.setClock(100000);
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  pinMode(LASER_PIN, OUTPUT); pinMode(BUZZER_PIN, OUTPUT); 
  pinMode(VIB_PIN, INPUT); pinMode(LDR_PIN, INPUT);
  pinMode(IR_PIN, INPUT); 
  
  attachInterrupt(digitalPinToInterrupt(VIB_PIN), detectVibration, RISING);
  
  myServo.attach(SERVO_PIN); myServo.write(90); 
  digitalWrite(LASER_PIN, HIGH); 
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); 
    display.clearDisplay(); display.setCursor(0,0); display.setTextColor(WHITE);
    display.println("WiFi Connecting..."); display.display();
  }
  
  
  server.on("/open", []() { door_open_trigger = true; server.send(200, "text/plain", "OK"); });


  server.on("/", []() {
    String status_text = system_armed ? "
    String status_color = system_armed ? "#e53e3e" : "#718096";
    String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1' charset='UTF-8'><link href='https://fonts.googleapis.com/css2?family=Kanit:wght@300;600&display=swap' rel='stylesheet'><style>body{font-family:'Kanit',sans-serif;background:#1a202c;color:white;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;}.card{background:#2d3748;padding:30px;border-radius:20px;width:90%;max-width:400px;text-align:center;box-shadow:0 10px 30px rgba(0,0,0,0.5);}.btn{display:block;width:100%;padding:15px;margin-bottom:15px;border-radius:10px;text-decoration:none;font-weight:bold;font-size:18px;transition:0.3s;box-sizing:border-box;color:white;}.btn-on{background:#38a169;} .btn-on:hover{background:#2f855a;}.btn-off{background:#e53e3e;} .btn-off:hover{background:#c53030;}.btn-reset{background:#d69e2e;} .btn-reset:hover{background:#b7791f;}</style></head><body><div class='card'><h2>🏛️ MUSEUM GATE</h2><p style='color:" + status_color + "; font-weight:bold; margin-bottom: 30px;'>" + status_text + "</p>";
    if(!system_armed) { html += "<a href='/arm' class='btn btn-on'>🔒 เปิดระบบกันขโมย</a>"; } else { html += "<a href='/disarm' class='btn btn-off'>🔓 ปิดระบบกันขโมย</a>"; }
    html += "<a href='/reset' class='btn btn-reset'>🔄 ตัดยอดรายวัน (Reset)</a></div></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/arm", []() { 
    system_armed = true; 
    global_event = "SYSTEM ARMED"; 
    digitalWrite(LASER_PIN, HIGH); 
    server.sendHeader("Location", "/"); 
    server.send(303); 
  });
  
  server.on("/disarm", []() { 
    system_armed = false; 
    global_event = "SYSTEM DISARMED"; 
    digitalWrite(LASER_PIN, LOW); 
    server.sendHeader("Location", "/"); 
    server.send(303); 
  });
  
  server.on("/reset", []() {
    String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1' charset='UTF-8'><link href='https://fonts.googleapis.com/css2?family=Kanit:wght@300;600&display=swap' rel='stylesheet'><style>body{font-family:'Kanit',sans-serif;background:#1a202c;color:white;display:flex;justify-content:center;align-items:center;height:100vh;margin:0;}.card{background:#2d3748;padding:30px;border-radius:20px;width:90%;max-width:400px;text-align:center;border-top:5px solid #ecc94b;}.btn{display:inline-block;padding:12px 20px;border-radius:8px;text-decoration:none;font-weight:bold;margin:5px;}.btn-yes{background:#e53e3e;color:white;} .btn-no{background:#718096;color:white;}</style></head><body><div class='card'><h2>⚠️ ยืนยันการรีเซ็ต?</h2><p>คุณต้องการเคลียร์ยอดผู้เข้าชมเป็น 0 ใช่หรือไม่?</p><a href='/do_reset' class='btn btn-yes'>ใช่, รีเซ็ตเลย</a><a href='/' class='btn btn-no'>ยกเลิก</a></div></body></html>";
    server.send(200, "text/html", html);
  });
  server.on("/do_reset", []() { people_count = 0; global_event = "DAILY RESET"; server.sendHeader("Location", "/"); server.send(303); });

  server.begin();
  xTaskCreatePinnedToCore(cloudTask, "CloudTask", 10000, NULL, 1, NULL, 0);
}

void loop() {
  server.handleClient(); 
  
  int current_ir = digitalRead(IR_PIN);
  if (last_ir_state == HIGH && current_ir == LOW) { 
    people_count++;
    if(global_event == "NORMAL") global_event = "PERSON_ENTERED";
  }
  last_ir_state = current_ir;

  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 26000); 
  global_dist = (duration == 0) ? 400.0 : (duration * 0.034 / 2);
  
  int laser_blocked = digitalRead(LDR_PIN);

  if (door_open_trigger) {
    global_event = "VIP ACCESS - DOOR OPENED";
    
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay(); display.setTextSize(2); display.setCursor(0,20); display.println("VIP ENTRY!"); display.display();
    
    digitalWrite(LASER_PIN, LOW);
    digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW);
    
    myServo.write(180); delay(3000); myServo.write(90); 
    digitalWrite(LASER_PIN, HIGH);
    door_open_trigger = false;
    
    delay(1000); 
    vibration_flag = false;
  } else if (system_armed && (laser_blocked == HIGH || vibration_flag == true)) {
    
    if (vibration_flag == true) {
      global_event = "ALARM - VIBRATION!";
      vibration_flag = false; 
    } else {
      global_event = "ALARM - LASER BREACH!";
    }

    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay(); display.setTextSize(2); display.setCursor(0,20); display.println("INTRUDER!"); display.display();
    for(int i=0; i<3; i++){ digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW); delay(100); }
    
  } else {
    display.clearDisplay();
    display.setTextSize(1); display.setCursor(0,0); display.setTextColor(WHITE);
    if (system_armed) { display.print("Gate: SECURE P:"); display.println(people_count); } 
    else { display.print("Gate: DISABLED P:"); display.println(people_count); }
    display.setCursor(0,10); display.print("IP: "); display.println(WiFi.localIP());
    display.setTextSize(2); display.setCursor(0,35); display.print("Dist:"); display.print(global_dist, 1);
    display.display();
  }
}














