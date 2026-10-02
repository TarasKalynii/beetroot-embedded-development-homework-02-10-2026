#include <Arduino.h>

// ==========================================
// КОНФІГУРАЦІЯ
// ==========================================

// Всі параметри програми винесені в одну структуру.
// static constexpr означає, що:
// 1. Не потрібно створювати об'єкт Config.
// 2. Значення відомі ще під час компіляції.
// 3. У коді немає "магічних чисел".
struct Config
{
    static constexpr uint8_t LED_PIN = 5;
    static constexpr uint8_t BUTTON_PIN = 16;

    // Інтервал перемикання LED під час блимання
    static constexpr uint32_t BLINK_INTERVAL_MS = 500;

    // Час антидребезгу кнопки
    static constexpr uint32_t DEBOUNCE_MS = 50;

    // Швидкість Serial Monitor
    static constexpr uint32_t SERIAL_BAUD = 115200;

    // Як часто виводити статистику superloop
    static constexpr uint32_t STATS_INTERVAL_MS = 2000;
};

// ==========================================
// СТАН LED
// ==========================================

enum class LedState
{
    Off,
    On
};

// ==========================================
// РЕЖИМ РОБОТИ LED
// ==========================================

enum class LedMode
{
    Blinking,
    AlwaysOn,
    AlwaysOff
};

// ==========================================
// КЛАС LED
// ==========================================

class Led
{
public:
    // Конструктор отримує номер GPIO.
    explicit Led(uint8_t pin)
        : pin_(pin)
    {
    }

    // Ініціалізація LED
    void init()
    {
        pinMode(pin_, OUTPUT);

        // Після запуску LED вимкнений
        set(LedState::Off);
    }

    // Встановлення стану LED
    void set(LedState state)
    {
        if (state == LedState::On)
        {
            digitalWrite(pin_, HIGH);
        }
        else
        {
            digitalWrite(pin_, LOW);
        }
    }

private:
    // Пін після створення об'єкта не змінюється
    const uint8_t pin_;
};

// ==========================================
// ОТРИМАННЯ ОБ'ЄКТА LED
// ==========================================

// static локальний об'єкт створюється тільки один раз
// і живе до кінця роботи програми.
//
// Так ми уникаємо зайвої глобальної змінної.
Led &getLed()
{
    static Led led(Config::LED_PIN);

    return led;
}

// ==========================================
// ЗМІННА ДЛЯ ISR
// ==========================================

// volatile потрібен, оскільки ця змінна
// змінюється асинхронно у функції переривання.
//
// ISR виставляє true,
// а loop() потім обробляє подію.
volatile bool buttonPressed = false;

// ==========================================
// ISR КНОПКИ
// ==========================================

// Interrupt Service Routine повинна бути
// максимально короткою.
//
// Тут НЕ робимо:
// Serial.print()
// delay()
// debounce
// зміну режимів
//
// Тільки встановлюємо прапорець.
void IRAM_ATTR buttonISR()
{
    buttonPressed = true;
}

// ==========================================
// SETUP
// ==========================================

void setup()
{
    // ------------------------------------------
    // SERIAL
    // ------------------------------------------

    Serial.begin(Config::SERIAL_BAUD);

    // ------------------------------------------
    // LED
    // ------------------------------------------

    getLed().init();

    // ------------------------------------------
    // BUTTON
    // ------------------------------------------

    // Кнопка підключена:
    //
    // GPIO 16 ---- BUTTON ---- GND
    //
    // Використовуємо внутрішній pull-up.
    //
    // Кнопка НЕ натиснута:
    // GPIO = HIGH
    //
    // Кнопка натиснута:
    // GPIO = LOW
    pinMode(Config::BUTTON_PIN, INPUT_PULLUP);

    // При натисканні сигнал переходить
    // HIGH -> LOW.
    //
    // Тому використовуємо FALLING.
    attachInterrupt(
        digitalPinToInterrupt(Config::BUTTON_PIN),
        buttonISR,
        FALLING);

    // ------------------------------------------
    // START MESSAGE
    // ------------------------------------------

    Serial.println();
    Serial.println("================================");
    Serial.println("Embedded C++ LED Superloop");
    Serial.println("================================");
    Serial.println("Initial mode: BLINKING");
    Serial.println();
}

// ==========================================
// LOOP / SUPERLOOP
// ==========================================

