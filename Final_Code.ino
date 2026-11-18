#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <Servo.h>
#include <DHT.h>

// ---------- 1. أطراف المكونات ----------
const byte WINDOW_PIN       = 2;      // بنوصل حساس الشباك MC-38 هنا
const byte INDOOR_LIGHT_PIN = 6;      // بنوصل اللمبة الداخلية هنا
const byte BUTTON_PIN       = 4;      // ده مكان الزرار اللي بنشغل بيه الإضاءة يدوي
const byte FAN_PIN          = 5;      // ده الطرف اللي بيتحكم في سرعة المروحة
const byte LIGHT_PIN        = 11;     // بنوصل لمبة المدخل اللي بتشتغل مع حساس المسافة هنا
const byte TRIG_PIN         = 7;      // ده طرف إرسال الإشارة في حساس المسافة
const byte ECHO_PIN         = 8;      // ده طرف استقبال الإشارة في حساس المسافة
const byte SERVO_PIN        = 9;      // بنوصل السيرفو المسؤول عن فتح وقفل الباب هنا
const byte BUZZER_PIN       = 10;     // ده الطرف اللي البزر متوصل بيه
const byte DHT_PIN          = 12;     // بنوصل حساس الحرارة DHT11 هنا
const byte LDR_PIN          = A0;     // ده مدخل حساس الضوء
const byte POT_PIN          = A1;     // ده مدخل البوتنشيومتر اللي بنظبط منه درجة الحرارة

// ---------- 2. إعدادات المنظومة ----------
const bool WINDOW_OPEN_LEVEL = HIGH;   // لما قراءة الحساس تبقى HIGH يبقى الشباك مفتوح
const bool BUZZER_ACTIVE     = true;   // بنحدد هنا إن البزر من النوع Active
const unsigned long WINDOW_DEBOUNCE_MS = 50; // بنستنى المدة دي عشان نتأكد إن الشباك اتفتح بجد

const int SERVO_CLOSED = 0;            // الزاوية اللي بيقف عندها الباب وهو مقفول
const int SERVO_OPEN   = 90;           // الزاوية اللي بيوصل لها السيرفو لما الباب يتفتح
const unsigned long DOOR_OPEN_TIME = 5000; // المدة اللي الباب بيفضل فيها مفتوح

const int DETECT_DISTANCE_CM = 10;     // لو حد قرب للمسافة دي اللمبة تنور
const unsigned long MEASURE_INTERVAL_MS = 100; // الوقت بين كل قراءة والتانية لحساس المسافة
const unsigned long LIGHT_HOLD_MS = 3000; // اللمبة تفضل منورة المدة دي بعد ما الحركة تقف

// بنحدد عدد المحاولات الغلط المسموح بيها قبل تشغيل الإنذار
const byte MAX_WRONG_ATTEMPTS = 3;

// إعدادات اللمبة الداخلية وحساس الضوء
const int LDR_THRESHOLD = 500;        // لو قراءة الحساس أقل من الرقم ده بنعتبر المكان ضلمة
bool manualOverride   = false;        // بنعرف من هنا إذا كان التشغيل اليدوي للُّمبة متفعل
bool prevButtonState  = HIGH;         // بنحتفظ بآخر حالة للزرار عشان نعرف إذا اتضغط
unsigned long lastDebounceTime = 0;   // بنسجل وقت آخر ضغطة زرار

// إعدادات المروحة وحساس الحرارة
const int MIN_THRESHOLD_C = 20;       // أقل درجة حرارة ممكن نختارها
const int MAX_THRESHOLD_C = 40;       // أعلى درجة حرارة ممكن نختارها
const int FAN_HYSTERESIS_C = 1;       // الفرق بين التشغيل والإيقاف عشان المروحة متفضلش تفصل وتشتغل بسرعة
const int FAN_FULL_SPEED_DELTA_C = 5; // فرق الحرارة اللي عنده المروحة توصل لأقصى سرعة
const int FAN_MIN_PWM = 100;          // أقل قيمة تشغيل للمروحة
const unsigned long FAN_KICK_MS = 300; // بندي المروحة دفعة قوية في البداية عشان تبدأ تلف
const unsigned long FAN_UPDATE_MS = 100; // كل قد إيه بنراجع سرعة المروحة
const unsigned long DHT_INTERVAL_MS = 2000; // الوقت بين قراءات حساس الحرارة
const unsigned long SENSOR_TIMEOUT_MS = 10000; // لو الحساس مديش قراءة سليمة خلال المدة دي بنعتبر القراءة غير صالحة
const unsigned long STATUS_REFRESH_MS = 300; // كل قد إيه بنحدث البيانات اللي على الشاشة

