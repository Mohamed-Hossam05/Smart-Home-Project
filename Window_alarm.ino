#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <Servo.h>
#include <DHT.h>

// ---------- الأطراف ----------
const byte WINDOW_PIN = 2;      // ده الطرف اللي وصلنا عليه حساس الشباك
const byte FAN_PIN    = 5;      // ده الطرف اللي بنوصل منه أمر تشغيل المروحة
const byte LIGHT_PIN  = 6;      // ده الطرف اللي وصلنا عليه لمبة المدخل
const byte TRIG_PIN   = 7;      // ده طرف إرسال الإشارة بتاع حساس المسافة
const byte ECHO_PIN   = 8;      // ده طرف استقبال الإشارة بتاع حساس المسافة
const byte SERVO_PIN  = 9;      // ده الطرف اللي وصلنا عليه سيرفو الباب
const byte BUZZER_PIN = 10;     // ده الطرف اللي وصلنا عليه البزر
const byte DHT_PIN    = 12;     // ده الطرف اللي وصلنا عليه حساس الحرارة
const byte POT_PIN    = A1;     // ده الطرف اللي وصلنا عليه مقاومة ضبط درجة الحرارة

// ---------- إعدادات ----------
const bool WINDOW_OPEN_LEVEL = HIGH;   // لما الشباك يتفتح الحساس بيدي HIGH، ولو العكس نغيرها LOW
const bool BUZZER_ACTIVE = true;       // بنحدد هنا نوع البزر، active ولا passive
const unsigned long WINDOW_DEBOUNCE_MS = 50; // بنستنى 50 مللي ثانية عشان نتأكد إن الشباك اتفتح بجد

const int SERVO_CLOSED = 0;            // الزاوية اللي الباب بيكون عندها مقفول
const int SERVO_OPEN   = 90;           // الزاوية اللي الباب بيفتح عندها
const unsigned long DOOR_OPEN_TIME = 5000;   // الباب هيفضل مفتوح 5 ثواني وبعدين يقفل

const int DETECT_DISTANCE_CM = 10;           // لو حد قرب من الحساس لمسافة 10 سم اللمبة هتنور
const unsigned long MEASURE_INTERVAL_MS = 100;   // بنقيس المسافة كل 100 مللي ثانية
const unsigned long LIGHT_HOLD_MS = 3000;    // اللمبة هتفضل منورة 3 ثواني بعد آخر مرة الحساس يرصد فيها حركة

// المروحة
const int MIN_THRESHOLD_C = 20;               // أقل درجة نقدر نختارها من المقاومة
const int MAX_THRESHOLD_C = 40;               // أعلى درجة نقدر نختارها من المقاومة
const int FAN_HYSTERESIS_C = 1;               // بنسيب فرق درجة عشان المروحة متفضلش تفصل وتشتغل بسرعة
const int FAN_FULL_SPEED_DELTA_C = 5;        
const int FAN_MIN_PWM = 100;                  
const unsigned long FAN_KICK_MS = 300;        // بنشغل المروحة بأقصى سرعة في الأول عشان تبدأ تلف
const unsigned long FAN_UPDATE_MS = 100;
const unsigned long DHT_INTERVAL_MS = 2000;  
const unsigned long SENSOR_TIMEOUT_MS = 10000;
const unsigned long STATUS_REFRESH_MS = 300;  // كل قد إيه بيانات الشاشة بتتحدث

// 1️⃣ الشاشة I2C (العنوان 0x27)
// بنجهز شاشة LCD عشان نعرض عليها حالة البيت
LiquidCrystal_I2C lcd(0x27, 16, 2);

// 2️⃣ الكيباد I2C (العنوان 0x26 بعد لحام A0)
// بنجهز الكيباد وبنحدد عدد الصفوف والأعمدة
const byte ROWS = 4;
const byte COLS = 3;

// هنا بنكتب ترتيب الأرقام والرموز الموجودة على الكيباد
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

