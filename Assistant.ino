#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include "SR04.h"
#include "pitches.h"
#include "TimerOne.h"
#define BLANK "                "

/* PART1. 各部品の宣言，初期化 */

// LCD
LiquidCrystal_I2C lcd(0x27,16,2);

// Keypad
char hexaKeys[4][4] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[4] = {9, 8, 7, 6}; 
byte colPins[4] = {A2, 4, 3, 2}; 
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, 4, 4); 

// passive buzzer
int melody[] = {
  NOTE_C5, NOTE_D5, NOTE_E5, NOTE_F5, NOTE_G5, NOTE_A5, NOTE_B5, NOTE_C6};
int duration = 300;  // 300 miliseconds

// Ultrasonic Sensor
#define TRIG_PIN 12
#define ECHO_PIN 11
SR04 sr04 = SR04(ECHO_PIN,TRIG_PIN);

// Thermistor
#define TEMP_PIN 0

// DC Motor
#define ENABLE 5
#define DIRA 10
#define DIRB 13


/* PART2. グローバル変数の宣言 */

float temperature_limit = 29.0;
int distance_limit = 100;
int time_limit = 60;  // mins
int relax_time = 60;  // secs
bool settingMode = false;
int settingModeState; // 設定モードの状態
int settingValue = 0; // キーパッドからの入力値
// 設定モードでの状態
enum {MAIN_PAGE, SET_TEMPERATURE_LIMIT, SET_DISTANCE_LIMIT, SET_TIME_LIMIT, SET_RELAX_TIME, EXIT};
// アシスタントの気持ち
enum {HAPPY, TIRED, ANGRY, DOUBT, UNHAPPY};
// 自作のキャラクター
enum {DEGREE, BELL, SOUND, MOUTH_HAPPY, EYE_HAPPY, MOUTH_ANGRY, EYE_ANGRY, SYMBOL_ANGRY};
// 自作の効果音
enum {START_SOUND, END_SOUND, REMIND_SOUND};
// LCDの2行目に表示するもの
enum {NONE, DISTANCE, TEMPERATURE};
int showLine2 = NONE; // LCDの2行目に表示する値
long distance; // 距離
float temperature; // 温度
int emotion = HAPPY; // アシスタントの気持ち
int fanNow = 0; // 今のファンの状態 0:off, 1:low speed, 2:half speed, 3:high speed, 4:自動モード
#define FAN_AUTO_MODE 4
int fanMode[4] = {0, 100, 150, 255}; // 各ファンの状態が対応しているanalogWrite関数用の値
int fanStartTime = -1; // ファンの運転開始時間
int second = 0; // 今の秒
int minute = 0; // 今の分
int prev_second = 0; // 前の秒
int prev_minute = 0; // 前の分

/* PART3. setupとloop関数 */

void setup() {
  lcd.init();
  lcd.backlight();
  playSound(START_SOUND);
  createCharacters(); // 自作のキャラクターを導入する
  Timer1.initialize(1000000); // タイマーの単位，ここで1単位=1s
  Timer1.attachInterrupt(timerIsr); // timerIsrはタイマーの単位ごとに実行する関数
  // 次の3行はDCモータ(ファン)用のpinModeを設定するためのコード
  pinMode(ENABLE,OUTPUT);
  pinMode(DIRA,OUTPUT);
  pinMode(DIRB,OUTPUT);
  // 次の2行はファンの方向を設定するためのコード
  digitalWrite(DIRA, HIGH);
  digitalWrite(DIRB, LOW);
}

void loop() {
  if (settingMode) {
    settingAction(); // 利用者の入力と設定モードの状態に基づいて動作するための関数
    updateSettingView(); // 設定モードの画面を更新する
  } else {
    keypadListener(); // キーパッドからの入力を監視し，その入力に基づいて動作するための関数
    updateDisplay(); // LCDの画面を更新する
    
    // 秒が変わるとき，次のif文の内部を実行する．すなわち，1秒間1回だけ実行する．
    if (second != prev_second) {
      distance = sr04.Distance();
      temperature = getTemperature();
      if (fanNow == FAN_AUTO_MODE) fanAutoModeAction(); // ファンの自動モードの動作
      if (leaveTableOverNSeconds(relax_time)) {
        // relax_time秒で机を離れるとき，スタート効果音を流し，タイマーをリセットする
        playSound(START_SOUND);
        minute = 0;
        second = 0;
      }
      setEmotion(); // アシスタントの気持ちを設定する
    }
    
    // 分が変わるとき，次のif文の内部を実行する．すなわち，1分間1回だけ実行する．
    if (minute != prev_minute) {
      if (minute == time_limit) {
        // 机の前にいる時間が制限時間と等しいとき
        playSound(END_SOUND);
      } else if (minute > time_limit) {
        // 机の前にいる時間が制限時間より大きいとき
        playSound(REMIND_SOUND);
      }
    }
    
    prev_second = second;
    prev_minute = minute;
  }
}

