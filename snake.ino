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
const unsigned long APPLE_BLINK_INTERVAL = 250; // 250ms blink on/off
const unsigned long EXPLOSION_FRAME_INTERVAL = 250; // 250ms per explosion frame
unsigned long lastMoveTime = 0;
unsigned long lastAppleBlinkTime = 0;
unsigned long explosionStartTime = 0;

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
enum GameState { WAITING, PLAYING, EXPLODING };
GameState gameState = WAITING;

// BITMAPS for start screen - currently says SNAKE

const byte LETTER_S[5][4] = {
  {1,1,1,1},
  {1,0,0,0},
  {1,1,1,1},
  {0,0,0,1},
  {1,1,1,1}
};

const byte LETTER_N[5][4] = {
  {1,0,0,1},
  {1,1,0,1},
  {1,0,1,1},
  {1,0,0,1},
  {1,0,0,1}
};

const byte LETTER_A[5][4] = {
  {0,1,1,0},
  {1,0,0,1},
  {1,1,1,1},
  {1,0,0,1},
  {1,0,0,1}
};

const byte LETTER_K[5][4] = {
  {1,0,0,1},
  {1,0,1,0},
  {1,1,0,0},
  {1,0,1,0},
  {1,0,0,1}
};

const byte LETTER_E[5][4] = {
  {1,1,1,1},
  {1,0,0,0},
  {1,1,0,0},
  {1,0,0,0},
  {1,1,1,1}
};

// The word is: S N A K E
const byte *LETTERS[5] = {
  (const byte*)LETTER_S,
  (const byte*)LETTER_N,
  (const byte*)LETTER_A,
  (const byte*)LETTER_K,
  (const byte*)LETTER_E
};

const int LETTER_COUNT = 5;
const int LETTER_HEIGHT = 5;
const int LETTER_WIDTH = 4;

// spacing: 1 column between letters, 3 columns after full word
const int INTER_LETTER_SPACE = 1;
const int END_SPACE = 3;

// Compute total scroll width
int computeScrollWidth() {
  int total = 0;
  for (int i = 0; i < LETTER_COUNT; i++)
    total += LETTER_WIDTH + INTER_LETTER_SPACE;
  return total + END_SPACE;
}

const int SCROLL_WIDTH = computeScrollWidth();

// Full scroll buffer: 5 rows, SCROLL_WIDTH columns
// Holds entire pattern and spacing
byte scrollBuffer[5][200]; // 200 > max possible width safety

// Build scroll buffer once
void buildScrollBuffer() {
  int col = 0;
  for (int i = 0; i < LETTER_COUNT; i++) {
    int w = LETTER_WIDTH;
    const byte (*letter)[4] = (const byte(*)[4])LETTERS[i];

    // Copy each letter
    for (int x = 0; x < w; x++) {
      for (int y = 0; y < 5; y++) {
        scrollBuffer[y][col + x] = letter[y][x];
      }
    }
    col += w;

    // Add inter-letter space
    for (int s = 0; s < INTER_LETTER_SPACE; s++) {
      for (int y = 0; y < 5; y++) scrollBuffer[y][col] = 0;
      col++;
    }
  }

  // Add trailing END_SPACE blank columns
  for (int s = 0; s < END_SPACE; s++) {
    for (int y = 0; y < 5; y++) scrollBuffer[y][col] = 0;
    col++;
  }
}

// Scroll position
int scrollOffset = 0;
unsigned long lastScrollTime = 0;
const unsigned long SCROLL_INTERVAL = 120; // ms per shift

// Draw the start screen into your ledOn buffer
void drawStartScreen(byte ledOn[8][8]) {
  unsigned long now = millis();
  if (now - lastScrollTime >= SCROLL_INTERVAL) {
    lastScrollTime = now;
    scrollOffset++;
    if (scrollOffset >= SCROLL_WIDTH) scrollOffset = 0;
  }

  // Clear
  for (int r=0;r<8;r++) for (int c=0;c<8;c++) ledOn[r][c] = 0;

  // Map scrollBuffer rows 0..4 onto matrix rows 1..5
  for (int y = 0; y < 5; y++) {
    int ledRow = y + 1;

    for (int x = 0; x < 8; x++) {
      int srcCol = (scrollOffset + x) % SCROLL_WIDTH;
      ledOn[ledRow][x] = scrollBuffer[y][srcCol];
    }
  }
}

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(100);
  
  pinMode(VRx, INPUT);
  pinMode(VRy, INPUT);
  Serial.println("Joystick Initialized");
  Serial.println("Move joystick to start game!");
  
  randomSeed(analogRead(A6)); // Seed random number generator with unconnected pin - true atmospheric noise a la CS109!
  generateApple(); // Generate first apple before game starts
  buildScrollBuffer();

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
  gameState = WAITING;
  generateApple(); // Generate first apple
  Serial.println("Game reset! Move joystick to start.");
}