byte rowPins[ROWS] = {0, 1, 2, 4};   // دي الأطراف اللي متوصلة بصفوف الكيباد
byte colPins[COLS] = {5, 6, 7};      // دي الأطراف اللي متوصلة بأعمدة الكيباد

byte keypad_address = 0x26;

Keypad_I2C kpd(makeKeymap(keys), rowPins, colPins, ROWS, COLS, keypad_address);

// 3️⃣ السيرفو وحساس الحرارة
// بنجهز السيرفو اللي هيفتح ويقفل الباب
Servo doorServo;
// بنجهز حساس الحرارة ونحدد نوعه
DHT dht(DHT_PIN, DHT11);

// 4️⃣ كلمات السر والأوامر
// المتغير ده بيجمع الأرقام اللي المستخدم بيدخلها
String inputPassword = "";
const String CORRECT_PASSWORD   = "5555";   // ده الباسورد اللي بيفتح الباب
const String ALARM_OFF_PASSWORD = "9999";   // ده الباسورد اللي بيوقف الإنذار

// 5️⃣ الحالات (States)
bool securityArmed = true;     // بنبدأ ووضع الأمان شغال
bool alarmActive   = false;     // بنعرف منها إذا كان الإنذار شغال ولا لأ
unsigned long windowOpenSince = 0;

bool doorOpen = false;         // بنعرف منها إذا كان الباب مفتوح ولا مقفول
unsigned long doorOpenSince = 0;

bool lightOn = false;
unsigned long lastMeasure = 0;
unsigned long lastDetected = 0;

// المتغيرات دي بتخزن قراءة الحرارة وحالة المروحة
int temperature = 0;
bool tempValid = false;
unsigned long lastDhtRead = 0;
unsigned long lastGoodRead = 0;

int threshold = 10;
bool fanOn = false;
unsigned long fanKickUntil = 0;
unsigned long lastFanUpdate = 0;

bool messageActive = false;     // عشان بيانات الشاشة متظهرش فوق الرسائل
unsigned long lastStatusDraw = 0;

// ---------- البزر ----------
// الدالة دي بتشغل البزر حسب نوعه
void buzzerOn() {
  if (BUZZER_ACTIVE) digitalWrite(BUZZER_PIN, HIGH);
  else tone(BUZZER_PIN, 2000);
}

// الدالة دي بتطفي البزر
void buzzerOff() {
  if (BUZZER_ACTIVE) digitalWrite(BUZZER_PIN, LOW);
  else noTone(BUZZER_PIN);
}

// ---------- الباب ----------
// لما نستدعي الدالة دي السيرفو بيفتح الباب
void openDoor() {
  doorServo.write(SERVO_OPEN);
  doorOpen = true;
  doorOpenSince = millis();
}

// هنا بنتأكد إذا كان وقت فتح الباب خلص ولا لسه
void updateDoor() {
  if (doorOpen && millis() - doorOpenSince >= DOOR_OPEN_TIME) {
// بنخلي الباب مقفول أول ما المشروع يشتغل
    doorServo.write(SERVO_CLOSED);
    doorOpen = false;
  }
}

// ---------- حساس المسافة واللمبة (مرتبط بوضع الأمان) ----------
// الدالة دي بتقيس المسافة بين الحساس وأقرب جسم قدامه
long readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 6000);
// لو مفيش إشارة راجعة بنرجع -1 عشان نعرف إن مفيش قراءة
  if (duration == 0) return -1;
// بنحول وقت رجوع الإشارة لمسافة بالسنتيمتر
  return (long)(duration / 58);
}