// ---------- 3. تعريف الموديولات ----------
// بنجهز شاشة LCD على عنوان I2C رقم 0x27
LiquidCrystal_I2C lcd(0x27, 16, 2);

// بنجهز الكيباد على عنوان I2C رقم 0x26
const byte ROWS = 4;
const byte COLS = 3;

// بنرتب الأزرار بنفس ترتيبها على الكيباد
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

// بنحدد أطراف الصفوف والأعمدة اللي الكيباد متوصل بيها
byte rowPins[ROWS] = {0, 1, 2, 4};
byte colPins[COLS] = {5, 6, 7};
byte keypad_address = 0x26;

// بننشئ الكيباد باستخدام ترتيب الأزرار والأطراف والعنوان اللي حددناهم
Keypad_I2C kpd(makeKeymap(keys), rowPins, colPins, ROWS, COLS, keypad_address);

// بنجهز السيرفو وحساس الحرارة
Servo doorServo;
DHT dht(DHT_PIN, DHT11);

// ---------- 4. كلمات السر والأوامر ----------
String inputPassword = ""; // هنا بنجمع الأرقام اللي المستخدم بيدخلها
const String CORRECT_PASSWORD   = "5555";   // الكود اللي بيفتح الباب
const String ALARM_OFF_PASSWORD = "9999";  // الكود اللي بيوقف الإنذار

// ---------- 5. متغيرات حالات النظام ---------- 
bool securityArmed = true;     // وضع الأمان بيبدأ شغال مع بداية تشغيل النظام
bool alarmActive   = false;    // بنعرف من المتغير ده إذا كان الإنذار شغال ولا لأ
unsigned long windowOpenSince = 0; // بنسجل وقت بداية فتح الشباك

byte failedAttempts = 0;       // بنعد هنا عدد مرات إدخال الباسورد الغلط

bool doorOpen = false;         // بنحدد إذا كان الباب مفتوح ولا مقفول
unsigned long doorOpenSince = 0; // بنسجل وقت فتح الباب

bool lightOn = false;          // بنحدد إذا كانت لمبة المدخل منورة ولا مطفية
unsigned long lastMeasure = 0; // بنسجل وقت آخر قياس للمسافة
unsigned long lastDetected = 0; // بنسجل آخر وقت الحساس اكتشف فيه حد قريب

int temperature = 0;           // بنحتفظ بآخر درجة حرارة اتقرت
bool tempValid = false;        // بنعرف إذا كانت قراءة الحرارة سليمة
unsigned long lastDhtRead = 0; // بنسجل وقت آخر قراءة من حساس الحرارة
unsigned long lastGoodRead = 0; // بنسجل وقت آخر قراءة حرارة سليمة

int threshold = 10;            // درجة الحرارة اللي على أساسها المروحة بتشتغل
bool fanOn = false;            // بنحدد إذا كانت المروحة شغالة
unsigned long fanKickUntil = 0; // بنحدد وقت انتهاء دفعة بداية تشغيل المروحة
unsigned long lastFanUpdate = 0; // بنسجل وقت آخر تحديث للمروحة

bool messageActive = false;    // بنستخدمه عشان الشاشة متكتبش فوق رسالة لسه ظاهرة
unsigned long lastStatusDraw = 0; // بنسجل وقت آخر تحديث لشاشة الحالة

bool bellActive = false;       // بنعرف إذا كان الجرس شغال بسبب الضغط على زرار *

// ---------- 6. دوال التحكم الفرعية ----------

// تشغيل البزر
void buzzerOn() {
  if (BUZZER_ACTIVE) digitalWrite(BUZZER_PIN, HIGH); // لو البزر Active بنشغله بإشارة HIGH
  else tone(BUZZER_PIN, 2000); // غير كده بنشغله بتردد 2000 هرتز
}

// إيقاف البزر
void buzzerOff() {
  if (BUZZER_ACTIVE) digitalWrite(BUZZER_PIN, LOW); // بنطفي البزر بإشارة LOW
  else noTone(BUZZER_PIN); // بنوقف التردد لو البزر مش Active
}

