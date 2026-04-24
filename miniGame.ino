#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.h>
#include <EEPROM.h>
#include <string.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int pinX = A0;
const int pinY = A1;
const int pinSW = 2;

const int IR_PIN = 11;

const char* games[] = {
  "Higher/Lower",
  "Morse Trainer",
  "Rhythm Tap",
  "Lucky Dice"
};

int selectedGame = 0;
bool joyMoved = false;

// --- LEDs et boutons ---
const int ledPins[] = {50, 52, 53, 51, 49};
const int buttonPins[] = {12, 10, 8, 6, 4};
const int NUM_LEDS = 5;



// --- Mapping IR ---
int decodeIRDigit(unsigned long code) {
  switch (code) {
    case 0xE916FF00UL: return 0;
    case 0xF30CFF00UL: return 1;
    case 0xE718FF00UL: return 2;
    case 0xA15EFF00UL: return 3;
    case 0xF708FF00UL: return 4;
    case 0xE31CFF00UL: return 5;
    case 0xA55AFF00UL: return 6;
    case 0xBD42FF00UL: return 7;
    case 0xAD52FF00UL: return 8;
    case 0xB54AFF00UL: return 9;
    default:
      return -1;
  }
}


//---------------------------------------------
//    Higher/Lower Record 
//---------------------------------------------
const int EEPROM_HL_ADDR = 0;
uint8_t bestHL; // 0xFF ou 0 = aucun

void saveBestHL() {
  EEPROM.put(EEPROM_HL_ADDR, bestHL);
}

bool updateBestHL(uint8_t attempts) {
  if (bestHL == 0 || bestHL == 0xFF) {
    bestHL = attempts;
    saveBestHL();
    return true;
  }
  if (attempts < bestHL) {
    bestHL = attempts;
    saveBestHL();
    return true;
  }
  return false;
}

//-------------------------------------
//     Morse Record 
//-------------------------------------
const int EEPROM_MORSE_ADDR = 10;
uint8_t bestMorse;

void saveBestMorse() {
  EEPROM.put(EEPROM_MORSE_ADDR, bestMorse);
}

bool updateBestMorse(uint8_t streak) {
  if (bestMorse == 0xFF) bestMorse = 0;
  if (streak > bestMorse) {
    bestMorse = streak;
    saveBestMorse();
    return true;
  }
  return false;
}

// -----------------------------------
//        Rhythm Record 
// -----------------------------------
const int EEPROM_RHYTHM_ADDR = 20;
int bestRhythm;

void saveBestRhythm() {
  EEPROM.put(EEPROM_RHYTHM_ADDR, bestRhythm);
}

bool updateBestRhythm(int score) {
  if (score > bestRhythm) {
    bestRhythm = score;
    saveBestRhythm();
    return true;
  }
  return false;
}

// -----------------------------------
//        Dice Record 
// -----------------------------------
const int EEPROM_DICE_ADDR = 30;
int bestDice;

void saveBestDice() {
  EEPROM.put(EEPROM_DICE_ADDR, bestDice);
}

bool updateBestDice(int tokens) {
  if (tokens > bestDice) {
    bestDice = tokens;
    saveBestDice();
    return true;
  }
  return false;
}

const int EEPROM_MAGIC_ADDR = 50;
const uint8_t EEPROM_MAGIC_VALUE = 0x42;

