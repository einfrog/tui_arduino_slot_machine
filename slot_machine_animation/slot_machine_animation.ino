/*
 * Slot Machine with Game Logic
 * 
 * This code integrates the slot machine game logic with the animation system.
 * 
 * Features:
 * - 3 reels for gameplay (reels 0-2)
 * - Reel 3 available for static display/animations
 * - Full game logic: win detection, payout calculation, credit tracking
 * - Button-controlled spins
 * - Serial output for debugging and results
 * 
 * Game Rules:
 * - Fixed wager: 5 credits per spin
 * - Starting credits: 500
 * - 8 symbols per reel (0-7), spaceship is symbol 7
 * - Win combinations:
 *   * Three spaceships: 600x wager (JACKPOT!)
 *   * Three of a kind: 122x wager
 *   * Two spaceships: 50x wager
 *   * One spaceship: 3x wager
 *   * Two of a kind: 2x wager
 * 
 * Note: Uses 8 symbols (0-7) with direct mapping. Symbol 7 is the spaceship (special symbol).
 */

#include <LedControl.h>
#include "symbols.h"
#include "animations.h"
#include <LiquidCrystal_I2C.h>
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"

const int DIN_PIN = 12;
const int CS_PIN = 11;
const int CLK_PIN = 10;
const int BUTTON_PIN = 2;  // Change this to your button pin

const int SDA_PIN = analogRead(4);
const int SCL_PIN = analogRead(5);

const int DFPLAYER_RX = 8;
const int DFPLAYER_TX = 9;


LedControl display = LedControl(DIN_PIN, CLK_PIN, CS_PIN, 4);
LiquidCrystal_I2C lcd(0x27, 16, 2);

SoftwareSerial mySoftwareSerial(DFPLAYER_RX, DFPLAYER_TX);
DFRobotDFPlayerMini myDFPlayer;

// Audio state tracking
bool spinSoundPlaying = false;
bool payoutSoundPlayed = false;


// ===== SLOT MACHINE GAME CONSTANTS =====
#define NUMREELS                3       // Use 3 reels for the game (reel 3 is for static display)
#define NUM_SYMBOLS             25      // Total symbols in the reel (0-7)
#define SHIP_SYMBOL_INDEX       7       // Spaceship is symbol index 7 (last symbol)

// Payout multipliers (based on wager)
#define THREE_SPACESHIP_PAYOUT  600
#define THREE_SYMBOL_PAYOUT     122
#define TWO_SPACESHIP_PAYOUT    50
#define ONE_SPACESHIP_PAYOUT    3
#define TWO_SYMBOL_PAYOUT       2

#define WAGER_AMOUNT            5       // Fixed wager per spin
#define STARTING_CREDITS        500     // Starting credit balance

// ===== GAME STATE =====
long creditBalance = STARTING_CREDITS;
long wagered = WAGER_AMOUNT;
int storedHold = 0;  // House hold percentage (0 = no house edge)

// Per-spin win tracking (reset each spin)
int reelMatches = 0;
int shipMatches = 0;
int twoMatchCount = 0;
int threeMatchCount = 0;
int shipOneMatchCount = 0;
int shipTwoMatchCount = 0;
int shipThreeMatchCount = 0;

// Button state tracking
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Reel state for each of the 4 displays
struct Reel {
  int currentSymbol;      // Which symbol is showing (0-7 for game reels)
  int scrollOffset;       // Current scroll position (0-7)
  bool isSpinning;        // Is this reel currently spinning?
  int targetSymbol;       // Symbol to land on
  int spinSpeed;          // Delay between frames (lower = faster)
  int minSpins;           // Minimum number of full rotations before stopping
  int spinsCompleted;     // Counter for rotations completed
};

Reel reels[3];
bool gameInProgress = false;
bool waitingForWinCheck = false;