// بنراقب ضغط ورفع الإيد عن زرار النجمة عشان نتحكم في الجرس
void keypadEvent(KeypadEvent key) {
  if (key != '*') return; // لو الزرار اللي اتضغط مش النجمة بنخرج من الدالة

  switch (kpd.getState()) {
    case PRESSED:
      // الجرس يشتغل لو مفيش كود بيتكتب ولا إنذار شغال ولا رسالة ظاهرة
      if (inputPassword.length() == 0 && !alarmActive && !messageActive) {
        bellActive = true;
        buzzerOn();
      }
      break;

    case RELEASED:
      if (bellActive) {
        bellActive = false;
        if (!alarmActive) buzzerOff();   // بنطفي الجرس بشرط إن الإنذار مش شغال
      }
      break;

    default:
      break;
  }
}

// التحكم في فتح الباب بالسيرفو
void openDoor() {
  doorServo.write(SERVO_OPEN); // بنحرك السيرفو لزاوية فتح الباب
  doorOpen = true; // بنسجل إن الباب بقى مفتوح
  doorOpenSince = millis(); // بنسجل وقت فتح الباب
}

void updateDoor() {
  // لو الباب مفتوح والمدة المحددة عدت بنقفله تاني
  if (doorOpen && millis() - doorOpenSince >= DOOR_OPEN_TIME) {
    doorServo.write(SERVO_CLOSED);
    doorOpen = false;
  }
}

// قراءة المسافة والتحكم في لمبة المدخل
long readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW); // بنبدأ بإيقاف طرف الإرسال
  delayMicroseconds(2); // بنستنى وقت صغير قبل إرسال النبضة
  digitalWrite(TRIG_PIN, HIGH); // بنبعت نبضة للحساس
  delayMicroseconds(10); // بنخلي النبضة مستمرة 10 ميكروثانية
  digitalWrite(TRIG_PIN, LOW); // بنوقف الإرسال بعد النبضة

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 6000); // بنقيس وقت رجوع الإشارة
  if (duration == 0) return -1; // لو مفيش إشارة رجعت بنرجع -1
  return (long)(duration / 58); // بنحول وقت الإشارة لمسافة بالسنتيمتر
}

void updateProximityLight() {
  // لو وضع الأمان مش شغال بنطفي لمبة المدخل ونخرج
  if (!securityArmed) {
    if (lightOn) {
      digitalWrite(LIGHT_PIN, LOW);
      lightOn = false;
    }
    return;
  }

  unsigned long now = millis(); // بنجيب الوقت الحالي

  // بنقيس المسافة لما ميعاد القراءة الجديدة ييجي
  if (now - lastMeasure >= MEASURE_INTERVAL_MS) {
    lastMeasure = now;
    long d = readDistanceCm();
    // لو في حد على مسافة قريبة بنسجل وقت اكتشافه وننور اللمبة
    if (d >= 0 && d <= DETECT_DISTANCE_CM) {
      lastDetected = now;
      if (!lightOn) {
        digitalWrite(LIGHT_PIN, HIGH);
        lightOn = true;
      }
    }
  }

  // لو مفيش حد اتكشف لمدة كفاية بنطفي اللمبة
  if (lightOn && now - lastDetected >= LIGHT_HOLD_MS) {
    digitalWrite(LIGHT_PIN, LOW);
    lightOn = false;
  }
}

// التحكم في اللمبة الداخلية حسب الضوء أو الزرار اليدوي
void updateIndoorLighting() {
  // بنقرأ قيمة حساس الضوء
  int ldrValue = analogRead(LDR_PIN);
  bool isDark = (ldrValue < LDR_THRESHOLD); // بنحدد إذا كانت الغرفة ضلمة

  // بنقرأ حالة الزرار اليدوي
  bool currentButtonState = digitalRead(BUTTON_PIN);

  // لو الزرار كان مرفوع وبقى مضغوط بنعتبر إن فيه ضغطة جديدة
  if (prevButtonState == HIGH && currentButtonState == LOW) {
    // بنستنى شوية بين الضغطات عشان نتجنب تكرار نفس الضغطة
    if (millis() - lastDebounceTime > 250) {
      manualOverride = !manualOverride; // بنعكس حالة التشغيل اليدوي
      lastDebounceTime = millis(); // بنحدث وقت آخر ضغطة
    }
  }
  prevButtonState = currentButtonState; // بنحتفظ بالحالة الحالية للمرّة الجاية

  // بننور اللمبة لو المكان ضلمة أو التشغيل اليدوي متفعل
  if (isDark || manualOverride) {
    digitalWrite(INDOOR_LIGHT_PIN, HIGH);
  } else {
    digitalWrite(INDOOR_LIGHT_PIN, LOW);
  }
}

