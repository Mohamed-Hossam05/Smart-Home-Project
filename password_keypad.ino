#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <Servo.h>

// توصيلات الحساس والسيرفو والبزر
const byte WINDOW_PIN = 2;     // حساس الشباك MC-38
const byte SERVO_PIN  = 9;     // سيرفو الباب
const byte BUZZER_PIN = 10;    // البزر

// إعدادات الحساس والبزر وحركة الباب
const bool WINDOW_OPEN_LEVEL = HIGH;   // لما الشباك يفتح القراءة تبقى HIGH، ولو العكس نخليها LOW
const bool BUZZER_ACTIVE = true;       // لو البزر active نخليها true ولو passive نخليها false
const unsigned long WINDOW_DEBOUNCE_MS = 50;

const int SERVO_CLOSED = 0;            // زاوية الباب وهو مقفول
const int SERVO_OPEN   = 90;           // زاوية الباب وهو مفتوح
const unsigned long DOOR_OPEN_TIME = 5000;   // الباب يفضل مفتوح 5 ثواني

// الشاشة
LiquidCrystal_I2C lcd(0x27, 16, 2);

// عدد الصفوف والأعمدة بتوع الكيباد
const byte ROWS = 4;
const byte COLS = 3;

// شكل الأزرار على الكيباد
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

// توصيلات الصفوف والأعمدة
byte rowPins[ROWS] = {0, 1, 2, 4};
byte colPins[COLS] = {5, 6, 7};

// عنوان الكيباد
byte keypad_address = 0x26;

// تعريف الكيباد
Keypad_I2C kpd(makeKeymap(keys), rowPins, colPins, ROWS, COLS, keypad_address);

// تعريف السيرفو
Servo doorServo;

// هنا بنحدد الباسوردات
String inputPassword = "";
const String CORRECT_PASSWORD   = "5555";   // الباسورد العادي
const String ALARM_OFF_PASSWORD = "9999";   // الباسورد اللي بيوقف الإنذار

// بنخزن حالة الإنذار والباب
bool alarmActive = false;
unsigned long windowOpenSince = 0;

bool doorOpen = false;
unsigned long doorOpenSince = 0;

// تشغيل البزر
void buzzerOn() {
  if (BUZZER_ACTIVE) digitalWrite(BUZZER_PIN, HIGH);
  else tone(BUZZER_PIN, 2000);
}

// إيقاف البزر
void buzzerOff() {
  if (BUZZER_ACTIVE) digitalWrite(BUZZER_PIN, LOW);
  else noTone(BUZZER_PIN);
}

// فتح الباب
void openDoor() {
  doorServo.write(SERVO_OPEN);
  doorOpen = true;
  doorOpenSince = millis();      // لو الباب اتفتح تاني بنبدأ حساب الخمس ثواني من جديد
}

// نشوف إذا كان وقت قفل الباب جه ولا لسه
void updateDoor() {
  if (doorOpen && millis() - doorOpenSince >= DOOR_OPEN_TIME) {
    doorServo.write(SERVO_CLOSED);
    doorOpen = false;
  }
}

// نشوف الشباك مفتوح ولا مقفول
bool windowIsOpen() {
  return digitalRead(WINDOW_PIN) == WINDOW_OPEN_LEVEL;
}

// تشغيل الإنذار لما الشباك يتفتح
void startAlarm() {
  alarmActive = true;
  buzzerOn();
  resetPasswordScreen();      // نعرض رسالة الإنذار على الشاشة
}

void checkWindow() {
  if (alarmActive) return;
  if (windowIsOpen()) {
    if (windowOpenSince == 0) windowOpenSince = millis();
    else if (millis() - windowOpenSince >= WINDOW_DEBOUNCE_MS) startAlarm();
  } else {
    windowOpenSince = 0;
  }
}

// الحاجات اللي بنحتاج نتابعها طول ما الجهاز شغال
void backgroundTasks() {
  checkWindow();
  updateDoor();
}

// الانتظار من غير ما نوقف متابعة الشباك والباب
void waitMs(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    backgroundTasks();
  }
}

void setup() {
  Wire.begin();

  // تجهيز الحساس والبزر
  pinMode(WINDOW_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  buzzerOff();

  // تجهيز السيرفو وخليه على وضع قفل الباب
  doorServo.attach(SERVO_PIN);
  doorServo.write(SERVO_CLOSED);

  // تشغيل الشاشة
  lcd.init();
  lcd.backlight();

  // تشغيل الكيباد
  kpd.begin();

  // أول ما الجهاز يشتغل تظهر رسالة الترحيب
  lcd.setCursor(0, 0);
  lcd.print("   Welcome!   ");
  lcd.setCursor(0, 1);
  lcd.print(" Security System");
  waitMs(2500);

  // بعد الترحيب نروح لشاشة الباسورد
  resetPasswordScreen();
}

void loop() {
  // متابعة الشباك والباب طول ما الجهاز شغال
  backgroundTasks();

  // نشوف المستخدم ضغط على أنهي زرار
  char key = kpd.getKey();

  if (key) {
    if (key == '#') {
      // لما يدوس شباك نتأكد من الباسورد
      checkPassword();
    }
    else if (key == '*') {
      // لما يدوس نجمة نمسح آخر رقم
      if (inputPassword.length() > 0) {
        inputPassword.remove(inputPassword.length() - 1);
        lcd.setCursor(inputPassword.length(), 1);
        lcd.print(" ");
        lcd.setCursor(inputPassword.length(), 1);
      }
    }
    else {
      // أي رقم يكتبه يظهر مكانه نجمة
      if (inputPassword.length() < 16) {
        inputPassword += key;
        lcd.setCursor(inputPassword.length() - 1, 1);
        lcd.print("*");
      }
    }
  }
}

// نجهز الشاشة ونكتب الرسالة المناسبة
void resetPasswordScreen() {
  inputPassword = "";
  lcd.clear();
  lcd.setCursor(0, 0);
  if (alarmActive) lcd.print("ALARM! Enter PIN");
  else             lcd.print("Enter Password:");
  lcd.setCursor(0, 1);
}

// نعرض رسالة على الشاشة شوية وبعدين نكمل
void showMessage(const char* line1, const char* line2, unsigned long ms) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
  waitMs(ms);
}

// هنا بنشوف الباسورد صح ولا غلط
void checkPassword() {
  if (alarmActive) {
    // لو الإنذار شغال لازم يدخل 9999 عشان يوقفه
    if (inputPassword == ALARM_OFF_PASSWORD) {
      if (windowIsOpen()) {
        showMessage("Close the Window", "then enter code", 2000);
      } else {
        alarmActive = false;
        windowOpenSince = 0;
        buzzerOff();
        showMessage("   Alarm Off   ", "", 2000);
      }
    } else {
      showMessage("     Wrong     ", "", 1500);
    }
  } else {
    // لو مفيش إنذار بنستخدم الباسورد العادي لفتح الباب
    if (inputPassword == CORRECT_PASSWORD) {
      openDoor();
      showMessage("   Right Pin   ", "   Door Open   ", 2000);
    } else {
      showMessage("     Wrong     ", "", 2000);
    }
  }

  // بعد ما نخلص نرجع نطلب الباسورد تاني
  resetPasswordScreen();
}