void setup() {
  Serial.begin(9600);

  // Initialize DFPlayer
  mySoftwareSerial.begin(9600);
  Serial.println(F("Initializing DFPlayer..."));
  
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println(F("DFPlayer initialization failed!"));
    Serial.println(F("1. Check the connection!"));
    Serial.println(F("2. Insert the SD card with 001.mp3 and 002.mp3!"));
  } else {
    Serial.println(F("DFPlayer Mini online."));
    myDFPlayer.volume(25);  // Set volume 0-30
  }

  lcd.init();
  lcd.clear();         
  lcd.backlight();   

  // lcd.setCursor(2,0);   //Set cursor to character 2 on line 0
  lcd.print("Hello world!");
  // Seed random number generator
  randomSeed(analogRead(0) + millis());
  
  // Setup button with internal pull-up resistor
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize all displays (4 matrices total)
  for (int device = 0; device < 4; device++) {
    display.shutdown(device, false);
    display.setIntensity(device, 1);
    display.clearDisplay(device);
  }
  
  // Initialize reel states for game reels (0-2)
  for (int device = 0; device < 3; device++) {
    reels[device].currentSymbol = 0;
    reels[device].scrollOffset = 0;
    reels[device].isSpinning = false;
    reels[device].targetSymbol = 0;
    reels[device].spinSpeed = 50;
    reels[device].minSpins = 3;  // Minimum 3 full rotations
    reels[device].spinsCompleted = 0;
  }
  
  // Display initial symbol on game reels (0-2)
  for (int device = 0; device < 3; device++) {
    showSymbol(device, SYMBOLS[7]);
  }
  
  // Clear the 4th matrix (device 3) - can be used for static display/animations
  display.clearDisplay(3);
  
  Serial.println("========================================");
  Serial.println("    SLOT MACHINE - GAME READY");
  Serial.println("========================================");
  Serial.print("Starting Credits: ");
  Serial.println(creditBalance);
  Serial.print("Wager per spin: ");
  Serial.println(wagered);
  // lcd.print(wagered);
  Serial.println("Press button to spin!");
  Serial.println("========================================");
}

// Display a single symbol on a specific device
void showSymbol(int device, const uint8_t symbol[25]) {
  for (int row = 0; row < 25; row++) {
    display.setRow(device, row, symbol[row]);
  }
}

// Get symbol data for a given symbol index (0-7)
const uint8_t* getSymbolData(int symbolIndex) {
  // Direct mapping to 8 symbols
  int mappedIndex = symbolIndex % NUM_SYMBOLS;  // Map to 8 symbols (0-7)
  return SYMBOLS[mappedIndex];
}

// Update one reel's animation
void updateReel(int reelIndex) {
  Reel &reel = reels[reelIndex];
  
  if (!reel.isSpinning) {
    // Just show the current symbol
    showSymbol(reelIndex, getSymbolData(reel.currentSymbol));
    return;
  }
  
  // Create scrolling animation
  uint8_t frame[25];
  int nextSymbol = (reel.currentSymbol + 1) % NUM_SYMBOLS;  // Modulo 8 for 8 symbols
  
  // Build frame row by row
  for (int row = 0; row < 25; row++) {
    if (row < (25 - reel.scrollOffset)) {
      // Still showing current symbol
      int srcRow = row + reel.scrollOffset;
      const uint8_t* currentSym = getSymbolData(reel.currentSymbol);
      frame[row] = currentSym[srcRow];
    } else {
      // Next symbol coming into view from top
      int srcRow = row - (25 - reel.scrollOffset);
      const uint8_t* nextSym = getSymbolData(nextSymbol);
      frame[row] = nextSym[srcRow];
    }
  }
  
  showSymbol(reelIndex, frame);
  
  // Advance scroll offset
  reel.scrollOffset++;
  
  // When we've scrolled a full symbol (8 pixels)
  if (reel.scrollOffset >= 8) {
    reel.scrollOffset = 0;
    reel.currentSymbol = nextSymbol;
    reel.spinsCompleted++;
    
    // Check if we've completed minimum spins and reached target
    if (reel.spinsCompleted >= reel.minSpins && reel.currentSymbol == reel.targetSymbol) {
      reel.isSpinning = false;
      reel.scrollOffset = 0; // Ensure clean alignment
      showSymbol(reelIndex, getSymbolData(reel.currentSymbol)); // Display final symbol cleanly
      Serial.print("Reel ");
      Serial.print(reelIndex);
      Serial.print(" stopped on symbol ");
      Serial.println(reel.currentSymbol);
    }
  }
}

// Start spinning a specific reel to land on targetSymbol
void spinReel(int reelIndex, int targetSymbol, int speed) {
  reels[reelIndex].isSpinning = true;
  reels[reelIndex].targetSymbol = targetSymbol;
  reels[reelIndex].spinSpeed = speed;
  reels[reelIndex].spinsCompleted = 0;
  reels[reelIndex].minSpins = 3 + random(3);  // 3-5 full rotations
  Serial.print("Reel ");
  Serial.print(reelIndex);
  Serial.print(" spinning to symbol ");
  Serial.println(targetSymbol);
}

