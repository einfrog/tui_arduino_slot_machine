#include <LedControl.h>
#include "symbols.h"
#include "animations.h"

const int DIN_PIN = 12;
const int CS_PIN = 11;
const int CLK_PIN = 10;
const int BUTTON_PIN = 2;  // Change this to your button pin

LedControl display = LedControl(DIN_PIN, CLK_PIN, CS_PIN, 4);

// Button state tracking
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Reel state for each of the 4 displays
struct Reel {
  int currentSymbol;      // Which symbol is showing (0-4)
  int scrollOffset;       // Current scroll position (0-7)
  bool isSpinning;        // Is this reel currently spinning?
  int targetSymbol;       // Symbol to land on
  int spinSpeed;          // Delay between frames (lower = faster)
};

Reel reels[4];

void setup() {
  Serial.begin(9600);
  
  // Setup button with internal pull-up resistor
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize all displays
  for (int device = 0; device < 4; device++) {
    display.shutdown(device, false);
    display.setIntensity(device, 8);
    display.clearDisplay(device);
    
    // Initialize reel states - all start showing symbol 0
    reels[device].currentSymbol = 0;
    reels[device].scrollOffset = 0;
    reels[device].isSpinning = false;
    reels[device].targetSymbol = 0;
    reels[device].spinSpeed = 50;
  }
  
  // Display initial symbol on all reels
  for (int device = 0; device < 4; device++) {
    showSymbol(device, SYMBOLS[0]);
  }
  
  Serial.println("Slot machine ready! Press button to spin.");
}

// Display a single symbol on a specific device
void showSymbol(int device, const uint8_t symbol[8]) {
  for (int row = 0; row < 8; row++) {
    display.setRow(device, row, symbol[row]);
  }
}

// Update one reel's animation
void updateReel(int reelIndex) {
  Reel &reel = reels[reelIndex];
  
  if (!reel.isSpinning) {
    // Just show the current symbol
    showSymbol(reelIndex, SYMBOLS[reel.currentSymbol]);
    return;
  }
  
  // Create scrolling animation
  uint8_t frame[8];
  int nextSymbol = (reel.currentSymbol + 1) % 5;  // Modulo 5 for 5 symbols
  
  // Build frame row by row
  for (int row = 0; row < 8; row++) {
    if (row < (8 - reel.scrollOffset)) {
      // Still showing current symbol
      int srcRow = row + reel.scrollOffset;
      frame[row] = SYMBOLS[reel.currentSymbol][srcRow];
    } else {
      // Next symbol coming into view from top
      int srcRow = row - (8 - reel.scrollOffset);
      frame[row] = SYMBOLS[nextSymbol][srcRow];
    }
  }
  
  showSymbol(reelIndex, frame);
  
  // Advance scroll offset
  reel.scrollOffset++;
  
  // When we've scrolled a full symbol (8 pixels)
  if (reel.scrollOffset >= 8) {
    reel.scrollOffset = 0;
    reel.currentSymbol = nextSymbol;
    
    // Check if we've reached target - stop at start of target symbol
    if (reel.currentSymbol == reel.targetSymbol) {
      reel.isSpinning = false;
      reel.scrollOffset = 0; // Ensure clean alignment
      showSymbol(reelIndex, SYMBOLS[reel.currentSymbol]); // Display final symbol cleanly
      Serial.print("Reel ");
      Serial.print(reelIndex);
      Serial.println(" stopped");
    }
  }
}

// Start spinning a specific reel to land on targetSymbol
void spinReel(int reelIndex, int targetSymbol, int speed) {
  reels[reelIndex].isSpinning = true;
  reels[reelIndex].targetSymbol = targetSymbol;
  reels[reelIndex].spinSpeed = speed;
  Serial.print("Reel ");
  Serial.print(reelIndex);
  Serial.print(" spinning to symbol ");
  Serial.println(targetSymbol);
}

// Stop a reel immediately
void stopReel(int reelIndex) {
  reels[reelIndex].isSpinning = false;
  reels[reelIndex].scrollOffset = 0;
}

// Start all reels spinning
void spinAllReels(int targets[4]) {
  for (int i = 0; i < 4; i++) {
    // Stagger the stop timing for cascade effect
    spinReel(i, targets[i], 50 + (i * 20));
  }
}

// Check if button is pressed (SIMPLE VERSION for testing)
bool isButtonPressed() {
  static bool wasPressed = false;
  bool isPressed = (digitalRead(BUTTON_PIN) == LOW);
  
  if (isPressed && !wasPressed) {
    wasPressed = true;
    return true;  // Button just pressed
  }
  
  if (!isPressed) {
    wasPressed = false;
  }
  
  return false;
}

// Check if any reel is still spinning
bool isAnyReelSpinning() {
  for (int i = 0; i < 4; i++) {
    if (reels[i].isSpinning) {
      return true;
    }
  }
  return false;
}

unsigned long lastUpdate = 0;

void loop() {
  unsigned long currentTime = millis();
  
  // Simple direct button check - no edge detection needed
  static bool lastCheckState = HIGH;
  bool currentButtonState = digitalRead(BUTTON_PIN);
  
  // Detect button press (transition from HIGH to LOW)
  if (currentButtonState == LOW && lastCheckState == HIGH && !isAnyReelSpinning()) {
    Serial.println(">>> BUTTON PRESSED! Starting spin...");
    // Generate random target symbols (or use your payout logic here)
    int newTargets[4] = {random(5), random(5), random(5), random(5)};
    spinAllReels(newTargets);
    delay(300); // Small delay to avoid double-trigger
  }
  
  lastCheckState = currentButtonState;
  
  // Find the fastest spinning reel to set update rate
  int minDelay = 1000;
  bool anySpinning = false;
  for (int i = 0; i < 4; i++) {
    if (reels[i].isSpinning) {
      anySpinning = true;
      if (reels[i].spinSpeed < minDelay) {
        minDelay = reels[i].spinSpeed;
      }
    }
  }
  
  // Only update reels if at least one is spinning
  if (anySpinning && (currentTime - lastUpdate >= minDelay)) {
    lastUpdate = currentTime;
    
    for (int i = 0; i < 4; i++) {
      updateReel(i);
    }
  }
}