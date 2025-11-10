/* 
  Serial assignment of matrix LEDs with brightness argument for PWN
    Input of the format 0 0 15
      First two args inidices of desired pin x y
      And third arg being brightness level 0-15

    Weston Keller
    Fall 2025
    ENGR40M
 */

// Arrays of pin numbers
const byte ANODE_PINS[8] = {13, 12, 11, 10, 9, 8, 7, 6};
const byte CATHODE_PINS[8] = {A3, A2, A1, A0, 5, 4, 3, 2};

// Time per brightness level in microseconds
const int BRIGHTNESS_UNIT = 1; // 60us per brightness level

void setup() {
  for (byte i = 0; i < 8; i++) {
    pinMode(ANODE_PINS[i], OUTPUT);
    pinMode(CATHODE_PINS[i], OUTPUT);
    digitalWrite(ANODE_PINS[i], HIGH);
    digitalWrite(CATHODE_PINS[i], HIGH); // turn all pins OFF
  }

  // Initialize serial
  Serial.begin(115200);
  Serial.setTimeout(100);
}

/* Function: display
 */
void display(byte pattern[8][8]) {
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      byte brightness = pattern[i][j];
      
      if (brightness > 0) {
        // Exponential scaling gives smoother ramp from off to full bright
        // brightness 1: 1μs, brightness 15: 225μs
        int scaledTime = brightness * brightness;
        
        digitalWrite(CATHODE_PINS[j], LOW);
        digitalWrite(ANODE_PINS[i], LOW);
        delayMicroseconds(scaledTime * 4); // Keeps max bright @ prev value
        digitalWrite(ANODE_PINS[i], HIGH);
        digitalWrite(CATHODE_PINS[j], HIGH);
      }
    }
  }
}

void loop() {
  static byte ledBrightness[8][8]; // Changed from ledOn to ledBrightness

  byte x = 0;
  byte y = 0;
  byte brightness = 0;
  static char message[60];

  if (Serial.available()) {
    x = Serial.parseInt();
    y = Serial.parseInt();
    brightness = Serial.parseInt();

    // Check for input validity
    if (Serial.read() != '\n') {
      Serial.println("invalid input - check that line ending is set to \"Newline\"; input must be three numbers (x y brightness)");
      return;
    }
    if (x < 0 || x > 7 || y < 0 || y > 7) {
      sprintf(message, "x = %d, y = %d -- indices out of bounds", x, y);
      Serial.println(message);
      return;
    }
    if (brightness < 0 || brightness > 15) {
      sprintf(message, "brightness = %d -- must be between 0 and 15", brightness);
      Serial.println(message);
      return;
    }

    // Print to Serial Monitor to give feedback about input
    sprintf(message, "x = %d, y = %d, brightness = %d", x, y, brightness);
    Serial.println(message);

    // Set the LED brightness
    ledBrightness[x][y] = brightness;
  }

  // This function gets called every loop
  display(ledBrightness);
}