void resetAllRecords() {
  bestHL = 0;
  saveBestHL();
  bestMorse = 0;
  saveBestMorse();
  bestRhythm = 0;
  saveBestRhythm();
  bestDice = 0;
  saveBestDice();
  EEPROM.put(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
}

void loadRecords() {
  uint8_t magic;
  EEPROM.get(EEPROM_MAGIC_ADDR, magic);
  if (magic != EEPROM_MAGIC_VALUE) {
    resetAllRecords();
    return;
  }

  uint8_t b;
  EEPROM.get(EEPROM_HL_ADDR, b);
  if (b == 0xFF) bestHL = 0;
  else bestHL = b;

  EEPROM.get(EEPROM_MORSE_ADDR, b);
  if (b == 0xFF) bestMorse = 0;
  else bestMorse = b;

  int v;
  EEPROM.get(EEPROM_RHYTHM_ADDR, v);
  if (v < 0 || v > 500) bestRhythm = 0;
  else bestRhythm = v;

  EEPROM.get(EEPROM_DICE_ADDR, v);
  if (v < 0 || v > 9999) bestDice = 0;
  else bestDice = v;
}

// --- États du programme ---
enum State {
  MENU,
  GAME_HL,
  GAME_MORSE,
  GAME_RHYTHM,
  GAME_DICE
};

State currentState = MENU;

// --- Setup / Loop ---
void setup() {
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Welcome to:");
  lcd.setCursor(0, 1);
  lcd.print(" RetroBox!");
  delay(2000);
  lcd.clear();

  pinMode(pinSW, INPUT_PULLUP);
  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(ledPins[i], OUTPUT);
    pinMode(buttonPins[i], INPUT_PULLUP);
    digitalWrite(ledPins[i], LOW);
  }

  Serial.begin(115200);
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);
  randomSeed(analogRead(A2));

  loadRecords();
  showMenu();
}

void loop() {
  if (currentState == MENU) {
    handleMenu();
  } else if (currentState == GAME_HL) {
    playHigherLower();
    currentState = MENU;
    showMenu();
  } else if (currentState == GAME_MORSE) {
    playMorse();
    currentState = MENU;
    showMenu();
  } else if (currentState == GAME_RHYTHM) {
    playRhythm();
    currentState = MENU;
    showMenu();
  } else if (currentState == GAME_DICE) {
    playDice();
    currentState = MENU;
    showMenu();
  }
}

// -----------------------------------
//        Menu principal
// -----------------------------------
void handleMenu() {
  int yVal = analogRead(pinY);
  int swVal = digitalRead(pinSW);

  if (!joyMoved) {
    if (yVal > 800) {
      selectedGame--;
      if (selectedGame < 0) selectedGame = 3;
      showMenu();
      joyMoved = true;
    } else if (yVal < 200) {
      selectedGame++;
      if (selectedGame > 3) selectedGame = 0;
      showMenu();
      joyMoved = true;
    }
  }

  if (yVal >= 400 && yVal <= 600) {
    joyMoved = false;
  }

  if (swVal == LOW) {

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Launching...");
    lcd.setCursor(0, 1);
    lcd.print(games[selectedGame]);
    delay(800);
    lcd.clear();

    if (selectedGame == 0) currentState = GAME_HL;
    else if (selectedGame == 1) currentState = GAME_MORSE;
    else if (selectedGame == 2) currentState = GAME_RHYTHM;
    else if (selectedGame == 3) currentState = GAME_DICE;

    while (digitalRead(pinSW) == LOW) {}
  }
}

void showMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Choose your game:");
  lcd.setCursor(0, 1);
  lcd.print(games[selectedGame]);
}

// -----------------------------------
//        Higher / Lower
// -----------------------------------
void playHigherLower() {
  int target = random(1, 100);
  int attempts = 0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Guess 1-99");
  lcd.setCursor(0, 1);
  lcd.print("IR : 2 digits");
  delay(1800);

  while (true) {
    int d1 = -1;
    int d2 = -1;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter: __");

    while (d1 == -1 || d2 == -1) {
      if (IrReceiver.decode()) {
        unsigned long code = IrReceiver.decodedIRData.decodedRawData;
        int d = decodeIRDigit(code);
        IrReceiver.resume();

        if (d != -1) {
          if (d1 == -1) {
            d1 = d;
            lcd.setCursor(7, 0);
            lcd.print(d1);
            lcd.print("_");
          } else {
            d2 = d;
            lcd.setCursor(7, 0);
            lcd.print(d1);
            lcd.print(d2);
          }
          delay(200);
        }
      }
    }

    int guess = d1 * 10 + d2;
    attempts++;

    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);

    if (guess == target) {
      lcd.print("Found in ");
      lcd.print(attempts);
      delay(1800);

      lcd.clear();
      lcd.setCursor(0, 0);
      if (updateBestHL((uint8_t)attempts)) {
        lcd.print("New Record");
        lcd.setCursor(0, 1);
        lcd.print(attempts);
        lcd.print(" try");
      } else {
        lcd.print("Found in ");
        lcd.print(attempts);
        lcd.print(" try");
        lcd.setCursor(0, 1);
        lcd.print("Record: ");
        lcd.print(bestHL);
        lcd.print(" try");
      }
      delay(3500);
      return;
    } else if (guess < target) {
      lcd.print("Higher!");
    } else {
      lcd.print("Lower!");
    }
    delay(1400);
  }
}