/* PART4. 様々な関数 */

// 利用者の入力と設定モードの状態に基づいて動作するための関数
void settingAction() {
  char key = customKeypad.getKey();
  if (settingModeState == MAIN_PAGE) {
    if (key) settingModeState = key - '0';
  } else if (settingModeState == EXIT) {
    settingModeState = MAIN_PAGE;
    settingMode = false;
    lcd.clear();
  } else {
    if (key == '#') {
      if (settingModeState == SET_TEMPERATURE_LIMIT) {
        temperature_limit = settingValue;
      } else if (settingModeState == SET_DISTANCE_LIMIT) {
        distance_limit = settingValue;
      } else if (settingModeState == SET_TIME_LIMIT) {
        time_limit = settingValue;
      } else if (settingModeState == SET_RELAX_TIME) {
        relax_time = settingValue;
      }
      settingValue = 0;
      settingModeState = MAIN_PAGE;
    } else if (key == '*') {
      settingValue /= 10;
    } else if (key) {
      settingValue = settingValue * 10 + key - '0';
    }
  }
}

// 設定モードの画面を更新するための関数
void updateSettingView() {
  if (!settingMode) return;
  if (settingModeState == MAIN_PAGE) {
    lcd.setCursor(0, 0);
    lcd.print("1Temp 2Dist 3t");
    lcd.setCursor(0, 1);
    lcd.print("4RelaxTime 5Exit");
  } else {
    lcd.setCursor(0, 0);
    switch (settingModeState) {
      case SET_TEMPERATURE_LIMIT:
        lcd.print("TempLimit(");
        lcd.write(DEGREE);
        lcd.print("C)");
        break;
      case SET_DISTANCE_LIMIT:
        lcd.print("DistLimit(cm)");
        break;
      case SET_TIME_LIMIT:
        lcd.print("TimeLimit(min)");
        break;
      case SET_RELAX_TIME:
        lcd.print("RelaxTime(sec)");
        break;
    }
    lcd.print(BLANK);
    lcd.setCursor(0, 1);
    lcd.print(settingValue);
    lcd.print(BLANK);
  }
}

// キーパッドからの入力を監視し，その入力に基づいて動作するための関数
void keypadListener() {
  char customKey = customKeypad.getKey();
  if (!customKey) return;
  emotion = DOUBT;
  switch (customKey) {
    case 'A':
      showLine2 = TEMPERATURE; //2行目に温度を表示する
      break;
    case 'B':
      showLine2 = DISTANCE; // 2行目に距離を表示する
      break;
    case 'C':
      showLine2 = NONE; // 温度(もしくは距離を非表示する
      break;
    case 'D':
      changeFanMode(); // ファンのモード(スピード)を変更する
      break;
    case '*':
      settingMode = true; // 設定モードに入る
      break;
    case '#':
      // タイマーをリセットする
      minute = 0;
      second = 0;
      playSound(START_SOUND); // スタート効果音を再生する
      break;
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
      // 対応している音を再生する
      tone(A1, melody[customKey-'1'], duration);
      break;
    case '9':
      // undefined
      break;
    case '0':
      // undefined
      break;
    default:
      break;
  }
}

// 温度を読み取り
float getTemperature() {
  int tempReading = analogRead(TEMP_PIN);
  double tempK = log(10000.0 * ((1024.0 / tempReading - 1)));
  tempK = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * tempK * tempK )) * tempK );
  return tempK - 273.15;
}

// アシスタントの気持ちを設定するための関数
void setEmotion() {
  if (fanStartTime != -1 && minute - fanStartTime >= 3) {
    // ファンが3分以上動いたとき
    emotion = TIRED;
  } else if (minute == time_limit) {
    emotion = UNHAPPY;
  } else if (minute > time_limit) {
    emotion = ANGRY;
  } else {
    emotion = HAPPY;
  }
}

// アシスタントの表情を表示するための関数
void showEmotion() {
  lcd.setCursor(0, 0);
  switch (emotion) {
    case HAPPY:
      lcd.print("(");
      lcd.write(EYE_HAPPY);
      lcd.write(MOUTH_HAPPY);
      lcd.write(EYE_HAPPY);
      lcd.print(")");
      lcd.write(SOUND);
      break;
    case TIRED:
      lcd.print("(>_<) ");
      break;
    case ANGRY:
      lcd.print("|");
      lcd.write(EYE_ANGRY);
      lcd.write(MOUTH_ANGRY);
      lcd.write(EYE_ANGRY);
      lcd.print("|");
      lcd.write(SYMBOL_ANGRY);
      break;
    case UNHAPPY:
      lcd.print("|O_O| ");
      break;
    case DOUBT:
      lcd.print("|O_O|?");
      break;
    default:
      break;
  }
}

// LCDの表示内容を更新するための関数
void updateDisplay() {
  displayLine1();
  displayLine2();
}

