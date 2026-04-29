import cv2
import numpy as np
import requests
import time

esp32_ip = "172.20.10.2" 
url_open = f"http://{esp32_ip}/open"

face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')

recognizer = cv2.face.LBPHFaceRecognizer_create()

def train_vip_face():
    print("=======================================")
    print("📸 [ระบบลงทะเบียน VIP] กำลังเรียนรู้ใบหน้า...")
    print("👉 กรุณามองกล้องและขยับหน้าเล็กน้อย (รอประมาณ 5 วินาที)")
    print("=======================================")
    
    cap = cv2.VideoCapture(0)
    faces_data = []
    labels = []
    count = 0
    
    while count < 50: # ถ่ายรูปหน้าคุณ 50 รูปเพื่อสอน AI
        ret, frame = cap.read()
        if not ret: continue
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        faces = face_cascade.detectMultiScale(gray, 1.3, 5)
        
        for (x, y, w, h) in faces:
            faces_data.append(gray[y:y+h, x:x+w])
            labels.append(1) # ID = 1 คือคุณ (VIP)
            count += 1
            
            cv2.rectangle(frame, (x, y), (x+w, y+h), (255, 255, 0), 2)
            cv2.putText(frame, f"Learning: {count}/50", (x, y-10), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)
        
        cv2.imshow('VIP Registration', frame)
        cv2.waitKey(100) # ถ่าย 1 รูปทุกๆ 0.1 วินาที
        
    cap.release()
    cv2.destroyWindow('VIP Registration')
    
    print("🧠 กำลังประมวลผลข้อมูลลงสมอง AI...")
    recognizer.train(faces_data, np.array(labels))
    print("✅ ลงทะเบียนสำเร็จ! เริ่มระบบรักษาความปลอดภัย...")

train_vip_face()

cap = cv2.VideoCapture(0)
last_trigger_time = 0

while True:
    ret, frame = cap.read()
    if not ret: break
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    faces = face_cascade.detectMultiScale(gray, 1.3, 5, minSize=(100, 100))

    for (x, y, w, h) in faces:
        face_roi = gray[y:y+h, x:x+w]
        id_, confidence = recognizer.predict(face_roi)
        
        print(f"🔍 ตรวจสอบใบหน้า... คะแนนความคลาดเคลื่อน: {confidence:.2f}")
        
        if confidence < 45:
            name = "VIP / Owner"
            color = (0, 255, 0) # สีเขียว
            
            current_time = time.time()
            if current_time - last_trigger_time > 8:
                print("👤 ยืนยันตัวตนสำเร็จ! กำลังยิงคำสั่งไปที่บอร์ด ESP32...")
                try:
                    response = requests.get(url_open, timeout=3.0)                    
                    
                    if response.status_code == 200:
                         print("✅ บอร์ด ESP32 รับคำสั่งและเปิดประตูแล้ว!")
                         
                except Exception as e:
                    print(f"❌ แจ้งเตือน: ส่งคำสั่งล้มเหลว! โทรศัพท์กับบอร์ดอาจจะอยู่คนละเน็ตเวิร์ค หรือใส่ IP ผิด")
                    print(f"   สาเหตุเชิงลึก: {e}")
                    
                last_trigger_time = current_time
        else:
            name = "Unknown Intruder!"
            color = (0, 0, 255) # สีแดง

        cv2.rectangle(frame, (x, y), (x+w, y+h), color, 2)
        cv2.putText(frame, name, (x, y-10), cv2.FONT_HERSHEY_SIMPLEX, 0.8, color, 2)

    cv2.imshow('Smart Gate Guard', frame)
    if cv2.waitKey(1) & 0xFF == ord('q'): break

cap.release()
cv2.destroyAllWindows()

