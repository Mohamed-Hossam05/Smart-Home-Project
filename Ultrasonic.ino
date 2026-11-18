#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad_I2C.h>
#include <Keypad.h>

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

// هنا بنخزن الباسورد اللي هيتكتب
String inputPassword = "";
const String CORRECT_PASSWORD = "5555";

void setup() {
  Wire.begin();

  // نشغل الشاشة
  lcd.init();
  lcd.backlight();

  // نشغل الكيباد
  kpd.begin();

  // أول ما الجهاز يشتغل تظهر رسالة الترحيب
  lcd.setCursor(0, 0);
  lcd.print("   Welcome!   ");
  lcd.setCursor(0, 1);
  lcd.print(" Security System");
  delay(2500);

  // بعد الترحيب نطلب الباسورد
  resetPasswordScreen();
}

void loop() {
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

        // نمسح آخر نجمة من الشاشة
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

// نرجع الشاشة من الأول عشان نكتب الباسورد
void resetPasswordScreen() {
  inputPassword = "";
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Password:");
  lcd.setCursor(0, 1);
}

// هنا بنشوف الباسورد صح ولا غلط
void checkPassword() {
  lcd.clear();

  if (inputPassword == CORRECT_PASSWORD) {
    lcd.setCursor(0, 0);
    lcd.print("   Right Pin   ");
    delay(2000);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("     Wrong     ");
    delay(2000);
  }

  // بعد ما نخلص نرجع نطلب الباسورد تاني
  resetPasswordScreen();
}