// قراءة درجة الحرارة والتحكم في المروحة
void updateTemperature() {
  unsigned long now = millis(); // بنجيب الوقت الحالي
  // بنقرأ الحرارة كل فترة محددة بدل ما نقرأها في كل لفة
  if (now - lastDhtRead >= DHT_INTERVAL_MS) {
    lastDhtRead = now;
    float t = dht.readTemperature(); // بنقرأ درجة الحرارة من الحساس
    if (!isnan(t)) {
      temperature = (int)round(t); // بنقرب القراءة لأقرب رقم صحيح
      tempValid = true; // بنسجل إن القراءة سليمة
      lastGoodRead = now; // بنحدث وقت آخر قراءة سليمة
    } else if (now - lastGoodRead >= SENSOR_TIMEOUT_MS) {
      tempValid = false; // لو بقالنا وقت طويل من غير قراءة سليمة بنعتبر الحساس مش مدي قراءة
    }
  }
}

int readThresholdC() {
  long sum = 0;
  // بناخد 8 قراءات من البوتنشيومتر عشان نقلل تأثير اختلاف القراءة
  for (byte i = 0; i < 8; i++) sum += analogRead(POT_PIN);
  long raw = sum / 8; // بنحسب متوسط القراءات
  // بنحول قراءة البوتنشيومتر لدرجة حرارة بين الحد الأدنى والأقصى
  return MIN_THRESHOLD_C + raw * (MAX_THRESHOLD_C - MIN_THRESHOLD_C) / 1023;
}

void updateFan() {
  unsigned long now = millis(); // بنجيب الوقت الحالي
  if (now - lastFanUpdate < FAN_UPDATE_MS) return; // لو لسه ميعاد التحديث مجاش بنخرج
  lastFanUpdate = now;

  threshold = readThresholdC(); // بنجيب درجة الحرارة اللي المستخدم ظابطها

  // لو قراءة حساس الحرارة مش سليمة بنطفي المروحة
  if (!tempValid) {
    fanOn = false;
    analogWrite(FAN_PIN, 0);
    return;
  }

  // بنشغل المروحة لما الحرارة تعدي الدرجة المحددة
  if (!fanOn && temperature > threshold) {
    fanOn = true;
    fanKickUntil = now + FAN_KICK_MS; // بنحدد وقت انتهاء دفعة البداية
  } else if (fanOn && temperature <= threshold - FAN_HYSTERESIS_C) {
    fanOn = false; // بنطفي المروحة لما الحرارة تنزل للحد المطلوب
  }

  if (!fanOn) {
    analogWrite(FAN_PIN, 0); // بنوقف المروحة لو مش محتاجة تشتغل
    return;
  }

  // في بداية التشغيل بندي المروحة أقصى سرعة عشان تبدأ تلف
  if (now < fanKickUntil) {
    analogWrite(FAN_PIN, 255);
    return;
  }

  // بنحسب سرعة المروحة على حسب الفرق بين الحرارة الحالية والدرجة المحددة
  int over = constrain(temperature - threshold, 0, FAN_FULL_SPEED_DELTA_C);
  int pwm = map(over, 0, FAN_FULL_SPEED_DELTA_C, FAN_MIN_PWM, 255);
  analogWrite(FAN_PIN, pwm); // بنبعت قيمة السرعة للمروحة
}