// Stop a reel immediately (utility function, not used in game flow)
void stopReel(int reelIndex) {
  reels[reelIndex].isSpinning = false;
  reels[reelIndex].scrollOffset = 0;
}

// Check if any game reel (0-2) is still spinning
bool isAnyReelSpinning() {
  for (int i = 0; i < NUMREELS; i++) {
    if (reels[i].isSpinning) {
      return true;
    }
  }
  return false;
}

// ===== GAME LOGIC FUNCTIONS =====

// Reset all per-spin variables
void zeroAllBalances() {
  reelMatches = 0;
  shipMatches = 0;
  twoMatchCount = 0;
  threeMatchCount = 0;
  shipOneMatchCount = 0;
  shipTwoMatchCount = 0;
  shipThreeMatchCount = 0;
}

// Check for winning combinations (works with 3 reels)
void checkForWin() {
  // Check for spaceships (symbol index 7)
  lcd.clear();
  for (int reelNum = 0; reelNum < NUMREELS; reelNum++) {
    if (reels[reelNum].currentSymbol == SHIP_SYMBOL_INDEX) {
      shipMatches++;
    }
  }
  
  // Check if other symbols matched
  for (int i = 0; i < NUMREELS; i++) {
    for (int j = 0; j < NUMREELS; j++) {
      if (reels[i].currentSymbol == reels[j].currentSymbol) {
        reelMatches++;
      }
    }
  }
  
  // Determine match counts
  if (reelMatches == 9) {  // 3 symbols match (3*3 = 9)
    reelMatches = 3;
    threeMatchCount++;
  } else if (reelMatches == 5) {  // 2 symbols match (2*2 + 1 = 5)
    reelMatches = 2;
    twoMatchCount++;
  } else if (reelMatches == 3) {
    reelMatches = 0;
  } else {
    reelMatches = -1;
  }
  
  // Count ship matches
  if (shipMatches == 3) {
    shipThreeMatchCount++;
  } else if (shipMatches == 2) {
    shipTwoMatchCount++;
  } else if (shipMatches == 1) {
    shipOneMatchCount++;
  }
  
  // Wins are mutually exclusive - clear lower wins if higher win exists
  if (shipThreeMatchCount) {
    threeMatchCount = 0;
    shipTwoMatchCount = 0;
    shipOneMatchCount = 0;
    twoMatchCount = 0;
    reelMatches = 0;
  } else if (threeMatchCount) {
    shipTwoMatchCount = 0;
    shipOneMatchCount = 0;
    twoMatchCount = 0;
    reelMatches = 0;
  } else if (shipTwoMatchCount) {
    shipOneMatchCount = 0;
    twoMatchCount = 0;
    reelMatches = 0;
  } else if (shipOneMatchCount) {
    twoMatchCount = 0;
    reelMatches = 0;
  } else if (twoMatchCount) {
    reelMatches = 0;
  }
}

// Calculate winnings based on matches
long calcWinnings() {
  double winnings = 0;
  
  if (shipThreeMatchCount > 0) {
    winnings = wagered * (THREE_SPACESHIP_PAYOUT - (THREE_SPACESHIP_PAYOUT * (storedHold / 100.0)));
  } else if (threeMatchCount > 0) {
    winnings = wagered * (THREE_SYMBOL_PAYOUT - (THREE_SYMBOL_PAYOUT * (storedHold / 100.0)));
  } else if (shipTwoMatchCount > 0) {
    winnings = wagered * (TWO_SPACESHIP_PAYOUT - (TWO_SPACESHIP_PAYOUT * (storedHold / 100.0)));
  } else if (shipOneMatchCount > 0) {
    winnings = wagered * (ONE_SPACESHIP_PAYOUT - (ONE_SPACESHIP_PAYOUT * (storedHold / 100.0)));
  } else if (twoMatchCount > 0) {
    winnings = wagered * (TWO_SYMBOL_PAYOUT - (TWO_SYMBOL_PAYOUT * (storedHold / 100.0)));
  } else {
    winnings = 0;
  }
  
  long roundWinnings = (long)round(winnings);
  // roundWinnings -= wagered;   // Subtract the wager (you pay whether you win or not)
  return roundWinnings;
}