void startExplosion() {
  gameState = EXPLODING;
  explosionStartTime = millis();
  Serial.println("BOOM!");
}

// Explosion animation frames - manually defined LED coordinates
// Frame 0: Center 2x2 square
const Point explosionFrame0[] = {{3,3}, {3,4}, {4,3}, {4,4}};
const int explosionFrame0Size = 4;

// Frame 1: Next ring out
const Point explosionFrame1[] = {
  {2,2}, {2,3}, {2,4}, {2,5},
  {3,2}, {3,5},
  {4,2}, {4,5},
  {5,2}, {5,3}, {5,4}, {5,5}
};
const int explosionFrame1Size = 12;

// Frame 2: Next ring out
const Point explosionFrame2[] = {
  {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6},
  {2,1}, {2,6},
  {3,1}, {3,6},
  {4,1}, {4,6},
  {5,1}, {5,6},
  {6,1}, {6,2}, {6,3}, {6,4}, {6,5}, {6,6}
};
const int explosionFrame2Size = 20;

// Frame 3: Outer border
const Point explosionFrame3[] = {
  {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, {0,6}, {0,7},
  {1,0}, {1,7},
  {2,0}, {2,7},
  {3,0}, {3,7},
  {4,0}, {4,7},
  {5,0}, {5,7},
  {6,0}, {6,7},
  {7,0}, {7,1}, {7,2}, {7,3}, {7,4}, {7,5}, {7,6}, {7,7}
};
const int explosionFrame3Size = 28;

void drawExplosionFrame(byte ledOn[8][8], int frameNum) {
  const Point* frameData;
  int frameSize;
  
  switch(frameNum) {
    case 0:
      frameData = explosionFrame0;
      frameSize = explosionFrame0Size;
      break;
    case 1:
      frameData = explosionFrame1;
      frameSize = explosionFrame1Size;
      break;
    case 2:
      frameData = explosionFrame2;
      frameSize = explosionFrame2Size;
      break;
    case 3:
      frameData = explosionFrame3;
      frameSize = explosionFrame3Size;
      break;
    default:
      return; // No LEDs for frame 4+ (blank)
  }
  
  // Light up all LEDs in this frame
  for (int i = 0; i < frameSize; i++) {
    ledOn[frameData[i].x][frameData[i].y] = 1;
  }
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
  
  // Handle explosion animation
  if (gameState == EXPLODING) {
    unsigned long elapsedTime = millis() - explosionStartTime;
    int frame = elapsedTime / EXPLOSION_FRAME_INTERVAL;
    
    // Clear display
    for (int i = 0; i < 8; i++) {
      for (int j = 0; j < 8; j++) {
        ledOn[i][j] = 0;
      }
    }
    
    if (frame < 4) {
      // Draw explosion frame for frames 0-3
      drawExplosionFrame(ledOn, frame);
    } else if (frame == 4) {
      // Frame 4: all LEDs off (already cleared)
    } else {
      // Animation complete, reset game
      resetGame();
    }
    
    display(ledOn);
    return; // Skip normal game logic during explosion
  }
  
  // Read joystick input
  byte newDirection = readDirection();

  // WAITING STATE → show scrolling start screen
  if (gameState == WAITING) {
    
    // If joystick moved → start game
    if (newDirection != 0) {
      gameState = PLAYING;
      direction = newDirection;
      lastDirection = newDirection;
      lastMoveTime = millis();
      lastAppleBlinkTime = millis();
      Serial.println("Game started!");
      return;
    }

    // Otherwise draw the scrolling start screen
    drawStartScreen(ledOn);
    display(ledOn);
    return;
  }
  
  // Handle apple blinking (only when game is playing)
  if (gameState == PLAYING && (millis() - lastAppleBlinkTime >= APPLE_BLINK_INTERVAL)) {
    lastAppleBlinkTime = millis();
    appleVisible = !appleVisible; // Toggle visibility
  }
  
  // Update direction if game is active and direction changed
  // Prevent opposite direction (can't go back on yourself)
  if (gameState == PLAYING && newDirection != 0 && newDirection != lastDirection) {
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
  if (gameState == PLAYING && (millis() - lastMoveTime >= MOVE_INTERVAL)) {
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
      startExplosion();
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
        startExplosion();
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
  
  // Draw apple (blinking if game is playing, solid if waiting to start)
  if (gameState == WAITING || appleVisible) {
    ledOn[appleX][appleY] = 1;
  }
  
  // Display the LED matrix
  display(ledOn);
}