// عرض حالة النظام ودرجات الحرارة على الشاشة
void drawStatusScreen() {
  char line0[17]; // السطر الأول اللي هنعرض فيه حالة الأمان والباب
  char tmp[17]; // بنجهز فيه نص درجة الحرارة والدرجة المطلوبة
  char line1[17]; // السطر التاني اللي هيظهر على الشاشة

  // بنجهز السطر الأول حسب حالة الأمان والباب
  snprintf(line0, sizeof(line0), "Sec:%-3s  Door:%-2s",
           securityArmed ? "ARM" : "DIS",
           doorOpen ? "OP" : "CL");

  // بنجهز سطر الحرارة، ولو القراءة مش سليمة بنعرض شرطتين بدل الرقم
  if (tempValid) {
    snprintf(tmp, sizeof(tmp), "T:%dC   Set:%dC", temperature, threshold);
  } else {
    snprintf(tmp, sizeof(tmp), "T:--C   Set:%dC", threshold);
  }
  snprintf(line1, sizeof(line1), "%-16s", tmp); // بنكمل السطر بمسافات عشان نمسح أي حروف قديمة

  lcd.setCursor(0, 0); // بنروح لأول السطر الأول
  lcd.print(line0); // بنعرض حالة الأمان والباب
  lcd.setCursor(0, 1); // بنروح لأول السطر التاني
  lcd.print(line1); // بنعرض درجات الحرارة
}

void updateStatusScreen() {
  // مش بنحدث الشاشة لو الإنذار أو رسالة أو إدخال كود شغال
  if (alarmActive || messageActive || inputPassword.length() > 0) return;
  unsigned long now = millis();
  if (now - lastStatusDraw < STATUS_REFRESH_MS) return; // بنتأكد إن وقت التحديث جه
  lastStatusDraw = now;
  drawStatusScreen(); // بنعرض البيانات الجديدة
}

// متابعة حالة الشباك وتشغيل الإنذار
bool windowIsOpen() {
  return digitalRead(WINDOW_PIN) == WINDOW_OPEN_LEVEL; // بنرجع إذا كان الشباك مفتوح حسب قراءة الحساس
}

void startAlarm() {
  alarmActive = true; // بنسجل إن الإنذار اشتغل
  buzzerOn(); // بنشغل البزر
  resetPasswordScreen(); // بنجهز الشاشة عشان المستخدم يدخل كود إيقاف الإنذار
}

void checkWindow() {
  // مش بنفحص الشباك لو وضع الأمان مش شغال أو الإنذار اشتغل بالفعل
  if (!securityArmed || alarmActive) return;

  if (windowIsOpen()) {
    if (windowOpenSince == 0) windowOpenSince = millis(); // بنسجل وقت بداية فتح الشباك
    // لو الشباك فضل مفتوح للمدة المحددة بنشغل الإنذار
    else if (millis() - windowOpenSince >= WINDOW_DEBOUNCE_MS) startAlarm();
  } else {
    windowOpenSince = 0; // بنصفر الوقت لما الشباك يتقفل
  }
}

// بنشغل المهام دي باستمرار من غير ما نوقف البرنامج باستخدام delay
void backgroundTasks() {
  checkWindow(); // بنتابع الشباك والإنذار
  updateDoor(); // بنتابع وقت قفل الباب
  updateProximityLight(); // بنتابع حساس المسافة ولمبة المدخل
  updateIndoorLighting(); // بنتابع اللمبة الداخلية
  updateTemperature(); // بنقرأ درجة الحرارة
  updateFan(); // بنحدث تشغيل وسرعة المروحة
  updateStatusScreen(); // بنحدث بيانات الشاشة
}

void waitMs(unsigned long ms) {
  unsigned long start = millis(); // بنسجل وقت بداية الانتظار
  // طول فترة الانتظار بنفضل نشغل المهام الخلفية عشان النظام يفضل مستجيب
  while (millis() - start < ms) {
    backgroundTasks();
  }
}