// -----------------------------------
//        Morse Trainer
// -----------------------------------
const char* morseCodes[] = {
  ".-",    // A
  "-...",  // B
  "-.-.",  // C
  "-..",   // D
  ".",     // E
  "..-.",  // F
  "--.",   // G
  "....",  // H
  "..",    // I
  ".---",  // J
  "-.-",   // K
  ".-..",  // L
  "--",    // M
  "-.",    // N
  "---",   // O
  ".--.",  // P
  "--.-",  // Q
  ".-.",   // R
  "...",   // S
  "-",     // T
  "..-",   // U
  "...-",  // V
  ".--",   // W
  "-..-",  // X
  "-.--",  // Y
  "--.."   // Z
};

void showMorseBuf(const char* buf) {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(buf);
}

void playMorse() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Morse Trainer");
  lcd.setCursor(0, 1);
  lcd.print("B0=. B1=- B4=OK");
  delay(2500);

  int streak = 0;
  bool alive = true;

  while (alive) {
    char letter = 'A' + random(0, 26);
    const char* expected = morseCodes[letter - 'A'];

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Letter:");
    lcd.print(letter);
    lcd.print(" S:");
    lcd.print(streak);

    char buf[8] = {0};
    int idx = 0;
    showMorseBuf(buf);

    unsigned long start = millis();
    bool validated = false;

    bool timedOut = false;
    while (!validated && !timedOut) {
      if (millis() - start > 12000) {
        timedOut = true;
        break;
      }

      if (digitalRead(buttonPins[0]) == LOW && idx < 7) {
        buf[idx++] = '.';
        buf[idx] = 0;
        showMorseBuf(buf);
        delay(220);
      } else if (digitalRead(buttonPins[1]) == LOW && idx < 7) {
        buf[idx++] = '-';
        buf[idx] = 0;
        showMorseBuf(buf);
        delay(220);
      } else if (digitalRead(buttonPins[3]) == LOW && idx > 0) {
        buf[--idx] = 0;
        showMorseBuf(buf);
        delay(220);
      } else if (digitalRead(buttonPins[4]) == LOW) {
        validated = true;
        delay(220);
      }
    }

    bool matches = false;
    if (idx == (int)strlen(expected)) {
      matches = (strcmp(buf, expected) == 0);
    }

    lcd.clear();
    lcd.setCursor(0, 0);

    if (matches) {
      lcd.print("Correct!");
      lcd.setCursor(0, 1);
      lcd.print(expected);
      streak++;

      for (int i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins[i], HIGH);
      delay(600);
      for (int i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins[i], LOW);
      delay(400);
    } else {
      lcd.print("Wrong! was:");
      lcd.setCursor(0, 1);
      lcd.print(expected);
      delay(2500);
      alive = false;
    }
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  if (updateBestMorse((uint8_t)streak)) {
    lcd.print("New Record");
    lcd.setCursor(0, 1);
    lcd.print("Streak: ");
    lcd.print(streak);
  } else {
    lcd.print("Best: ");
    lcd.print(bestMorse);
    lcd.setCursor(0, 1);
    lcd.print("Streak: ");
    lcd.print(streak);
  }
  delay(3500);
}