// هنا بنتابع الشخص لو قرب من الحساس ونشغل اللمبة وقتها
void updateProximityLight() {
// لو وضع الأمان مقفول بنطفي اللمبة ومش بنكمل قياس
  if (!securityArmed) {
    if (lightOn) {
      digitalWrite(LIGHT_PIN, LOW);
      lightOn = false;
    }
    return;
  }

  unsigned long now = millis();

// بنقيس المسافة كل 100 مللي ثانية
  if (now - lastMeasure >= MEASURE_INTERVAL_MS) {
    lastMeasure = now;
    long d = readDistanceCm();
// لو حد قرب من الحساس لمسافة 10 سم أو أقل بنشغل اللمبة
    if (d >= 0 && d <= DETECT_DISTANCE_CM) {
      lastDetected = now;
      if (!lightOn) {
        digitalWrite(LIGHT_PIN, HIGH);
        lightOn = true;
      }
    }
  }

// لو عدى 3 ثواني من غير ما الحساس يرصد حد، بنطفي اللمبة
  if (lightOn && now - lastDetected >= LIGHT_HOLD_MS) {
    digitalWrite(LIGHT_PIN, LOW);
    lightOn = false;
  }
}

// ---------- الحرارة والمروحة ----------
// بنقرأ درجة الحرارة من الحساس ونحدث آخر قراءة صحيحة
void updateTemperature() {
  unsigned long now = millis();
  if (now - lastDhtRead >= DHT_INTERVAL_MS) {
    lastDhtRead = now;
    float t = dht.readTemperature();
// لو قراءة الحرارة سليمة بنحفظها ونحدث وقت آخر قراءة
    if (!isnan(t)) {
      temperature = (int)round(t);
      tempValid = true;
      lastGoodRead = now;
// لو الحساس بقاله فترة مش بيدي قراءة مظبوطة بنعتبر الحرارة مش متاحة
    } else if (now - lastGoodRead >= SENSOR_TIMEOUT_MS) {
      tempValid = false;
    }
  }
}

// بنقرأ قيمة المقاومة ونحولها لدرجة الحرارة اللي المستخدم اختارها
int readThresholdC() {
  long sum = 0;
// بناخد 8 قراءات من المقاومة عشان القيمة تبقى أهدى
  for (byte i = 0; i < 8; i++) sum += analogRead(POT_PIN);
  long raw = sum / 8;
  return MIN_THRESHOLD_C + raw * (MAX_THRESHOLD_C - MIN_THRESHOLD_C) / 1023;
}

// هنا بنحدد المروحة تشتغل ولا تفصل وسرعتها تبقى قد إيه
void updateFan() {
  unsigned long now = millis();
  if (now - lastFanUpdate < FAN_UPDATE_MS) return;
  lastFanUpdate = now;

  threshold = readThresholdC();

// لو مفيش قراءة حرارة سليمة بنطفي المروحة
  if (!tempValid) {
    fanOn = false;
    analogWrite(FAN_PIN, 0);
    return;
  }

// لو الحرارة عدت الدرجة اللي اخترناها بنشغل المروحة
  if (!fanOn && temperature > threshold) {
    fanOn = true;
    fanKickUntil = now + FAN_KICK_MS;
// لو الحرارة نزلت عن الدرجة المطلوبة بالفرق المحدد بنطفي المروحة
  } else if (fanOn && temperature <= threshold - FAN_HYSTERESIS_C) {
    fanOn = false;
  }

  if (!fanOn) {
    analogWrite(FAN_PIN, 0);
    return;
  }

// في أول لحظات التشغيل بنخلي المروحة تلف بأقصى سرعة
  if (now < fanKickUntil) {
    analogWrite(FAN_PIN, 255);
    return;
  }

  int over = constrain(temperature - threshold, 0, FAN_FULL_SPEED_DELTA_C);
  int pwm = map(over, 0, FAN_FULL_SPEED_DELTA_C, FAN_MIN_PWM, 255);
  analogWrite(FAN_PIN, pwm);
}

// ---------- عرض كافة بيانات النظام على الشاشة (Real-time LCD Feedback) ----------
// الدالة دي بتعرض حالة الأمان والباب والحرارة على الشاشة
void drawStatusScreen() {
  char line0[17];
  char line1[17];

  snprintf(line0, sizeof(line0), "Sec:%-3s  Door:%-2s",
           securityArmed ? "ARM" : "DIS",
           doorOpen ? "OP" : "CL");

  if (tempValid) {
    snprintf(line1, sizeof(line1), "T:%dC   Set:%dC", temperature, threshold);
  } else {
    snprintf(line1, sizeof(line1), "T:--C   Set:%dC", threshold);
  }

  lcd.setCursor(0, 0);
  lcd.print(line0);
  lcd.setCursor(0, 1);
  lcd.print(line1);
}

