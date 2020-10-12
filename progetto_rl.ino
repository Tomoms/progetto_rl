#include <LiquidCrystal_I2C.h>
#include <TimeLib.h>

#define TIME_INTERRUPT  2
#define ALARM_INTERRUPT 3
#define ADJ_BUTTON      4
#define BUZZER          12

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2);
time_t t;
int h = -1, m = -1, s = -1;
int alarm_h, alarm_m;
String sep = String(':');
String zero = String('0');
volatile short time_mode = 0, alarm_mode = 0;
unsigned long last_press;
bool alarm_set = 0, alarm_ringing = 0, clear_alarm = 0;

String build_string(int n)
{
  String ret = String(n);
  if (ret.length() == 1)
    ret = zero + ret;
  return ret;
}

bool check_last_press()
{
  if ((millis() - last_press) < 500)
    return 0;

  last_press = millis();
  return 1;
}

void time_ISR(void)
{
  if (alarm_mode)
    return;

  if (check_last_press()) {
    if (++time_mode == 4)
      time_mode = 0;
  }
}

void alarm_ISR(void)
{
  if (time_mode)
    return;

  if (check_last_press()) {
    if (alarm_ringing) {
      alarm_ringing = 0;
      return;
    }
    if (!alarm_mode && alarm_set) {
      clear_alarm = 1;
    } else if (++alarm_mode == 3) {
      alarm_mode = 0;
    }
  }
}

void ring()
{
  alarm_ringing = 1;
  digitalWrite(BUZZER, HIGH);
  while (alarm_ringing) {
    tone(11, 1000); // Replace hardcoded pwm pin
    delay(500);
    noTone(11);
    delay(500);
  }
  digitalWrite(BUZZER, LOW);
}

bool blink_number(int val, int col, int row)
{
  unsigned long a = millis();
  lcd.setCursor(col, row);
  lcd.print("  ");

  while ((millis() - a) < 500) {
    if (digitalRead(ADJ_BUTTON) == HIGH) {
      last_press = millis();
      return 1;
    }
  }
  a = millis();

  lcd.setCursor(col, row);
  lcd.print(build_string(val));

  while ((millis() - a) < 500) {
    if (digitalRead(ADJ_BUTTON) == HIGH) {
      last_press = millis();
      return 1;
    }
  }

  return 0;
}

int h_incr()
{
  if (++h == 24)
    h = 0;
  return h;
}

int m_s_incr(int n)
{
  if (++n == 60)
    n = 0;
  return n;
}

void set_time()
{
  while (1) {
    if (time_mode == 1) {
      if (blink_number(h, 0, 0))
        h = h_incr();
    } else if (time_mode == 2) {
      if (blink_number(m, 3, 0))
        m = m_s_incr(m);
    } else if (time_mode == 3) {
      if (blink_number(s, 6, 0))
        s = m_s_incr(s);
    } else {
      setTime(h, m, s, day(t), month(t), year(t));
      return;
    }
    while ((millis() - last_press) < 300) {}
      last_press = millis();
  }
}

void unset_alarm()
{
  alarm_set = 0;
  lcd.setCursor(9, 1);
  lcd.print("      ");
  clear_alarm = 0;
}

void set_alarm()
{
  if (!alarm_set) {
    alarm_h = h;
    alarm_m = m;
    print_alarm();
  }
  while (1) {
    if (alarm_mode == 1) {
      if (blink_number(alarm_h, 9, 1))
        alarm_h = h_incr();
    } else if (alarm_mode == 2) {
      if (blink_number(alarm_m, 12, 1))
        alarm_m = m_s_incr(alarm_m);
    } else {
      print_alarm();
      alarm_set = 1;
      return;
    }
    while ((millis() - last_press) < 300) {}
    last_press = millis();
  }
}

void print_alarm()
{
  String h_str = build_string(alarm_h), m_str = build_string(alarm_m), shown;
  shown = String(h_str);
  shown.concat(sep);
  shown.concat(m_str);
  lcd.setCursor(9, 1);
  lcd.print(shown);
}

void print_time()
{
  String h_str = build_string(h), m_str = build_string(m), s_str = build_string(s), shown;
  shown = String(h_str);
  shown.concat(sep);
  shown.concat(m_str);
  shown.concat(sep);
  shown.concat(s_str);
  lcd.setCursor(0, 0);
  lcd.print(shown);
}

void setup()
{
  lcd.init();
  lcd.backlight();
  pinMode(TIME_INTERRUPT, INPUT);
  pinMode(ALARM_INTERRUPT, INPUT);
  pinMode(ADJ_BUTTON, INPUT);
  pinMode(BUZZER, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(TIME_INTERRUPT), time_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(ALARM_INTERRUPT), alarm_ISR, RISING);
  Serial.begin(9600);
  lcd.setCursor(0, 1);
  lcd.print("SVEGLIA: ");
  lcd.setCursor(0, 0);
}

void loop()
{
  t = now();
  if (second(t) != s) {
    s = second(t);
    m = minute(t);
    h = hour(t);
    print_time();
  }
  if (time_mode)
    set_time();
  if (alarm_mode)
    set_alarm();
  else if (clear_alarm)
    unset_alarm();
  if (alarm_set && !s && (alarm_m == m) && (alarm_h == h))
    ring();
}