// ---------- 7. دالة الإعداد الأساسية ----------
void setup() {
  pinMode(FAN_PIN, OUTPUT); // بنحدد طرف المروحة إنه خرج
  digitalWrite(FAN_PIN, LOW); // وبنبدأ والمروحة مطفية

  Wire.begin(); // بنبدأ اتصال I2C

  pinMode(WINDOW_PIN, INPUT_PULLUP); // بنجهز طرف حساس الشباك كدخل مع مقاومة داخلية
  pinMode(BUTTON_PIN, INPUT_PULLUP); // بنفعل المقاومة الداخلية للزرار
  pinMode(INDOOR_LIGHT_PIN, OUTPUT); // بنحدد طرف اللمبة الداخلية كخرج
  pinMode(BUZZER_PIN, OUTPUT); // بنحدد طرف البزر كخرج
  pinMode(LIGHT_PIN, OUTPUT); // بنحدد طرف لمبة المدخل كخرج
  pinMode(TRIG_PIN, OUTPUT); // بنحدد طرف إرسال حساس المسافة كخرج
  pinMode(ECHO_PIN, INPUT); // بنحدد طرف استقبال حساس المسافة كدخل

  buzzerOff(); // بنتأكد إن البزر مطفي في البداية
  digitalWrite(LIGHT_PIN, LOW); // بنبدأ ولمبة المدخل مطفية
  digitalWrite(INDOOR_LIGHT_PIN, LOW); // وبنبدأ واللمبة الداخلية مطفية

  doorServo.attach(SERVO_PIN); // بنوصل السيرفو بالطرف المحدد
  doorServo.write(SERVO_CLOSED); // بنخلي الباب مقفول أول ما النظام يشتغل

  dht.begin(); // بنبدأ تشغيل حساس الحرارة

  lcd.init(); // بنجهز شاشة LCD
  lcd.backlight(); // بنشغل إضاءة الشاشة

  kpd.begin(); // بنبدأ تشغيل الكيباد
  kpd.addEventListener(keypadEvent); // بنربط الكيباد بالدالة اللي بتتابع ضغط زرار النجمة

  // بنعرض رسالة الترحيب على الشاشة
  messageActive = true;

  // أول شاشة: بنرحب بالمستخدم في البيت الذكي
  lcd.setCursor(0, 0);
  lcd.print("   Welcome To   ");
  lcd.setCursor(0, 1);
  lcd.print(" Our Smart Home!");
  waitMs(2500); // بنسيب رسالة الترحيب ظاهرة شوية

  // تاني شاشة: بنعرض أسماء أعضاء الفريق
  lcd.setCursor(0, 0);
  lcd.print("Mohamed & Hazem ");
  lcd.setCursor(0, 1);
  lcd.print("Jannah & Fatma  ");
  waitMs(2500); // بنسيب أسماء الفريق ظاهرة شوية

  messageActive = false; // بنسمح بتحديث الشاشة بعد ما رسائل البداية تخلص

  resetPasswordScreen(); // بنرجع للشاشة الأساسية
}

// ---------- 8. دالة التكرار الرئيسية ----------
void loop() {
  backgroundTasks(); // بننفذ المهام اللي لازم تفضل شغالة طول الوقت

  char key = kpd.getKey(); // بنشوف إذا كان المستخدم ضغط على زرار في الكيباد

  if (key) {
    if (key == '#') {
      checkPassword(); // لما يضغط شباك بنفحص الكود اللي اتكتب
    }
    else if (key == '*') {
      if (bellActive) {
        // لو الجرس شغال مش بنستخدم النجمة لمسح أي رقم
      }
      else if (inputPassword.length() > 0) {
        // بنمسح آخر رقم اتكتب بس
        inputPassword.remove(inputPassword.length() - 1);
        if (inputPassword.length() == 0) {
          resetPasswordScreen(); // لو مفيش أرقام فاضلة بنرجع للشاشة العادية
        } else {
          lcd.setCursor(inputPassword.length(), 1); // بنروح مكان آخر نجمة اتمسحت
          lcd.print(" "); // بنمسح النجمة من الشاشة
          lcd.setCursor(inputPassword.length(), 1); // بنرجع المؤشر لنفس المكان
        }
      }
    }
    else {
      // بنقبل أرقام جديدة طول ما فيه مكان في الشاشة
      if (inputPassword.length() < 16) {
        if (inputPassword.length() == 0) {
          drawEntryScreen(); // أول رقم بيتكتب بنعرض شاشة إدخال الكود
        }
        inputPassword += key; // بنضيف الرقم الجديد للكود
        lcd.setCursor(inputPassword.length() - 1, 1); // بنحدد مكان عرض النجمة
        lcd.print("*"); // بنعرض نجمة بدل الرقم عشان الكود يفضل مخفي
      }
    }
  }
}

// ---------- 9. إدارة الشاشة والأوامر ----------
// تجهيز شاشة إدخال الكود مع الحفاظ على عنوان الإنذار لو شغال
void drawEntryScreen() {
  lcd.clear(); // بنمسح محتوى الشاشة القديم
  lcd.setCursor(0, 0); // بنبدأ من أول السطر الأول
  if (alarmActive) lcd.print("ALARM! Enter PIN"); // لو الإنذار شغال بنعرض رسالة الإنذار
  else             lcd.print("Enter Code/PIN: "); // غير كده بنطلب من المستخدم يدخل الكود
  lcd.setCursor(0, 1); // بنجهز السطر التاني لعرض النجوم
}

