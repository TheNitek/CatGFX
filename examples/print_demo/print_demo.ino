#include <Arduino.h>
#include <CatPrinter.h>

// Buffer which can hold 400 lines
byte buffer[48 * 400] = {0};

// Create a printer supporting those 400 lines
CatPrinter cat(400);

void setup() {
  Serial.begin(115200);
  Serial.println();

  // Hand buffer to printer
  cat.begin(buffer, sizeof(buffer));

  // Every non-zero color is white
  cat.fillScreen(1);
  cat.setTextWrap(true);
  cat.setTextSize(3);
  cat.setTextColor(0);
  cat.setCursor(0, 24);
  cat.drawCircle(20, 20, 10, 0);
  cat.drawCircle(40, 30, 10, 0);
  cat.drawCircle(40, 30, 20, 0);
  cat.drawCircle(40, 30, 30, 0);
  cat.drawCircle(354, 30, 30, 0);
  cat.print("Powered by\nAdafruit GFX");

  if (cat.connect()) {
    Serial.print("Connected! Printing...");

    // Print it!
    cat.printBuffer();
    // Feed enough paper to show the whole print
    cat.feed(100);

    Serial.println("Disconnecting");
    cat.disconnect();
    Serial.println("Done!");
  } else {
    Serial.println("Could not find printer!");
  }
}

void loop() {
}
