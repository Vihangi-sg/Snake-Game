

#include <EEPROM.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h> // TFT display library
#include "pitches.h"

// Define pins for the TFT display
#define TFT_CS     10
#define TFT_RST    8
#define TFT_DC     9
#define TFT_MOSI   11
#define TFT_SCK    13
#define TFT_MISO   12
#define TFT_LED    3
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// Define pins for the joystick
#define joystickXPin A1
#define joystickYPin A0
#define joystickSwPin 2

#define SPEAKER_PIN 7

// EEPROM address to store high score
const int EEPROM_ADDRESS = 0;

// Variables for joystick input
int xValue, yValue;
int xMap, yMap;

// Snake variables
int snakeSize = 10;     // Snake block size
int snakeLength = 1;    // Snake initial length
int snakeDirection = 0; // 0 = up, 1 = down, 2 = left, 3 = right
int snakeMaxLength = 100; // Max snake length

// Snake body positions (x, y)
int snakeX[100] = {120};  // Initial X positions of the snake
int snakeY[100] = {170};  // Initial Y positions of the snake

// Food variables
int foodX, foodY;       // Food coordinates
bool foodAvailable = false;     // Track if food is available
unsigned long foodTimer = 0;    // Timer for food appearance
const unsigned long foodDuration = 5000; // Food lasts for 5 seconds (5000 ms)
int foodCountdown = 5;          // Countdown timer for disappearing food (5 seconds)

// Red food variables
int redFoodX[10], redFoodY[10];  // Arrays to store positions for multiple red foods
bool redFoodAvailable[10] = {false};  // Track if each red food is available
int redFoodCount = 1;  // Start with 1 red food, increase by 1 for each new level

// Barrier position (digit 9)
int barrierX, barrierY; // coordinates of the barrier
int margin = 50;    // Minimum distance between food and the barrier
int barrierAppeared = false;


bool gameOver = false;  // Game over state

// Score and level variables
int score = 0;   // Initialize score
int level = 1;   // Initialize level at 1
int baseSpeed = 250; //Base speed of snake
int speed = baseSpeed; // Speed that gets adjusted based on levels
int currentLevel = 1;
int highScore = 0;  // Variable to store high score

// Menu variables
bool inMenu = true; // Flag to check if we are in the menu
int menuOption = 0; // 0 = Start Game, 1 = View High Score

// Reserved display area height (for score and level display)
const int reservedAreaHeight = 20;  // Reserve the top 20 pixels

unsigned long testFastLines(uint16_t color1, uint16_t color2) {
  unsigned long start;
  int x, y, w = tft.width(), h = tft.height();

  tft.fillScreen(ILI9341_BLACK);
  start = micros();
  for(y=0; y<h; y+=5) tft.drawFastHLine(0, y, w, color1);
  for(x=0; x<w; x+=5) tft.drawFastVLine(x, 0, h, color2);

  return micros() - start;
}

void playLevelUpSound() {
  tone(SPEAKER_PIN, NOTE_E4);
  delay(25);
  tone(SPEAKER_PIN, NOTE_G4);
  delay(25);
  tone(SPEAKER_PIN, NOTE_E5);
  delay(25);
  tone(SPEAKER_PIN, NOTE_C5);
  delay(25);
  tone(SPEAKER_PIN, NOTE_D5);
  delay(25);
  tone(SPEAKER_PIN, NOTE_G5);
  delay(25);
  noTone(SPEAKER_PIN);
}

void playGameOverSound() {
  // Play a Wah-Wah-Wah-Wah sound
  tone(SPEAKER_PIN, NOTE_DS5);
  delay(300);
  tone(SPEAKER_PIN, NOTE_D5);
  delay(300);
  tone(SPEAKER_PIN, NOTE_CS5);
  delay(300);
  for (byte i = 0; i < 10; i++) {
    for (int pitch = -10; pitch <= 10; pitch++) {
      tone(SPEAKER_PIN, NOTE_C5 + pitch);
      delay(5);
    }
  }
  noTone(SPEAKER_PIN);
  delay(500);
}