// -----------------------------------
//        Rhythm Tap
// -----------------------------------
void playRhythm() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Rhythm Tap");
  lcd.setCursor(0, 1);
  lcd.print("Hit on beat!");
  delay(1800);

  int score = 0;
  int misses = 0;
  unsigned long interval = 900;

  while (misses < 3) {
    int target = random(NUM_LEDS);
    digitalWrite(ledPins[target], HIGH);

    unsigned long t0 = millis();
    bool hit = false;
    bool wrong = false;

    while (millis() - t0 < interval && !hit && !wrong) {
      for (int i = 0; i < NUM_LEDS; i++) {
        if (digitalRead(buttonPins[i]) == LOW) {
          if (i == target) {
            hit = true;
            score++;
          } else {
            wrong = true;
            misses++;
          }
          delay(120);
        }
      }
    }

    digitalWrite(ledPins[target], LOW);
    if (!hit && !wrong) misses++;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Score: ");
    lcd.print(score);
    lcd.setCursor(0, 1);
    lcd.print("Miss : ");
    lcd.print(misses);

    if (score > 0 && score % 10 == 0 && interval > 300) {
      interval -= 100;
    }
    delay(150);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  if (updateBestRhythm(score)) {
    lcd.print("New Record");
    lcd.setCursor(0, 1);
    lcd.print("Score: ");
    lcd.print(score);
  } else {
    lcd.print("Best: ");
    lcd.print(bestRhythm);
    lcd.setCursor(0, 1);
    lcd.print("Score: ");
    lcd.print(score);
  }
  delay(3500);
}

// -----------------------------------
//        Lucky Dice
// -----------------------------------
void playDice() {
  int tokens = 10;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Lucky Dice");
  lcd.setCursor(0, 1);
  lcd.print("10 tokens start");
  delay(2000);

  bool quit = false;

  while (tokens > 0 && !quit) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Tokens: ");
    lcd.print(tokens);
    lcd.setCursor(0, 1);
    lcd.print("Bet btn1-5 SW=x");

    int bet = 0;
    while (bet == 0 && !quit) {
      for (int i = 0; i < NUM_LEDS; i++) {
        if (digitalRead(buttonPins[i]) == LOW) {
          int v = i + 1;
          if (v <= tokens) {
            bet = v;
            delay(220);
          }
        }
      }
      if (digitalRead(pinSW) == LOW) {
        while (digitalRead(pinSW) == LOW) {}
        quit = true;
      }
    }
    if (quit) break;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Bet=");
    lcd.print(bet);
    lcd.setCursor(0, 1);
    lcd.print("IR face 1-5");

    int guess = -1;
    while (guess < 1 || guess > 5) {
      if (IrReceiver.decode()) {
        unsigned long code = IrReceiver.decodedIRData.decodedRawData;
        int d = decodeIRDigit(code);
        IrReceiver.resume();
        if (d >= 1 && d <= 5) guess = d;
      }
    }

    // animation de lancer
    for (int k = 0; k < 18; k++) {
      int r = random(NUM_LEDS);
      for (int i = 0; i < NUM_LEDS; i++) {
        if (i == r) digitalWrite(ledPins[i], HIGH);
        else digitalWrite(ledPins[i], LOW);
      }
      delay(70 + k * 5);
    }

    int roll = random(1, 6); // 1..5
    for (int i = 0; i < NUM_LEDS; i++) {
      if (i == roll - 1) digitalWrite(ledPins[i], HIGH);
      else digitalWrite(ledPins[i], LOW);
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Roll:");
    lcd.print(roll);
    lcd.print(" Bet:");
    lcd.print(guess);
    lcd.setCursor(0, 1);

    if (roll == guess) {
      tokens += bet * 2;
      lcd.print("Win +");
      lcd.print(bet * 2);
    } else {
      tokens -= bet;
      lcd.print("Lose -");
      lcd.print(bet);
    }
    delay(2500);

    for (int i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins[i], LOW);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  if (updateBestDice(tokens)) {
    lcd.print("New Record");
    lcd.setCursor(0, 1);
    lcd.print("End: ");
    lcd.print(tokens);
    lcd.print(" tok");
  } else {
    lcd.print("Best: ");
    lcd.print(bestDice);
    lcd.setCursor(0, 1);
    lcd.print("End: ");
    lcd.print(tokens);
    lcd.print(" tok");
  }
  delay(3500);
}
