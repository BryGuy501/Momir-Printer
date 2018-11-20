#include <SPI.h>
#include <SD.h>
#include <RotaryEncoder.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Thermal.h>
#include <SoftwareSerial.h>

#define TX_PIN 6
#define RX_PIN 5

const int chipSelect = 4;
const int buttonPin = 2;    // the number of the pushbutton pin

int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin

// the following variables are unsigned long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

// Setup a RoraryEncoder for pins A2 and A3:
RotaryEncoder encoder(A2, A3);

LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

SoftwareSerial mySerial(RX_PIN, TX_PIN); // Declare SoftwareSerial obj first
Adafruit_Thermal printer(&mySerial);     // Pass addr to printer constructor

int cardCounts[16];

void randomCard(int cmc) {

  char * path = malloc (sizeof(char) * 20);
  //char * numCMC = malloc (sizeof(char) * 5);

  //sprintf(numCMC, "");
  //sprintf(path, "cards/%d/info.dat", cmc);
  sprintf(path, "cards/%d/", cmc);

  Serial.print("Root is ");
  Serial.println(path);

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  /*File root = SD.open(path);
  while (root.available()) {
    char c = (char)(root.read());
    if (c >= '0' && c <= '9') {
    Serial.println(c);
    sprintf(numCMC, "%s%c", numCMC, c);
    }
  }
  root.close();
  Serial.println(numCMC);
  int numFiles = atoi(numCMC);
  Serial.println(numFiles);*/
  /*while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      break;
    }
    numFiles++;
    entry.close();
  }
  root.close();*/
  //int card = random(numFiles);
  int card = random(cardCounts[cmc]);

  sprintf(path, "cards/%d/%d/", cmc, (int)(card / 50));
  int fileNum = card % 50;
  File dataFile;
  File root = SD.open(path);
  for (int idx = 0; idx <= fileNum; idx++) {
    dataFile.close();
    dataFile = root.openNextFile();
  }

  // if the file is available, read it:
  if (dataFile) {
    char * textLine = malloc(sizeof(char) * 32);
    sprintf(textLine, "");
    lcd.setCursor(0,1);
    lcd.print(dataFile.name());
    while (dataFile.available()) {
      char c = dataFile.read();
      Serial.write(c);
      if (c == '@') {
        // Center this line of text
        printer.justify('C');
      } else if (c == '%') {
        // Right-align this line of text
        printer.justify('R');
      } else {
        if (c == '\n') {
          printer.println(textLine);
          sprintf(textLine, "");
          // Default to left-aligning text
          printer.justify('L');
        } else {
          sprintf(textLine, "%s%c", textLine, c);
        }
        //printer.write(c); // print thermal // Temporarily disable
      }
    }
    free(textLine);
    printer.feed(3);
    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening file");
  }

  free(path);
  root.close();

  printer.sleep();      // Tell printer to sleep
  delay(3000L);         // Sleep for 3 seconds
  printer.wake();       // MUST wake() before printing again, even if reset
  printer.setDefault(); // Restore printer to defaults

}

void setup() {

  randomSeed(analogRead(0));
  pinMode(buttonPin, INPUT);
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();

  // Begin the thermal printer
  mySerial.begin(9600);
  printer.begin();

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");

  char * numCMC = malloc (sizeof(char) * 5);
  sprintf(numCMC, "");

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File infoFile = SD.open("cards/info.dat");
  int lines = 0;
  while (infoFile.available()) {
    char c = (char)(infoFile.read());
    if (c >= '0' && c <= '9') {
      //Serial.println(c);
      sprintf(numCMC, "%s%c", numCMC, c);
    } else if (c == '\n') {
      cardCounts[lines] = atoi(numCMC);
      sprintf(numCMC, "");
      lines++;
    }
  }
  free(numCMC);
  infoFile.close();

  for (int idx = 0; idx <= 16; idx++) { // TODO: handle variable-sized array
    Serial.print("CMC ");
    Serial.print(idx);
    Serial.print(": ");
    Serial.print(cardCounts[idx]);
    Serial.println();
  }

  lcd.setCursor(3,0);
  lcd.print("Momir Ready");
  delay(1000);
  lcd.setCursor(0,0);
  lcd.print("                ");

}

void loop() {

  static int pos = 0;

  // read the state of the switch into a local variable:
  int reading = digitalRead(buttonPin);

  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer
    // than the debounce delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;

      // only toggle the LED if the new button state is HIGH
      if (buttonState == HIGH) {
        Serial.print("Submit CMC: ");
        Serial.println(pos);
        Serial.println();
        randomCard(pos);
      }
    }
  }

  // save the reading.  Next time through the loop,
  // it'll be the lastButtonState:
  lastButtonState = reading;

  encoder.tick();

  int newPos = encoder.getPosition();
  if (pos != newPos) {

    // Clamp the encoder
    if (newPos < 0) {
      newPos = 0;
      encoder.setPosition(newPos);
    } else if (newPos > 20) {
      newPos = 20;
      encoder.setPosition(newPos);
    }

    Serial.print(newPos);
    Serial.println();
    pos = newPos;

    lcd.setCursor(5,0);
    lcd.print("CMC: ");
    lcd.print(pos);
    lcd.print(" ");

  }

}
