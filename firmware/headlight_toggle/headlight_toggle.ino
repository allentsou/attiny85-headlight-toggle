// 機車大燈控制器（強光線組版，ATtiny85，ATTinyCore）
// 燈的電源：電瓶 +12V 經保險絲 → 兩顆 P-MOS → 燈
// 原車近燈、遠燈線：只當訊號，並經二極體供電給 MCU（熄火就斷電）
//
// 上電預設：近燈關
// 切遠燈約 2 秒 → 遠燈閃一下提示 → 立即切回近燈 → 近燈 開/關 切換
// 閃完拖太久才切回 → 視為正常使用遠燈，不切換
//
// 狀態燈（板上 LED，接 PB1）：
//   開機閃 1 下：正常開機
//   開機閃 3 下：上次是被看門狗重啟（雜訊干擾），近燈狀態會自動恢復
//   恆亮 / 不亮：近燈開 / 近燈關
//   快閃：提示閃燈後的確認時間，這時切回近燈就會切換

#include <avr/wdt.h>

const uint8_t PIN_HIGH_SENSE = A3;  // PB3：原車遠燈線（22k/10k 分壓）
const uint8_t PIN_LOW_DRIVE  = 4;   // PB4：近燈 MOSFET
const uint8_t PIN_HIGH_DRIVE = 0;   // PB0：遠燈 MOSFET
const uint8_t PIN_LED        = 1;   // PB1：狀態燈（LED 正極接 5V，輸出 LOW 才亮）
const uint8_t PIN_SPARE      = 2;   // PB2：保留

// 分壓 22k/10k，VCC 5V 當參考：ADC = 車電壓 × 10/32 ÷ 5 × 1023
const int ADC_ON  = 576;   // 約 9V 以上才算遠燈開
const int ADC_OFF = 384;   // 約 6V 以下才算遠燈關，中間維持原狀態

const unsigned long HOLD_MS     = 2000;  // 按住多久閃提示
const unsigned long CONFIRM_MS  = 800;   // 閃完後多久內切回才算數
const unsigned long BLINK_MS    = 150;   // 提示閃滅時間
const unsigned long DEBOUNCE_MS = 100;   // 切換判斷用的穩定時間
const unsigned long LED_BOOT_MS = 150;   // 狀態燈開機閃的速度
const unsigned long LED_FAST_MS = 80;    // 狀態燈快閃的速度

// 遠燈時近燈是否也亮（遠近同亮）
const bool LOW_WITH_HIGH = false;

// 這幾個變數放在 .noinit，看門狗重啟時不會被清掉，斷電才會消失
const uint8_t MAGIC = 0xA5;
uint8_t resetFlags __attribute__((section(".noinit")));
uint8_t savedLow   __attribute__((section(".noinit")));
uint8_t savedMagic __attribute__((section(".noinit")));

// 開機最早階段：記下重置原因，並關掉看門狗，否則會一直重開機
void wdt_init(void) __attribute__((naked, used, section(".init3")));
void wdt_init(void) { resetFlags = MCUSR; MCUSR = 0; wdt_disable(); }

bool lowOn = false;
bool senseState = false;   // 遲滯判斷後的遠燈狀態（燈直接跟這個，反應快）
bool highStable = false;   // 再經防彈跳（切換判斷用）
bool highLast = false;
bool blinked = false;
unsigned long lastChange = 0;
unsigned long highStart = 0;
unsigned long blinkTime = 0;
uint8_t bootBlinks = 1;

void setup() {
  digitalWrite(PIN_LED, HIGH);          // 先寫 HIGH 再設輸出，開機不會亂閃
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_LOW_DRIVE, OUTPUT);
  pinMode(PIN_HIGH_DRIVE, OUTPUT);
  pinMode(PIN_SPARE, INPUT_PULLUP);     // 沒用到的腳拉高，比較不吃雜訊
  digitalWrite(PIN_HIGH_DRIVE, LOW);
  digitalWrite(PIN_LOW_DRIVE, LOW);

  bool wdtReset = resetFlags & (1 << WDRF);
  if (wdtReset && savedMagic == MAGIC) lowOn = savedLow;   // 騎車中被重啟，近燈維持原狀
  savedLow = lowOn;
  savedMagic = MAGIC;
  bootBlinks = wdtReset ? 3 : 1;

  wdt_enable(WDTO_500MS);               // 當機 0.5 秒自動重啟
}

void updateLed(unsigned long now) {
  bool on;
  if (now < bootBlinks * 2 * LED_BOOT_MS) {
    on = (now / LED_BOOT_MS) % 2 == 0;                    // 開機閃 1 或 3 下
  } else if (highStable && blinked && now - blinkTime <= CONFIRM_MS) {
    on = (now / LED_FAST_MS) % 2 == 0;                    // 現在切回近燈就會切換
  } else {
    on = lowOn;
  }
  digitalWrite(PIN_LED, on ? LOW : HIGH);
}

void loop() {
  wdt_reset();

  int adc = analogRead(PIN_HIGH_SENSE);
  if (adc > ADC_ON)  senseState = true;
  if (adc < ADC_OFF) senseState = false;

  unsigned long now = millis();

  if (senseState != highLast) {
    highLast = senseState;
    lastChange = now;
  }

  if (now - lastChange >= DEBOUNCE_MS && senseState != highStable) {
    highStable = senseState;

    if (highStable) {
      highStart = now;                  // 切入遠燈
      blinked = false;
    } else if (blinked && now - blinkTime <= CONFIRM_MS) {
      lowOn = !lowOn;                   // 提示後立即切回 → 切換
      savedLow = lowOn;
    }
  }

  // 遠燈持續到 2 秒 → 閃一下提示
  if (highStable && !blinked && now - highStart >= HOLD_MS) {
    digitalWrite(PIN_HIGH_DRIVE, LOW);
    delay(BLINK_MS);
    blinked = true;
    blinkTime = millis();
  }

  // 輸出：遠燈跟著開關走；近燈看切換狀態，開遠燈時預設熄近燈
  bool highOut = senseState;
  bool lowOut  = lowOn && (LOW_WITH_HIGH || !senseState);
  digitalWrite(PIN_HIGH_DRIVE, highOut ? HIGH : LOW);
  digitalWrite(PIN_LOW_DRIVE,  lowOut  ? HIGH : LOW);
  updateLed(millis());
}