void loop()
{
    // ==========================================
    // ПОЧАТОК ВИМІРЮВАННЯ SUPERLOOP
    // ==========================================

    // micros() повертає час у мікросекундах.
    const uint32_t loopStartUs = micros();

    // ==========================================
    // STATIC ЗМІННІ
    // ==========================================

    // static локальні змінні створюються один раз
    // і зберігають своє значення між
    // викликами loop().

    // Поточний режим LED
    static LedMode mode = LedMode::Blinking;

    // Поточний стан LED під час blinking
    static LedState blinkState = LedState::Off;

    // Час останнього перемикання LED
    static uint32_t lastBlinkMs = 0;

    // Час останнього прийнятого натискання кнопки
    static uint32_t lastButtonMs = 0;

    // Час останнього виведення статистики
    static uint32_t lastStatsMs = 0;

    // Загальний час виконання loop()
    // за поточний період
    static uint64_t totalLoopTimeUs = 0;

    // Кількість виконаних ітерацій loop()
    static uint32_t loopCounter = 0;

    // ==========================================
    // ПОТОЧНИЙ ЧАС
    // ==========================================

    const uint32_t currentMs = millis();

    // Отримуємо LED
    Led &led = getLed();

    // ==========================================
    // 1. ОБРОБКА КНОПКИ
    // ==========================================

    // Якщо ISR зафіксував натискання
    if (buttonPressed)
    {
        // Забираємо подію
        buttonPressed = false;

        // ======================================
        // DEBOUNCE
        // ======================================

        // Перевіряємо, чи минуло достатньо часу
        // від попереднього прийнятого натискання.
        if (currentMs - lastButtonMs >= Config::DEBOUNCE_MS)
        {
            lastButtonMs = currentMs;

            // ==================================
            // ЗМІНА РЕЖИМУ
            // ==================================

            switch (mode)
            {
                // ----------------------------------
                // BLINKING -> ALWAYS ON
                // ----------------------------------

                case LedMode::Blinking:
                {
                    mode = LedMode::AlwaysOn;

                    led.set(LedState::On);

                    Serial.println();
                    Serial.println("Button pressed");
                    Serial.println("Mode: ALWAYS ON");

                    break;
                }

                // ----------------------------------
                // ALWAYS ON -> ALWAYS OFF
                // ----------------------------------

                case LedMode::AlwaysOn:
                {
                    mode = LedMode::AlwaysOff;

                    led.set(LedState::Off);

                    Serial.println();
                    Serial.println("Button pressed");
                    Serial.println("Mode: ALWAYS OFF");

                    break;
                }

                // ----------------------------------
                // ALWAYS OFF -> BLINKING
                // ----------------------------------

                case LedMode::AlwaysOff:
                {
                    mode = LedMode::Blinking;

                    // Починаємо блимання зі стану OFF
                    blinkState = LedState::Off;

                    led.set(blinkState);

                    // Запам'ятовуємо час початку
                    // нового циклу blinking
                    lastBlinkMs = currentMs;

                    Serial.println();
                    Serial.println("Button pressed");
                    Serial.println("Mode: BLINKING");

                    break;
                }
            }
        }
    }

    // ==========================================
    // 2. КЕРУВАННЯ LED
    // ==========================================

    // delay() НЕ використовується.
    //
    // loop() постійно виконується,
    // а ми тільки перевіряємо,
    // чи пройшов необхідний час.

    if (mode == LedMode::Blinking)
    {
        // Перевіряємо, чи пройшло 500 мс
        if (currentMs - lastBlinkMs >= Config::BLINK_INTERVAL_MS)
        {
            // Запам'ятовуємо час
            lastBlinkMs = currentMs;

            // Перемикаємо стан LED
            if (blinkState == LedState::Off)
            {
                blinkState = LedState::On;
            }
            else
            {
                blinkState = LedState::Off;
            }

            // Встановлюємо новий стан
            led.set(blinkState);
        }
    }

    // ==========================================
    // 3. КІНЕЦЬ ВИМІРЮВАННЯ SUPERLOOP
    // ==========================================

    const uint32_t loopEndUs = micros();

    // Час виконання однієї ітерації loop()
    const uint32_t loopTimeUs =
        loopEndUs - loopStartUs;

    // Додаємо результат до загального часу
    totalLoopTimeUs += loopTimeUs;

    // Збільшуємо кількість ітерацій
    loopCounter++;

    // ==========================================
    // 4. СТАТИСТИКА SUPERLOOP
    // ==========================================

    // Виводимо статистику тільки раз на 2 секунди,
    // щоб Serial Monitor не був завалений логами.
    if (currentMs - lastStatsMs >= Config::STATS_INTERVAL_MS)
    {
        lastStatsMs = currentMs;

        // Рахуємо середній час loop()
        const float averageLoopTimeUs =
            static_cast<float>(totalLoopTimeUs) /
            static_cast<float>(loopCounter);

        Serial.printf(
            "Loop: avg %.2f us | iterations: %u\n",
            averageLoopTimeUs,
            loopCounter);

        // Обнуляємо статистику
        // і починаємо новий період вимірювання
        totalLoopTimeUs = 0;
        loopCounter = 0;
    }
}