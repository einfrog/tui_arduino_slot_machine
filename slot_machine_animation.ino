#include <LedControl.h>
#include "symbols.h"
#include "animations.h"

const int DIN_PIN = 12;
const int CS_PIN = 11;
const int CLK_PIN = 10;

LedControl display = LedControl(DIN_PIN, CLK_PIN, CS_PIN, 4);

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
  
  // Initialize all displays
  for (int device = 0; device < 4; device++) {
    display.shutdown(device, false);
    display.setIntensity(device, 8);
    display.clearDisplay(device);
    
    // Initialize reel states
    reels[device].currentSymbol = 0;
    reels[device].scrollOffset = 0;
    reels[device].isSpinning = false;
    reels[device].targetSymbol = 0;
    reels[device].spinSpeed = 500;
  }
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
  int nextSymbol = (reel.currentSymbol + 1) % 5;
  
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

unsigned long lastUpdate = 0;

void loop() {
  unsigned long currentTime = millis();
  
  // Find the fastest spinning reel to set update rate
  int minDelay = 1000;
  for (int i = 0; i < 4; i++) {
    if (reels[i].isSpinning && reels[i].spinSpeed < minDelay) {
      minDelay = reels[i].spinSpeed;
    }
  }
  
  // Update all reels at appropriate intervals
  if (currentTime - lastUpdate >= minDelay) {
    lastUpdate = currentTime;
    
    for (int i = 0; i < 4; i++) {
      updateReel(i);
    }
  }
  
  // Example: Start a spin when all reels are stopped
  static bool hasSpun = false;
  if (!hasSpun) {
    delay(1000); // Wait a second
    int targets[4] = {2, 1, 3, 2}; // Target symbols for each reel
    spinAllReels(targets);
    hasSpun = true;
  }
  
  // Check if all reels stopped, then trigger new spin after delay
  static unsigned long stopTime = 0;
  bool allStopped = true;
  for (int i = 0; i < 4; i++) {
    if (reels[i].isSpinning) {
      allStopped = false;
      break;
    }
  }
  
  if (allStopped && hasSpun) {
    if (stopTime == 0) {
      stopTime = currentTime;
    } else if (currentTime - stopTime > 3000) {
      // Spin again after 3 seconds
      int newTargets[4] = {random(5), random(5), random(5), random(5)};
      spinAllReels(newTargets);
      stopTime = 0;
    }
  } else {
    stopTime = 0;
  }
}