// LCDの1行目を更新するための関数
void displayLine1() {
  showEmotion();
  // display information of fan
  lcd.setCursor(11, 0);
  if (fanNow == 0) {
    lcd.print(BLANK);
    return;
  }
  lcd.print("fan:");
  if (fanNow == FAN_AUTO_MODE) {
    lcd.print("A");
  } else {
    lcd.print(fanNow);
  }
}

// LCDの2行目を更新するための関数
void displayLine2() {
  lcd.setCursor(0, 1);
  lcd.print(minute);
  lcd.print(":");
  lcd.print(second < 10 ? "0" : "");
  lcd.print(second);
  lcd.print("    ");
  switch (showLine2) {
    case DISTANCE:
      lcd.print(" ");
      lcd.print(distance);
      lcd.print("cm");
      break;
    case TEMPERATURE:
      lcd.print(temperature);
      lcd.write(DEGREE);
      lcd.print("C");
      break;
    case NONE:
      lcd.print(BLANK);
      break;
    default:
      break;
  }
  lcd.print(BLANK);
}

// 机をn秒離れているかどうかを判断するための関数
int leaveTableCnt = 0;
bool leaveTableOverNSeconds(int n) {
  if (distance > distance_limit) {
    leaveTableCnt++;
  } else {
    leaveTableCnt = 0;
  }
  return leaveTableCnt > n;
}

// ファンのモードを変更するための関数
void changeFanMode() {
  fanNow++;
  fanNow %= 5;
  if (fanNow != FAN_AUTO_MODE) {
    if (fanNow == 0) 
      fanStartTime = -1;
    else 
      fanStartTime = minute;
    analogWrite(ENABLE, fanMode[fanNow]);
  }
}

// ファンの自動モードの動作を定義するための関数
void fanAutoModeAction() {
  if (temperature > temperature_limit && distance < distance_limit) {
    if (fanStartTime == -1) fanStartTime = minute;
    digitalWrite(ENABLE, HIGH);
  } else {
    if (fanStartTime != -1) fanStartTime = -1;
    digitalWrite(ENABLE, LOW);
  }
}

// 効果音を定義し，再生するための関数
void playSound(int sound) {
  switch(sound) {
    case START_SOUND:
      tone(A1, melody[0], duration);
      delay(duration+100);
      tone(A1, melody[2], duration);
      delay(duration+100);
      tone(A1, melody[4], duration);
      delay(duration+100);
      tone(A1, melody[7], duration);
      break;
    case END_SOUND:
      tone(A1, melody[7], duration);
      delay(duration+100);
      tone(A1, melody[4], duration);
      delay(duration+100);
      tone(A1, melody[2], duration);
      delay(duration+100);
      tone(A1, melody[0], duration);
      break;
    case REMIND_SOUND:
      tone(A1, NOTE_C7, 100);
      delay(300);
      tone(A1, NOTE_C7, 100);
      delay(1000);
      tone(A1, NOTE_C7, 100);
      delay(300);
      tone(A1, NOTE_C7, 100);
      break;
  }
}

// 毎秒に実行する
// 時間計測用の関数
void timerIsr() {
  second++;
  if (second > 59) {
    second = 0;
    minute++;
  }
}

/* PART5. 自作のキャラクター */

byte Degree[] = {
  B00111,
  B00101,
  B00111,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};

byte Bell[] = {
  B00100,
  B01110,
  B01110,
  B01110,
  B11111,
  B00000,
  B00100,
  B00000
};

byte Sound[] = {
  B00001,
  B00011,
  B00101,
  B01001,
  B01001,
  B01011,
  B11011,
  B11000
};

byte MouthHappy[] = {
  B00000,
  B00000,
  B00000,
  B11111,
  B10001,
  B10001,
  B01010,
  B00100
};

byte EyeHappy[] = {
  B01110,
  B10001,
  B10001,
  B10001,
  B10001,
  B00000,
  B00000,
  B00000
};

byte MouthAngry[] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B00100,
  B01010,
  B10001,
  B10001
};

byte EyeAngry[] = {
  B10101,
  B11111,
  B11111,
  B01110,
  B00000,
  B00000,
  B00000,
  B00000
};

byte SymbolAngry[] = {
  B01010,
  B11011,
  B00000,
  B11011,
  B01010,
  B00000,
  B00000,
  B00000
};

void createCharacters() {
  lcd.createChar(DEGREE, Degree);
  lcd.createChar(BELL, Bell);
  lcd.createChar(SOUND, Sound);
  lcd.createChar(MOUTH_HAPPY, MouthHappy);
  lcd.createChar(EYE_HAPPY, EyeHappy);
  lcd.createChar(MOUTH_ANGRY, MouthAngry);
  lcd.createChar(EYE_ANGRY, EyeAngry);
  lcd.createChar(SYMBOL_ANGRY, SymbolAngry);
}