// Function to display a "Game Over" message
void displayGameOver() {
  playGameOverSound();
  Serial.println(testFastLines(ILI9341_RED, ILI9341_BLUE));
  delay(150);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_RED);
  tft.setTextSize(3);
  tft.setCursor(30, 130);
  tft.println("GAME OVER!");
  tft.setTextSize(2);
  tft.setCursor(40, 170);
  tft.setTextColor(ILI9341_WHITE);
  tft.print("Your Score:");
  tft.print(score);
  tft.setTextSize(2);
  tft.setCursor(45, 210);
  tft.setTextColor(ILI9341_RED);
  tft.println("Press Reset");
}

// Function to display the score on the screen
void displayScore() {
  // Clear previous score
  tft.fillRect(0, 0, 110, reservedAreaHeight, ILI9341_BLACK); // Clear top-left corner area

  // Set text size and color for the score
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);

  // Display score at the top-left corner
  tft.setCursor(10, 0);
  tft.print("Score:");
  tft.print(score);
}

// Function to display the level on the screen
void displayLevel() {
  // Clear previous level
  tft.fillRect(120, 0, 100, reservedAreaHeight, ILI9341_BLACK); // Clear space for level display

  // Set text size and color for the level
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);

  // Display level next to the score
  tft.setCursor(120, 0);
  tft.print("L:");
  tft.print(level);
}

// Function to move the snake
void moveSnake() {
  // Erase the last segment of the snake's tail
  tft.fillRect(snakeX[snakeLength-1], snakeY[snakeLength-1], snakeSize, snakeSize, ILI9341_BLACK);

  // Shift the snake's body positions forward
  for (int i = snakeLength - 1; i > 0; i--) {
    snakeX[i] = snakeX[i - 1];
    snakeY[i] = snakeY[i - 1];
  }

  // Update snake head's position based on the current direction
  switch (snakeDirection) {
    case 0: snakeY[0] += snakeSize; break;  // Move up
    case 1: snakeY[0] -= snakeSize; break;  // Move down
    case 2: snakeX[0] += snakeSize; break;  // Move left
    case 3: snakeX[0] -= snakeSize; break;  // Move right
  }

  // Handle edge wrapping (snake reappears from opposite side)
  if (snakeX[0] < 0) snakeX[0] = tft.width() - snakeSize;
  if (snakeX[0] >= tft.width()) snakeX[0] = 0;
  if (snakeY[0] < reservedAreaHeight) snakeY[0] = tft.height() - snakeSize;  // Respect reserved area
  if (snakeY[0] >= tft.height()) snakeY[0] = reservedAreaHeight;  // Respect reserved area

  // Draw the snake's new head
  tft.fillRect(snakeX[0], snakeY[0], snakeSize, snakeSize, ILI9341_GREEN);
}

// Function to update the snake's direction based on joystick input
void updateJoystickInput() {
  // Read joystick X and Y axis values
  xValue = analogRead(joystickXPin);  // Horizontal movement
  yValue = analogRead(joystickYPin);  // Vertical movement
  
  // Map joystick values to a simpler range (-1, 0, 1)
  xMap = map(xValue, 0, 1023, -1, 1);  // Map X-axis values
  yMap = map(yValue, 0, 1023, -1, 1);  // Map Y-axis values

  // Update snake direction
  if (xMap == 1 && snakeDirection != 2) {
    snakeDirection = 3;  // Move right
  } else if (xMap == -1 && snakeDirection != 3) {
    snakeDirection = 2;  // Move left
  } else if (yMap == 1 && snakeDirection != 0) {
    snakeDirection = 1;  // Move down
  } else if (yMap == -1 && snakeDirection != 1) {
    snakeDirection = 0;  // Move up
  }
}

// Function to place food randomly on the screen
void placeFood() {
  // Generate random coordinates for food within the display boundary
  do{
  foodX = random(0, tft.width() / snakeSize) * snakeSize;
  foodY = random(reservedAreaHeight / snakeSize, tft.height() / snakeSize) * snakeSize;
  }while ((foodX > barrierX - margin && foodX < barrierX + 80 + margin) && 
           (foodY > barrierY - margin && foodY < barrierY + 100 + margin));
  // Draw the food as a yellow block
  tft.fillRect(foodX, foodY, snakeSize, snakeSize, ILI9341_YELLOW);

  foodAvailable = true;
  foodTimer = millis();   // Start the timer for food
  foodCountdown = 5;      // Start countdown at 5 seconds
}

// Function to clear the food from the screen
void clearFood() {
  tft.fillRect(foodX, foodY, snakeSize, snakeSize, ILI9341_BLACK);
  foodAvailable = false;
}