// بنحدث بيانات الشاشة من غير ما نمسح رسالة أو باسورد المستخدم
void updateStatusScreen() {
// مش بنحدث الشاشة لو فيه إنذار أو رسالة أو المستخدم بيكتب الباسورد
  if (alarmActive || messageActive || inputPassword.length() > 0) return;
  unsigned long now = millis();
  if (now - lastStatusDraw < STATUS_REFRESH_MS) return;
  lastStatusDraw = now;
  drawStatusScreen();
}

// ---------- الشباك والإنذار ----------
// الدالة دي بتقولنا الشباك مفتوح ولا مقفول
bool windowIsOpen() {
  return digitalRead(WINDOW_PIN) == WINDOW_OPEN_LEVEL;
}

// لما الشباك يتفتح في وضع الأمان بنشغل الإنذار
void startAlarm() {
  alarmActive = true;
  buzzerOn();
  resetPasswordScreen();
}

// هنا بنتابع حالة الشباك ونتأكد إنه مفتوح قبل تشغيل الإنذار
void checkWindow() {
// لو وضع الأمان مش شغال أو الإنذار اشتغل بالفعل مش محتاجين نفحص الشباك
  if (!securityArmed || alarmActive) return;
 
// لازم الشباك يكون مقفول عشان نسمح بإيقاف الإنذار
  if (windowIsOpen()) {
    if (windowOpenSince == 0) windowOpenSince = millis();
// لو الشباك فضل مفتوح 50 مللي ثانية بنشغل الإنذار
    else if (millis() - windowOpenSince >= WINDOW_DEBOUNCE_MS) startAlarm();
  } else {
    windowOpenSince = 0;
  }
}

// المهام الخلفية المستمرة
// الدالة دي بتشغل كل الحاجات اللي لازم نتابعها طول الوقت
void backgroundTasks() {
  checkWindow();
  updateDoor();
  updateProximityLight();
  updateTemperature();
  updateFan();
  updateStatusScreen();
}

// بنستنى المدة المطلوبة وفي نفس الوقت بنخلي باقي أجزاء المشروع شغالة
void waitMs(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    backgroundTasks();
  }
}

// هنا بنجهز كل المكونات أول ما الأردوينو يشتغل
void setup() {
// بنجهز طرف المروحة كمخرج
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);

  Wire.begin();

// بنجهز طرف حساس الشباك كمدخل
  pinMode(WINDOW_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
// بنطفي البزر
  buzzerOff();
  digitalWrite(LIGHT_PIN, LOW);

  doorServo.attach(SERVO_PIN);
// بنخلي الباب مقفول أول ما المشروع يشتغل
  doorServo.write(SERVO_CLOSED);

  dht.begin();

  lcd.init();
// بنشغل إضاءة الشاشة
  lcd.backlight();

// بنبدأ تشغيل الكيباد
  kpd.begin();

  // 🌟 رسالة الترحيب مقسمة بالتساوي على السطور
// بنوقف تحديث بيانات الشاشة طول ما رسالة الترحيب ظاهرة
  messageActive = true;
 
  // الصفحة الأولى: الترحيب بالبيت الذكي (16 حرفاً في كل سطر بالضبط)
  lcd.setCursor(0, 0);
  lcd.print("   Welcome To   "); // 16 حرفاً بالمسافات
  lcd.setCursor(0, 1);
  lcd.print(" Our Smart Home!"); // 16 حرفاً بالمسافات
  waitMs(2500);

  // الصفحة الثانية: أسماء أعضاء الفريق
  lcd.setCursor(0, 0);
  lcd.print("Mohamed & Hazem "); // 16 حرفاً بالمسافات
  lcd.setCursor(0, 1);
  lcd.print("Jannah & Fatma  "); // 16 حرفاً بالمسافات
  waitMs(2500);

// خلصنا عرض الرسالة ونقدر نحدث الشاشة تاني
  messageActive = false;

  resetPasswordScreen();
}