void resetPasswordScreen() {
  inputPassword = ""; // بنفضي الكود اللي المستخدم كان كاتبه
  if (alarmActive) {
    drawEntryScreen(); // لو الإنذار شغال بنرجع لشاشة إدخال كود إيقافه
  } else {
    lcd.clear(); // بنمسح الشاشة
    drawStatusScreen(); // وبنعرض حالة النظام من جديد
    lastStatusDraw = millis(); // بنحدث وقت آخر عرض للبيانات
  }
}

void showMessage(const char* line1, const char* line2, unsigned long ms) {
  messageActive = true; // بنوقف تحديث شاشة الحالة طول ما الرسالة ظاهرة
  lcd.clear(); // بنمسح الشاشة القديمة
  lcd.setCursor(0, 0); // بنحدد بداية السطر الأول
  lcd.print(line1); // بنعرض الرسالة الأولى
  lcd.setCursor(0, 1); // بنحدد بداية السطر التاني
  lcd.print(line2); // بنعرض الرسالة التانية
  waitMs(ms); // بنسيب الرسالة ظاهرة للمدة المطلوبة
  messageActive = false; // بنسمح بتحديث الشاشة تاني
}

// فحص الكود وتنفيذ الأمر المناسب
void checkPassword() {
  if (inputPassword.length() == 0) return; // لو المستخدم ضغط # من غير ما يكتب حاجة بنتجاهل الضغط

  // لو الإنذار شغال بنستنى كود إيقافه
  if (alarmActive) {
    if (inputPassword == ALARM_OFF_PASSWORD) {
      // لازم الشباك يكون مقفول قبل ما نوقف الإنذار
      if (windowIsOpen()) {
        showMessage("Close the Window", "then enter code", 2000);
      } else {
        alarmActive = false; // بنوقف حالة الإنذار
        windowOpenSince = 0; // بنصفر وقت فتح الشباك
        failedAttempts = 0; // بنصفر عداد المحاولات الغلط
        buzzerOff(); // بنطفي البزر
        showMessage("   Alarm Off    ", "", 2000); // بنعرض إن الإنذار اتوقف
      }
    } else {
      showMessage("     Wrong      ", "", 1500); // بنعرض رسالة لو كود إيقاف الإنذار غلط
    }
  } else {
    // لو الكود 1 بنفعل وضع الأمان
    if (inputPassword == "1") {
      securityArmed = true;
      showMessage(" Security Mode  ", "     ARMED      ", 2000);
    }
    // لو الكود 0 بنلغي تفعيل وضع الأمان
    else if (inputPassword == "0") {
      securityArmed = false;
      showMessage(" Security Mode  ", "    DISARMED    ", 2000);
    }
    // لو الكود صح بنفتح الباب
    else if (inputPassword == CORRECT_PASSWORD) {
      failedAttempts = 0; // بنصفر عدد المحاولات الغلط
      openDoor(); // بنفتح الباب
      showMessage("   Right Pin    ", "   Door Open    ", 2000); // بنأكد إن الباب اتفتح
    }
    // لو الكود غلط بنزود عداد المحاولات
    else {
      failedAttempts++;
      // لو عدد المحاولات وصل للحد المسموح بنشغل الإنذار
      if (failedAttempts >= MAX_WRONG_ATTEMPTS) {
        failedAttempts = 0; // بنرجع العداد للصفر
        alarmActive = true; // بنفعل الإنذار، وإيقافه بيكون بالكود 9999
        buzzerOn(); // بنشغل البزر
        showMessage(" Too Many Tries ", "     ALARM!     ", 2000); // بنعرض إن المحاولات خلصت والإنذار اشتغل
      } else {
        char buf[17];
        // بنجهز رسالة بعدد المحاولات اللي لسه فاضلة
        snprintf(buf, sizeof(buf), "  Tries left: %d ", MAX_WRONG_ATTEMPTS - failedAttempts);
        showMessage("  Invalid Code  ", buf, 1500); // بنعرض إن الكود غلط وعدد المحاولات المتبقية
      }
    }
  }

  resetPasswordScreen(); // بعد ما نخلص بنرجع لشاشة إدخال الكود أو شاشة الحالة
}