// Function to handle food appearance, countdown, and disappearance
void handleFood() {
  if (foodAvailable && level >= 3) {
    unsigned long elapsed = millis() - foodTimer;

    // Update the countdown timer every second
    if (elapsed >= 1000 * (5 - foodCountdown) && foodCountdown > 0) {
      foodCountdown--;
      tft.fillRect(180, 0, 60, 20, ILI9341_BLACK); // Clear previous countdown display
      tft.setCursor(180, 0);
      tft.setTextColor(ILI9341_MAGENTA);
      tft.setTextSize(2);
      tft.print("T:");
      tft.print(foodCountdown);
    }

    // If food has been available for 5 seconds, make it disappear
    if (elapsed >= foodDuration) {
      clearFood();
      placeFood();  // Respawn food at a new position after it disappears
    }
  }
}

// Function to check if the snake eats the food
void checkFoodCollision() {
  // Check if the snake's head is on the normal food
  if (snakeX[0] == foodX && snakeY[0] == foodY) {
    // Increase the score and snake length
    score++;
    snakeLength++;
    displayScore();

    // Increase the level every 2 points
    if (score % 2 == 0) {
      level++;
      displayLevel();
      playLevelUpSound();
    }

    // Place new food after eating
    foodAvailable = false;
    placeFood();
  }

  // Check if the snake's head is on any red food
  for (int i = 0; i < redFoodCount; i++) {
    if (redFoodAvailable[i] && snakeX[0] == redFoodX[i] && snakeY[0] == redFoodY[i]) {
      // Decrease the score
      if (score>0){
        score--;
      } else {
        score = 0;
      }
      redFoodAvailable[i] = false;
      tone(SPEAKER_PIN, NOTE_A5);
      delay(150);
      noTone(SPEAKER_PIN);
      displayScore();
    }
  }
}

// Function to check if the snake collides with its own body
void checkSelfCollision() {
  // Check if the snake's head collides with any of its body parts
  for (int i = 1; i < snakeLength; i++) {
    if (snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) {
      gameOver = true;  // Game over if the snake's head collides with its body
    }
  }
}

// Function to place red food randomly on the screen
void placeRedFood(int foodIndex) {
  // Generate random coordinates for red food within the display boundary
  do{
  redFoodX[foodIndex] = random(0, tft.width() / snakeSize) * snakeSize;
  redFoodY[foodIndex] = random(reservedAreaHeight / snakeSize, tft.height() / snakeSize) * snakeSize;
  }while ((redFoodX[foodIndex] > barrierX - margin && redFoodX[foodIndex] < barrierX + 80 + margin) && 
           (redFoodY[foodIndex] > barrierY - margin && redFoodY[foodIndex] < barrierY + 100 + margin));
  // Draw the red food
  tft.fillRect(redFoodX[foodIndex], redFoodY[foodIndex], snakeSize, snakeSize, ILI9341_RED);

  redFoodAvailable[foodIndex] = true;
}

// Function to place barrier randomly on the screen (excluding reserved area)
void placeBarrier() {
  // Generate random coordinates for food within the display boundary

  barrierX = 80;
  barrierY = 100;
  
  tft.fillRect(barrierX, barrierY, 80, 100, ILI9341_CYAN);
  tft.fillRect(barrierX+10, barrierY+10, 60, 40, ILI9341_BLACK);
  tft.fillRect(barrierX, barrierY+60, 70, 40, ILI9341_BLACK);
}

// Function to draw the barrier (digit 9)
void drawBarrier() {
  if (!barrierAppeared  && level >= 2){
    placeBarrier();
    barrierAppeared = true;
  }
}

// Function to check if the snake collides with the barrier
void checkBarrierCollision() {
  
  // Check if the snake's head overlaps with the barrier
  if (barrierAppeared && (snakeX[0] >= (barrierX) && snakeX[0] < (barrierX + 80) &&
      snakeY[0] >= (barrierY) && snakeY[0] < (barrierY + 60)) | (snakeX[0] >= (barrierX+70) && snakeX[0] < (barrierX + 80) &&
      snakeY[0] >= (barrierY+60) && snakeY[0] < (barrierY+100))) {
    tone(SPEAKER_PIN, NOTE_G5);
    delay(150);
    noTone(SPEAKER_PIN);
    gameOver = true;  // Trigger game over if snake collides with the barrier
  }
}