// Display game results
void displayResults(long winnings) {
  // Stop spin sound and play payout sound
  if (spinSoundPlaying) {
    Serial.println("Playing payout sound...");
    myDFPlayer.play(1);
    spinSoundPlaying = false;
    payoutSoundPlayed = true;
  }

  Serial.println("\n=== RESULTS ===");
  Serial.print("Reels: [");
  for (int i = 0; i < NUMREELS; i++) {
    Serial.print(reels[i].currentSymbol);
    if (i < NUMREELS - 1) Serial.print(", ");
  }
  Serial.println("]");
  
  String winType = "No Win";
  if (shipThreeMatchCount > 0) {
    winType = "*** JACKPOT! THREE SEVENS! ***";
  } else if (threeMatchCount > 0) {
    winType = "Three of a Kind!";
  } else if (shipTwoMatchCount > 0) {
    winType = "Two SEVENS!";
  } else if (shipOneMatchCount > 0) {
    winType = "One SEVEN!";
  } else if (twoMatchCount > 0) {
    winType = "Two of a Kind!";
  }
  
  Serial.print("Win Type: ");
  Serial.println(winType);
  lcd.print(winType);
  Serial.print("Wager: ");
  Serial.print(wagered);
  Serial.println(" credits");

  
  if (winnings > 0) {
    Serial.print("You WON: ");
    Serial.print(winnings);
    // lcd.setCursor(0, 1);
    // lcd.print(winnings);
    Serial.println(" credits!");
  } else if (winnings < 0) {
    Serial.print("You LOST: ");
    Serial.print(abs(winnings));
    Serial.println(" credits");
  } else {
    Serial.println("Break even!");
  }
  lcd.setCursor(0, 1);
  lcd.print(winnings);
  creditBalance += winnings;
  Serial.print("New Balance: ");
  Serial.print(creditBalance);
  lcd.setCursor(8,1);
  lcd.print(creditBalance);
  Serial.println(" credits");
  Serial.println("==================\n");
}

// Start a new game spin
void startSpin() {
  if (creditBalance < wagered) {
    Serial.println("Not enough credits!");
    return;
  }
  
  if (isAnyReelSpinning()) {
    return;  // Already spinning
  }
  
  Serial.println("\n>>> SPINNING! <<<");
  creditBalance -= wagered;  // Deduct wager
  zeroAllBalances();
  gameInProgress = true;
  waitingForWinCheck = false;
  
  // Generate random target symbols for the 3 game reels
  int targets[NUMREELS];
  for (int i = 0; i < NUMREELS; i++) {
    targets[i] = random(NUM_SYMBOLS);  // Random symbol 0-7
  }
  
  // Start spinning with staggered timing
  for (int i = 0; i < NUMREELS; i++) {
    spinReel(i, targets[i], 30 + (i * 10));  // Faster spin, staggered stops
  }

  // Play spinning sound (track 001)
  Serial.println("Playing spin sound...");
  myDFPlayer.play(2);
  spinSoundPlaying = true;
  payoutSoundPlayed = false;
}

unsigned long lastUpdate = 0;

void loop() {
  unsigned long currentTime = millis();
  
  // Button check with debouncing
  static bool lastCheckState = HIGH;
  static unsigned long lastButtonPress = 0;
  bool currentButtonState = digitalRead(BUTTON_PIN);
  
  // Detect button press (transition from HIGH to LOW) with debounce
  if (currentButtonState == LOW && lastCheckState == HIGH && 
      !isAnyReelSpinning() && (currentTime - lastButtonPress > 500)) {
    lastButtonPress = currentTime;
    startSpin();
  }
  
  lastCheckState = currentButtonState;
  
  // Check if all reels have stopped and we need to evaluate win
  if (gameInProgress && !isAnyReelSpinning() && !waitingForWinCheck) {
    waitingForWinCheck = true;
    gameInProgress = false;
    
    // Small delay to show final positions
    delay(500);
    
    // Check for wins and calculate payout
    checkForWin();
    long winnings = calcWinnings();
    displayResults(winnings);
  }
  
  // Find the fastest spinning reel to set update rate
  int minDelay = 1000;
  bool anySpinning = false;
  for (int i = 0; i < NUMREELS; i++) {
    if (reels[i].isSpinning) {
      anySpinning = true;
      if (reels[i].spinSpeed < minDelay) {
        minDelay = reels[i].spinSpeed;
      }
    }
  }
  
  // Update game reels (0-2) if spinning
  if (anySpinning && (currentTime - lastUpdate >= minDelay)) {
    lastUpdate = currentTime;
    
    for (int i = 0; i < NUMREELS; i++) {
      updateReel(i);
    }
  }
  
  // Reel 3 can be used for static display or animations
  // (not part of the game logic)
}