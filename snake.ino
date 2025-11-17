/*
  A Little Snake Game on an 8x8 LED Matrix
  HW-504 Joystick for Input

  Weston Keller
  E40M Fall 2025
*/

// Joystick Pins
const int VRx = A5;
const int VRy = A4;

// LED Pins
const byte ANODE_PINS[8] = {13, 12, 11, 10, 9, 8, 7, 6};
const byte CATHODE_PINS[8] = {A3, A2, A1, A0, 5, 4, 3, 2};

// Joystick thresholds
const int JOY_THRESHOLD = 300; // Threshold for detecting joystick movement from center

// Game timing
const unsigned long MOVE_INTERVAL = 500; // 500ms = 2 moves per second
const unsigned long APPLE_BLINK_INTERVAL = 250; // 500ms blink on/off
unsigned long lastMoveTime = 0;
unsigned long lastAppleBlinkTime = 0;

// Snake state
struct Point {
  byte x;
  byte y;
};

Point snakeBody[64]; // Maximum possible snake length on 8x8 grid
byte snakeLength = 1; // Current length of snake

// Direction: 0=none, 1=up, 2=down, 3=left, 4=right
byte direction = 0;
byte lastDirection = 0; // To prevent rapid direction changes

// Apple state
byte appleX = 0;
byte appleY = 0;
bool appleVisible = true; // For blinking

// Game state
bool gameActive = false;

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(100);
  
  pinMode(VRx, INPUT);
  pinMode(VRy, INPUT);
  Serial.println("Joystick Initialized");
  Serial.println("Move joystick to start game!");
  
  randomSeed(analogRead(A6)); // Seed random number generator with unconnected pin
  generateApple(); // Generate first apple before game starts

  for (byte i = 0; i < 8; i++) {
    pinMode(ANODE_PINS[i], OUTPUT);
    pinMode(CATHODE_PINS[i], OUTPUT);
    digitalWrite(ANODE_PINS[i], HIGH);
    digitalWrite(CATHODE_PINS[i], HIGH);
  }
}

void display(byte pattern[8][8]) {
  for (int i = 0; i < 8; i++) {  
    for (int j = 0; j < 8; j++) {
      if (pattern[i][j]) {
        digitalWrite(CATHODE_PINS[j], LOW);
      }
      else {
        digitalWrite(CATHODE_PINS[j], HIGH);
      }
    }
    digitalWrite(ANODE_PINS[i], LOW);
    delayMicroseconds(1000);
    digitalWrite(ANODE_PINS[i], HIGH);
  }
}

byte readDirection() {
  int joyX = analogRead(VRx);
  int joyY = analogRead(VRy);
  
  // Check for UP (Y high)
  if (joyY > 512 + JOY_THRESHOLD) {
    return 1; // UP
  }
  // Check for DOWN (Y low)
  else if (joyY < 512 - JOY_THRESHOLD) {
    return 2; // DOWN
  }
  // Check for RIGHT (X low)
  else if (joyX < 512 - JOY_THRESHOLD) {
    return 4; // RIGHT
  }
  // Check for LEFT (X high)
  else if (joyX > 512 + JOY_THRESHOLD) {
    return 3; // LEFT
  }
  
  return 0; // No movement
}

void resetGame() {
  // Initialize snake at center
  snakeBody[0].x = 3;
  snakeBody[0].y = 3;
  snakeLength = 1;
  direction = 0;
  lastDirection = 0;
  gameActive = false;
  generateApple(); // Generate first apple
  Serial.println("Game reset! Move joystick to start.");
}

void generateApple() {
  // Generate random position that's not occupied by any part of the snake
  bool validPosition = false;
  
  while (!validPosition) {
    appleX = random(0, 8);
    appleY = random(0, 8);
    
    // Check if this position overlaps with any snake segment
    validPosition = true;
    for (int i = 0; i < snakeLength; i++) {
      if (appleX == snakeBody[i].x && appleY == snakeBody[i].y) {
        validPosition = false;
        break;
      }
    }
  }
  
  Serial.print("Apple generated at: (");
  Serial.print(appleX);
  Serial.print(", ");
  Serial.print(appleY);
  Serial.println(")");
}

void loop() {
  static byte ledOn[8][8] = {0};
  
  // Read joystick input
  byte newDirection = readDirection();
  
  // Start game if not active and joystick moved
  if (!gameActive && newDirection != 0) {
    gameActive = true;
    direction = newDirection;
    lastDirection = newDirection;
    lastMoveTime = millis();
    lastAppleBlinkTime = millis(); // Start apple blinking
    Serial.println("Game started!");
  }
  
  // Handle apple blinking (only when game is active)
  if (gameActive && (millis() - lastAppleBlinkTime >= APPLE_BLINK_INTERVAL)) {
    lastAppleBlinkTime = millis();
    appleVisible = !appleVisible; // Toggle visibility
  }
  
  // Update direction if game is active and direction changed
  // Prevent opposite direction (can't go back on yourself)
  if (gameActive && newDirection != 0 && newDirection != lastDirection) {
    // Check not going opposite direction
    if (!((direction == 1 && newDirection == 2) ||  // UP and DOWN
          (direction == 2 && newDirection == 1) ||
          (direction == 3 && newDirection == 4) ||  // LEFT and RIGHT
          (direction == 4 && newDirection == 3))) {
      direction = newDirection;
      lastDirection = newDirection;
    }
  }
  
  // Move snake at fixed intervals
  if (gameActive && (millis() - lastMoveTime >= MOVE_INTERVAL)) {
    lastMoveTime = millis();
    
    // Calculate next head position based on direction
    int nextX = snakeBody[0].x;
    int nextY = snakeBody[0].y;
    
    switch(direction) {
      case 1: // UP
        nextY++;
        break;
      case 2: // DOWN
        nextY--;
        break;
      case 3: // LEFT
        nextX--;
        break;
      case 4: // RIGHT
        nextX++;
        break;
    }
    
    // Check bounds
    if (nextX < 0 || nextX > 7 || nextY < 0 || nextY > 7) {
      Serial.println("Hit boundary!");
      resetGame();
    } else {
      // Check collision with self (tail)
      bool hitSelf = false;
      for (int i = 0; i < snakeLength; i++) {
        if (nextX == snakeBody[i].x && nextY == snakeBody[i].y) {
          hitSelf = true;
          break;
        }
      }
      
      if (hitSelf) {
        Serial.println("Hit yourself!");
        resetGame();
      } else {
        // Check if snake ate the apple
        bool ateApple = (nextX == appleX && nextY == appleY);
        
        if (ateApple) {
          Serial.println("Apple eaten!");
          snakeLength++; // Grow the snake
          generateApple(); // Generate new apple
        }
        
        // Move the snake body: shift all segments back
        // Start from tail and move towards head
        for (int i = snakeLength - 1; i > 0; i--) {
          snakeBody[i].x = snakeBody[i - 1].x;
          snakeBody[i].y = snakeBody[i - 1].y;
        }
        
        // Update head position
        snakeBody[0].x = nextX;
        snakeBody[0].y = nextY;
      }
    }
  }
  
  // Clear display
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      ledOn[i][j] = 0;
    }
  }
  
  // Draw entire snake body
  for (int i = 0; i < snakeLength; i++) {
    ledOn[snakeBody[i].x][snakeBody[i].y] = 1;
  }
  
  // Draw apple (blinking if game is active, solid if waiting to start)
  if (!gameActive || appleVisible) {
    ledOn[appleX][appleY] = 1;
  }
  
  // Display the LED matrix
  display(ledOn);
}