// Function to handle red food appearance and disappearance
void handleRedFood() {
   // Start handling red food from Level 4
  if (level >= 4) {
    // Determine the red food count based on the current level
    if (level == 4) {
      redFoodCount = 1;  // 1 red food for Level 4
    } else {
      redFoodCount = level - 3;  // 2 red foods at Level 5, 3 red foods at Level 6, etc.
    }

    // Ensure the correct number of red food is always on screen
    for (int i = 0; i < redFoodCount; i++) {
      if (!redFoodAvailable[i]) {  // Check if red food has been eaten or not placed
        placeRedFood(i);  // Place the red food at a new random position
      }
    }
  }
}

// Function to display the menu
void showMenu() {
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);

  tft.setCursor(20, 70);
  if (menuOption == 0) tft.setTextColor(ILI9341_GREEN);
  tft.print("1. Start Game");

  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(20, 150);
  if (menuOption == 1) tft.setTextColor(ILI9341_GREEN);
  tft.print("2. View High Score");

  tft.setTextColor(ILI9341_WHITE);
}

// Function to reset the game
void resetGame() {
  snakeLength = 1;
  snakeX[0] = 120;
  snakeY[0] = 160;
  snakeDirection = 0;
  score = 0;
  level = 1;
  speed = baseSpeed;
  gameOver = false;

  // Draw the initial snake block
  tft.fillRect(snakeX[0], snakeY[0], snakeSize, snakeSize, ILI9341_GREEN);

  // Place the first food item
  placeFood();
  displayScore();
  displayLevel();
}

// Function to display the high score
void displayHighScore() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_CYAN);
  tft.setTextSize(3);
  tft.setCursor(30, 70);
  tft.print("High Score:");
  tft.setCursor(30, 140);
  tft.print(highScore);

  delay(2000); // Pause for 2 seconds before returning to the menu
  tft.fillScreen(ILI9341_BLACK);
  showMenu();
}

// Function to start a new game
void startNewGame() {
  inMenu = false;
  tft.fillScreen(ILI9341_BLACK);
  resetGame();
}

// Function to handle joystick input in the menu
void handleMenuInput() {
  // Read joystick input
  xValue = analogRead(joystickXPin);
  yValue = analogRead(joystickYPin);
  yMap = map(yValue, 0, 1023, -1, 1);

  // Move up or down in the menu
  if (yMap == -1 && menuOption == 1) {
    menuOption = 0;
    tft.fillRect(10, 135, 240, 30, ILI9341_BLACK);
    showMenu();
  } else if (yMap == 1 && menuOption == 0) {
    menuOption = 1;
    tft.fillRect(10, 55, 240, 30, ILI9341_BLACK);
    showMenu();
  }

  // Select menu option
  if (digitalRead(joystickSwPin) == LOW) {
    if (menuOption == 0) {
      startNewGame();
    } else if (menuOption == 1) {
      displayHighScore();
    }
  }
}

// Function to save the high score in EEPROM
void saveHighScore() {
  if (score > highScore) {
    highScore = score;
    EEPROM.write(EEPROM_ADDRESS, highScore);
  }
}

void setup() {
  
  // Initialize the TFT display
  tft.begin();
  tft.fillScreen(ILI9341_BLACK);

  // Initialize joystick pins
  pinMode(joystickSwPin, INPUT_PULLUP);

  // Load high score from EEPROM
  highScore = EEPROM.read(EEPROM_ADDRESS);
  if (highScore == 255) {
    highScore = 0;  // Initialize if EEPROM is blank
  }
  randomSeed(analogRead(A5));
  // Show the menu at the beginning
  showMenu();
}

void loop() {
  if (inMenu) {
    handleMenuInput();
  } else {
    if (!gameOver) {
      updateJoystickInput();
      moveSnake();
      drawBarrier();      
      checkFoodCollision();
      checkSelfCollision();
      checkBarrierCollision();
      handleFood();
      handleRedFood();
      if (level >= 5 && currentLevel != level) {
        currentLevel = level;
        speed = baseSpeed * pow(0.8,(level - 4));  // Decrease speed by 30 ms per level after level 5
      }
      delay(speed);
    } else {
        saveHighScore();
        displayGameOver();
    }
  }
}