// دي اللوب الرئيسية اللي بتفضل تتكرر طول ما الجهاز شغال
void loop() {
  backgroundTasks();

// بنشوف إذا كان المستخدم ضغط على زرار ولا لأ
  char key = kpd.getKey();

  if (key) {
// لما المستخدم يضغط # بنراجع الباسورد اللي دخله
    if (key == '#') {
      checkPassword();
    }
// لما يضغط * بنمسح آخر رقم دخله
    else if (key == '*') {
      if (inputPassword.length() > 0) {
// بنشيل آخر رقم من الباسورد
        inputPassword.remove(inputPassword.length() - 1);
        if (inputPassword.length() == 0) {
          resetPasswordScreen();
        } else {
          lcd.setCursor(inputPassword.length() - 1, 1);
          lcd.print(" ");
          lcd.setCursor(inputPassword.length() - 1, 1);
        }
      }
    }
    else {
      if (inputPassword.length() < 16) {
        if (inputPassword.length() == 0) {
          lcd.clear();
          lcd.setCursor(0, 0);
// بنطلب من المستخدم يدخل الباسورد
          lcd.print("Enter Code/PIN:");
          lcd.setCursor(0, 1);
        }
// بنضيف الرقم الجديد للباسورد
        inputPassword += key;
        lcd.setCursor(inputPassword.length() - 1, 1);
// بنعرض نجمة بدل الرقم عشان الباسورد يفضل مخفي
        lcd.print("*");
      }
    }
  }
}

// ---------- إدارة الشاشة ----------
// بنمسح الرقم القديم ونرجع نعرض الشاشة المناسبة
void resetPasswordScreen() {
  inputPassword = "";
  lcd.clear();
  if (alarmActive) {
    lcd.setCursor(0, 0);
// لو الإنذار شغال بنطلب منه يدخل باسورد إيقافه
    lcd.print("ALARM! Enter PIN");
    lcd.setCursor(0, 1);
  } else {
    drawStatusScreen();
    lastStatusDraw = millis();
  }
}

// الدالة دي بتعرض رسالة على سطرين لمدة معينة
void showMessage(const char* line1, const char* line2, unsigned long ms) {
// بنوقف تحديث بيانات الشاشة طول ما رسالة الترحيب ظاهرة
  messageActive = true;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
  waitMs(ms);
// خلصنا عرض الرسالة ونقدر نحدث الشاشة تاني
  messageActive = false;
}

// ---------- فحص الأوامر والباسوردات ----------
// هنا بنراجع الرقم اللي المستخدم دخله وننفذ الأمر المناسب
void checkPassword() {
  if (alarmActive) {
// لو الباسورد صح نقدر نحاول نوقف الإنذار
    if (inputPassword == ALARM_OFF_PASSWORD) {
// لازم الشباك يكون مقفول عشان نسمح بإيقاف الإنذار
      if (windowIsOpen()) {
        showMessage("Close the Window", "then enter code", 2000);
      } else {
        alarmActive = false;
        windowOpenSince = 0;
// بنطفي البزر
        buzzerOff();
        showMessage("   Alarm Off    ", "", 2000);
      }
    } else {
      showMessage("     Wrong      ", "", 1500);
    }
  } else {
// لو دخل 1 بنشغل وضع الأمان
    if (inputPassword == "1") {
      securityArmed = true;
      showMessage(" Security Mode  ", "     ARMED      ", 2000);
    }
// لو دخل 0 بنقفل وضع الأمان
    else if (inputPassword == "0") {
      securityArmed = false;
      showMessage(" Security Mode  ", "    DISARMED    ", 2000);
    }
// لو الباسورد صح بنفتح الباب
    else if (inputPassword == CORRECT_PASSWORD) {
      openDoor();
      showMessage("   Right Pin    ", "   Door Open    ", 2000);
    }
    else {
      showMessage("  Invalid Code  ", "", 1500);
    }
  }

  resetPasswordScreen();
}
