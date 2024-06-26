//  Flipdot
//  Sebastius 2018
//  CC BY-SA 4.0
//
//  https://github.com/sebastius/dotflipper
//  https://revspace.nl/dotflipper
//
//  The Dotflipper is a generic FlipDot controller, for 32x16 panels without their own driver chip.
//  There can be up to 8 panels on the i2c bus for a maximum of 4096 dots of flippin' goodness.
//  Adressing is done per panel, starting top left so:
//
//  0 1    or  0 1 2 3 4 5 6 7   or 0    or 0 1 2   etc...
//  2 3                             1       3 4 5
//  4 5                             2
//  6 7                             3
//
//  Set the adresses on the board using the DIP switch top right.
//  A cheat-sheet is provided on the board.
//  (0 = 000, 1 = 001, 2 = 010, 3 = 011, 4 = 100, 5 = 101, 6 = 110, 7 = 111)
//
//  PCA9685 cheatsheet
//  Datasheet at
//  https://www.nxp.com/products/analog/interfaces/ic-bus/ic-led-controllers/16-channel-12-bit-pwm-fm-plus-ic-bus-led-controller:PCA9685
//
//  Specific for this flipdot controller:
//
//  All adressing is done through I2C
//
//  Addresses 0x40 - 0x5F columndrivers (4 on a board, for 32 colums)
//            0x60 - 0x6F rowdriver (2 on a bord, for 16 rows)
//            0x70        all call
//
//  Note: The PCF9685 does have adress ranges up to 7F but after 0x6F
//        there are some adresses used for other purposes (0x70 ALL_CALL for instance)
//        to make $math easier, i chose not to go above 6F (48 chips distributed over 8 boards)
//        In theory you could add two more boards. Since i only have 8 flipdot panels of this type
//        I won't bother. ;)
//
//  Registers PCA9685:
//  Registers 0x00 - Mode 0 register (see below)
//            0x07 - led 00 on   - data 0x10 ON, 0x00 OFF
//            0x09 - led 00 off  - data 0x10 ON, 0x00 OFF
//            0x43 - led 15 on
//            0x45 - led 15 off
//            0xfb - all leds on
//            0xfd - all leds off
//            Note: there are a few more registers around each LED
//            But if you write to the 0x07 and 0x09 registers (and onward),
//            you can ignore the others.
//
//  Mode 0    0x00 - Normal operation
//            0x80 - Reset
//
//  Theory of operation to flip a dot
//   - Column positive voltage + row gnd = BLACK dot
//   - Column gnd + row positive voltage = YELLOW dot
//
//   To get a black dot:  Column driven by UDN2981 and Row driven by ULN2803
//   To get a yellow dot: Column driven by ULN2803 and Row driven by UDN2981

//#define DEBUG
#define GOL
//#define SNAKE
//#define TETRIS
//#define NETWORK

// Network function example QR code
// qrencode -t ascii -m 2 'HOLY SHIT!' | perl -lne's/(.)(.)/$1/g; $_ .= "#" x (32 - length $_); tr/ #/10/; 0 and print $_; $x .= $_ }{ $x .= "1" x 32 for $. .. 31; 0 and print $x; 1 and print pack "b*", $x'  | nc -w0 -u 10.42.45.255 1337

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

const char* ssid = "SSID_GOES_HERE";
const char* password = "PASSWORD_GOES_HERE";

#ifdef NETWORK
#include "font.h"
#endif

#ifdef TETRIS
#include "tetris.h"

enum key_state {NONE, JOY_UP, JOY_DOWN, JOY_LEFT, JOY_RIGHT, JOY_PRESSED};
enum key_state key = NONE, prev_key = NONE;
unsigned long cur_time;

#endif

#include <Wire.h>

#define width 3         // in number of panels
#define height 2        // in number of panels
#define panelwidth  32  // in dots
#define panelheight 16  // in dots
#define COLS (width*panelwidth)
#define ROWS (height*panelheight)
#define pulsetime 1     // in ms, How long should the pulse last?
#define offtime 1       // in ms, Delay to to power down

#define nunchuk_lowtres 90
#define nunchuk_hitres 170

#if width==0 || height==0 || width*height > 8
#error "Please check width and height: width and height should both be > 0 and width*height should not be >8!"
#endif

// Framebuffers
boolean current_state[COLS][ROWS];
boolean next_state[COLS][ROWS];

//max size as defined in showotapicture
const boolean OTA_picture[32][32] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
  {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
  {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
  {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
  {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

#ifdef SNAKE
//SNAKE//
boolean dl = false, dr = false, du = false, dd = false; // to check in which direction the snake is currently moving
boolean l, r, u, d, p = HIGH; // direction

int snakex[200], snakey[200], slength;
int tempx = 10, tempy = 10, xx, yy;
int xegg, yegg;

unsigned long mytime = 280;
unsigned long schedule;

int score = 0, flag = 0;
//END SNAKE//
#endif

#ifdef NETWORK
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

WiFiUDP udp;
uint16_t udpPort = 1337;
#endif

void flipdot(uint16_t x, uint16_t y, bool color) {
  // This routine seems a bit convoluted, calculating the addresses for each pixel when you could make a map beforehand.
  // However the math takes up < 1ms for 8 panels in total (64x64 sets of calculations) on an 80mhz ESP8266.

  // Two ideas for improvement:
  // - flipping a dot on all 8 panels
  // - partial refresh (so not changing what doesnt need changing)
  // Both require an actual framebuffer, so thats next for this project.

  // Input sanitation. Out of range? We wrap around.
  // Sanitation is needed because this panel is headed for hacker-events...
  // Stuff will break (actual short circuits, blown chips, coils and diodes) when addressing is wrong.
  x %= width * panelwidth;
  y %= height * panelheight;

  // Quick mafs
  // Find out what panel we're on
  byte xpanelcoordinate = x / panelwidth;
  byte ypanelcoordinate = y / panelheight;
  byte panelnumber = xpanelcoordinate + (ypanelcoordinate * width);

  // Determine which drivers we want to speak to
  byte columndriveraddress = 0x40 + ((panelnumber * 4) + ((x % panelwidth)  / 8));
  byte rowdriveraddress    = 0x60 + ((panelnumber * 2) + ((y % panelheight) / 8));

  // Now we determine what column and row we're on (%8) and determine which register matches
  // If color is HIGH then we need the column to be at positive rail and row at gnd-rail.
  // If color is LOW then we need the row to be at positive rail and column at gnd-rail.
  // This is done in hardware by using the first 8 outputs of the chip for the gnd-rail and the second 8 for the positive rail.
  byte column = x % 8;
  byte columnregisteroffset = (color) ? 0x07 : 0x27;
  byte columnregister = 4 * column + columnregisteroffset;

  byte row = y % 8;
  byte rowregisteroffset = (color) ? 0x27 : 0x07;
  byte rowregister =  4 * row + rowregisteroffset;

  // Safety
  // If you try to use multiple rows or columns on a panel you will get magic smoke,
  // so we switch everything off on that panel for safety reasons:

  // rows
  //  for (uint8_t address = 0x60 + (panelnumber * 2); address < 0x62 + (panelnumber * 2); address++) {
  //    everythingoff(address);
  //  }
  //
  //  // columns
  //  for (uint8_t address = 0x40 + (panelnumber * 4); address < 0x44 + (panelnumber * 4); address++) {
  //    everythingoff(address);
  //  }

  // Now we will flip some bits!
  // Powerup!
  i2cwrite(rowdriveraddress,    rowregister          , 0x10);
  //i2cwrite(rowdriveraddress,    rowregister    + 0x02, 0x00);

  delayMicroseconds(100);

  i2cwrite(columndriveraddress, columnregister       , 0x10);
  //i2cwrite(columndriveraddress, columnregister + 0x02, 0x00);

  delayMicroseconds(100);

  // And powerdown!
  i2cwrite(rowdriveraddress,    rowregister          , 0x00);
  //i2cwrite(rowdriveraddress,    rowregister    + 0x02, 0x10);

  delayMicroseconds(100);

  i2cwrite(columndriveraddress, columnregister       , 0x00);
  //i2cwrite(columndriveraddress, columnregister + 0x02, 0x10);

  delayMicroseconds(100);
}
void fullupdate() {
  for (uint8_t x = 0; x < COLS; x++) {
    for (uint8_t y = 0; y < ROWS; y++) {
      if (current_state[x][y] != next_state[x][y]) {
        flipdot(x, y, next_state[x][y]);
      }
    }
  }
  memcpy(current_state, next_state, sizeof(next_state));
}

void i2cwrite(byte address, byte reg, byte content) {
#ifdef DEBUG
  Serial.print("I2C: ");
  if (address < 0x10) {
    Serial.print("0");
  }
  Serial.print(address, HEX);
  Serial.print(" ");
  if (reg < 0x10) {
    Serial.print("0");
  }
  Serial.print(reg, HEX);
  Serial.print(" ");
  if (content < 0x10) {
    Serial.print("0");
  }
  Serial.println(content, HEX);
#endif
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(content);
  Wire.endTransmission();
}

void resetPCA9685() {
  for (uint8_t x = 0x40; x < 0x6F; x++) {
    i2cwrite(x, 0x00, 0x80); // ???
    i2cwrite(x, 0x00, 0b00000001); // Awake and responds to 'all call'
    i2cwrite(x, 0x01, 0b00000100); //
    i2cwrite(x, 0x00, 0b00010001); // Sleep mode and responds to 'all call'
    i2cwrite(x, 0xFE, 0x03);       // Set PWM frequency to 1526hz (maximum), formula is round( (osc_clock / ( 4096*update_rate ) ) - 1 ), so round ( ( 25mhz / (4096*1526) ) - 1 ) = 0x03
    i2cwrite(x, 0x00, 0b00000001); // Awake and responds to 'all call'
    everythingoff(x);
  }
}

void everythingoff(uint8_t address) {
  // Write to the ALL_LED registers to switch them all off.
  i2cwrite(address, 0xFA, 0x00);
  i2cwrite(address, 0xFB, 0x00);
  i2cwrite(address, 0xFC, 0x00);
  i2cwrite(address, 0xFD, 0x00);
}

void ClearScreen() {
  memset(current_state, 1, sizeof(current_state));
  memset(next_state, 0, sizeof(next_state));
  fullupdate();
}

void YellowScreen() {
  memset(current_state, 0, sizeof(current_state));
  memset(next_state, 1, sizeof(next_state));
  fullupdate();
}

void pixelwalker() {
  for (uint8_t x = 0; x < 32; x++) {
    for (uint8_t y = 0; y < 32; y++) {
      flipdot(x, y, 1);
      delay(50);
      flipdot(x, y, 0);
    }
  }
}

void i2cscanner() {
  byte error, address;
  int nDevices;

  Serial.println("Scanning I2C bus...");

  nDevices = 0;
  for (address = 1; address < 127; address++ ) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      Serial.print((address < 0x10) ? "0" : "");
      Serial.println(address, HEX);
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      Serial.print((address < 0x10) ? "0" : "");
      Serial.println(address, HEX);
    }
  }
  Serial.println("I2C scan done\n");
  if (nDevices == 0) {
    Serial.println("No I2C devices found, infinite loop now so you can check everything.");
    infiniteloop();
  } else {
    Serial.print(nDevices, DEC);
    Serial.print(" I2C device");
    Serial.print((nDevices > 1) ? "s" : "");
    Serial.println(" found");
  }
}

void infiniteloop() {
  while (1) {
    yield(); // so the ESP doesnt crash
  }
}

#ifdef GOL
void GoLnext() {
  memset(next_state, 0, sizeof(next_state));
  int x;
  int y;

  boolean value;

  for (int r = 0; r < ROWS; r++) { // for each row
    for (int c = 0; c < COLS; c++) { // and each column

      // count how many live neighbors this cell has
      int liveNeighbors = 0;
      for (int i = -1; i < 2; i++) {
        y = r + i;

        // wrap y
        if (y == -1) {
          y = ROWS - 1;
        } else if (y == ROWS) {
          y = 0;
        }

        for (int j = -1; j < 2; j++) {
          if (i != 0 || j != 0) {
            x = c + j;

            // wrap
            if (x == -1) {
              x = COLS - 1;
            } else if (x == COLS) {
              x = 0;
            }

            if (current_state[x][y]) {
              liveNeighbors++;
            }
          }
        }
      }
      //Serial.print(liveNeighbors);

      // apply the rules
      if (current_state[c][r] && liveNeighbors >= 2 && liveNeighbors <= 3) { // live cells with 2 or 3 neighbors remain alive
        value = true;
      } else if (!current_state[c][r] && liveNeighbors == 3) { // dead cells with 3 neighbors become alive
        value = true;
      } else {
        value = false;
      }

      next_state[c][r] = value;
    }
  }


  //memcpy(current_state, next_state, sizeof(next_state));
}

void GoLrandomize() {
  int slider = 90;
  int num;
  boolean value;

  randomSeed(millis());
  //memset(current_state, 0, sizeof(current_state));
  //memset(next_state, 0, sizeof(next_state));

  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      num = random(1, 100);

      value = num >= slider;
      if (value) {
        next_state[c][r] = value;

      }
    }
  }

}
#endif

void showotapicture() {
  for (uint8_t x = 0; x < 32; x++) {
    for (uint8_t y = 0; y < 32; y++) {
      next_state[y][x] = OTA_picture[x][y];
    }
  }
  fullupdate();
}



void setup() {


//  Wire.begin();
  Wire.begin(21,22);
  Wire.setClock(1000000); // ESP32

  Serial.begin(115200);
  Serial.println();

  Serial.println("Flipdot software");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

#ifdef DEBUG
  i2cscanner();
  for (uint8_t t = 10; t > 0; t--) {
    Serial.print("Waiting ");
    Serial.print(t);
    Serial.println(" seconds to continue");
    delay(1000);
  }
  Serial.print("Resuming regular code");
#endif

  Serial.println("Flipdot getting ready to go!");

  resetPCA9685();
  YellowScreen();
  ClearScreen();
// what is flipdemo?
  //flipdemo();

#ifdef SNAKE
  snakesetup();
#endif

#ifdef NETWORK
  const char *ssid = "revspace-pub-2.4ghz";
  const char *password = "";
  Serial.printf("Connecting to: %s\n", ssid);
  WiFi.begin(ssid, password);
  uint16_t progress = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    flipdot(progress % 32, progress / 32, 1);
    progress++;
  }
  Serial.println();
  for (int i = progress - 1; i >= 0; i--) flipdot(i % 32, i / 32, 0);
  Serial.printf("WiFi connected, IP address: %s\n", WiFi.localIP().toString().c_str());

  uint32_t ip = (uint32_t)(WiFi.localIP());
  for (int octet = 0; octet < 4; octet++) {
    uint8_t b = (ip >> (octet * 8)) & 0xff;
    for (int fy = 0; fy < 5; fy++) {
      for (int fx = 0; fx < 3; fx++) {
        for (int d = 0, e = 100; d < 3; d++, e /= 10) {
          uint8_t num = (b / e) % 10;
          if (num || d) flipdot(fx + d * 4, fy + octet * 6, (fontDigits[num][fy] >> (2 - fx)) & 1);
        }
      }
    }
  }


  udp.begin(udpPort);
  Serial.printf("Running UDP server on port: %d\n", udpPort);
  delay(5000);
  ClearScreen();
#endif

  ArduinoOTA.setHostname("flipdot");

  // No authentication by default
  ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
    ClearScreen();
    showotapicture();

  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    //Serial.println(progress / (total / 32));
    // Serial.println(total);
    Serial.printf("Progress: %u dots\r", (progress / (total / 32)));
    byte progressdot = ((progress / (total / 27)));
    flipdot(progressdot + 2, 3, 1);
    flipdot(progressdot + 2, 4, 1);
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

}

#ifdef SNAKE
#include <NintendoExtensionCtrl.h>
ClassicController controller;

void snakesetup()
{
  controller.begin();
  while (!controller.connect()) {
    Serial.println("Controller not detected!");
    delay(1000);

  }
  slength = 3;               //Start with snake length 8
  xegg = COLS / 2;
  yegg = ROWS / 2;

  //ClearScreen();

  for (uint8_t i = 0; i <= slength; i++) //Set starting coordinates of snake
  {
    snakex[i] = COLS / 3;
    snakey[i] = ROWS / 3;
  }

  for (uint8_t i = 0; i < slength; i++)  //Draw the snake
  {
    flipdot(snakex[i], snakey[i], HIGH);
  }

  dr = true;  //Going to move right initially
  p = HIGH;
}
#endif

#ifdef GOL
void GoL() {
  bool statechange;
  GoLrandomize();
  fullupdate();
  for (uint8_t i = 0; i < 100; i++) {
    ArduinoOTA.handle();
    yield();
    long starttijd = millis();
    GoLnext();
    

      fullupdate();
      long gewacht = (millis() - starttijd);
      const int wachttijd = 75;
      if (gewacht < wachttijd) {
        delay(wachttijd - gewacht);
      }
    }
  }

#endif



#ifdef SNAKE
void readjoystick() {
  bool success = controller.update();  // Get new data from the controller
  if (!success) {
    Serial.println("Controller disconnected!");
    delay(1000);
  }
  //controller.printDebug();

  boolean padUp = controller.dpadUp();
  boolean padDown = controller.dpadDown();
  boolean padLeft = controller.dpadLeft();
  boolean padRight = controller.dpadRight();
  boolean butMinus = controller.buttonMinus();
  boolean butHome = controller.buttonHome();
  boolean butPlus = controller.buttonPlus();


  if (padLeft) {
    l = LOW;
    r = HIGH;
    d = HIGH;
    u = HIGH;
  }

  if (padRight) {
    l = HIGH;
    r = LOW;
    d = HIGH;
    u = HIGH;
  }

  if (padDown) {
    d = LOW;
    u = HIGH;
    l = HIGH;
    r = HIGH;
  }

  if (padUp) {
    d = HIGH;
    u = LOW;
    l = HIGH;
    r = HIGH;
  }

  if (butPlus) {
    p = LOW;
    d = HIGH;
    u = HIGH;
    l = HIGH;
    r = HIGH;
  } else {
    p = HIGH;
  }
}

void movesnake()
{
  readjoystick();
  if (flag == 0)
  {
    snakedirect();    //When key is pressed,this will change the coordinates accordingly and set flag to 1
    //flag would be set to 1 so that direction is not changed multiple times in a short duration
  }

  if (millis() > schedule) //this condition becomes true after every 'time' milliseconds...millis() returns the time since launch of program
  {
    if (flag == 0)                                //flag 0 means no directional key has been pressed in the last 'time' milliseconds
    {
      if (dr == true) {
        tempx = snakex[0] + 1;  // so the snake moves one step in the direction it is moving currently
        tempy = snakey[0];
      }
      if (dl == true) {
        tempx = snakex[0] - 1;  //The new coordinates of head of snake goes in tempx,tempy
        tempy = snakey[0];
      }
      if (du == true) {
        tempy = snakey[0] - 1;
        tempx = snakex[0];
      }
      if (dd == true) {
        tempy = snakey[0] + 1;
        tempx = snakex[0];
      }
    }

    flag = 0;
    checkgame();                              //Check if snake has met egg or coincided with itself
    checkegg();

    if (tempx <= 0) {
      tempx = COLS + tempx; //If the new coordinates are out of screen, set them accordingly
    }
    if (tempx >= COLS) {
      tempx = tempx - COLS;
    }
    if (tempy <= 0) {
      tempy = ROWS + tempy;
    }
    if (tempy >= ROWS) {
      tempy = tempy - ROWS;
    }

    for (uint8_t i = 0; i <= slength; i++) //Change the coordinates of all points of snake
    {
      xx = snakex[i];
      yy = snakey[i];
      snakex[i] = tempx;
      snakey[i] = tempy;
      tempx = xx;
      tempy = yy;
    }

    drawsnake();           //Draw the snake and egg at the new coordinates
    schedule = millis() + mytime;
  }
}


void checkgame()       //Game over checker
{
  for (uint8_t i = 1; i < slength; i++)        //Checking if the coordinates of head have become equal to one of the non head points of snake
  {
    if (snakex[i] == snakex[0] && snakey[i] == snakey[0])
    {
      //insert highscore routine
      YellowScreen();
      ClearScreen();

      slength = 3;            //Resetting the values
      score = 0;
      mytime = 280;

      redrawsnake();              //Restart game by drawing snake with the resetted length and score
    }
  }

}

void checkegg()      //Snake meets egg
{
  if ((snakex[0] == xegg) && (snakey[0] == yegg))
  {
    score += 1;                     //Increase length,score and increase movement speed by decreasing 'time'
    slength += 1;
    if (mytime >= 20)
    {
      mytime -= 20;
    }
    flipdot(xegg, yegg, 0);    //Delete the consumed egg

    xegg = random(1, COLS);           //Create New egg randomly
    yegg = random(1, ROWS);
  }
}


void snakedirect()                  //Check if user pressed any keys and change direction if so
{
  if (l == LOW and dr == false)  //when key LEFT is pressed ,L will become low
  {
    dl = true; du = false; dd = false;
    tempx = snakex[0] - 1;            //Save the new coordinates of head in tempx,tempy
    tempy = snakey[0];
    flag = 1;                   //Do not change direction any further for the ongoing 'time' milliseconds
  }
  else if (r == LOW and dl == false)
  {
    dr = true; du = false; dd = false;
    tempx = snakex[0] + 1;
    tempy = snakey[0];
    flag = 1;
  }
  else if (u == LOW and dd == false)
  {
    du = true; dl = false; dr = false;
    tempy = snakey[0] - 1;
    tempx = snakex[0];
    flag = 1;
  }
  else if (d == LOW and du == false)
  {
    dd = true; dl = false; dr = false;
    tempy = snakey[0] + 1;
    tempx = snakex[0];
    flag = 1;
  }
  else if (p == LOW)            //Pause game for 5 seconds
  {
    ClearScreen();
    for (uint8_t i = 5; i > 0; i--)
    {
      delay(1000);
    }
    redrawsnake();          //Redraw the snake and egg at the same position as it was
  }
}


void drawsnake()        //Draw snake and egg at newly changed positions
{
  flipdot(xegg, yegg, 1);     //Draw egg at new pos
  flipdot(snakex[0], snakey[0], 1);     //Draw new head of snake
  flipdot(snakex[slength], snakey[slength], 0); //Delete old tail of snake
}

void redrawsnake()   //Redraw ALL POINTS of snake and egg
{
  flipdot(xegg, yegg, 1);
  for (uint8_t i = 0; i < slength; i++)
  {
    flipdot(snakex[i], snakey[i], 1);
  }
}
#endif

#ifdef TETRIS
#include <NintendoExtensionCtrl.h>
ClassicController controller;

void tetrissetup()
{
  controller.begin();
  while (!controller.connect()) {
    Serial.println("Controller not detected!");
    delay(1000);

  }
}
#endif



void loop() {
  ArduinoOTA.handle();
  yield();

#ifdef NETWORK
  static uint8_t prevFramebuffer[ROWS * COLS / 8] = { 0 };
  static uint8_t framebuffer[ROWS * COLS / 8] = { 0 };

  int c = udp.parsePacket();
  if (c) {
    Serial.printf("Received %d bytes from %s, port %d\n", c, udp.remoteIP().toString().c_str(), udp.remotePort());
    udp.read(framebuffer, sizeof(framebuffer));
  }

  for (uint16_t y = 0; y < ROWS; y++) {
    for (uint16_t x = 0; x < COLS; x++) {
      uint16_t i = y * COLS + x;

      uint8_t a = (framebuffer[i >> 3] >> (i & 7)) & 1,
              b = (prevFramebuffer[i >> 3] >> (i & 7)) & 1;
      if (a ^ b) {
        flipdot(x, y, a);
      }
    }
  }

  memcpy(prevFramebuffer, framebuffer, sizeof(framebuffer));
  return;
#endif

#ifdef TETRIS
  // tetris
  randomSeed(analogRead(0));
  ClearScreen();
  tetrissetup();
  game_InitGame();
  board_InitBoard();
  refresh_screen();
  key = NONE;
  while (key != JOY_PRESSED) {
    game_DrawBoard();
    game_DrawPiece(mPosX, mPosY, mPiece, mRotation);
    refresh_screen();

    cur_time = millis();
    int piece_delay = 200 - (deleted_lines / 10 % 10) * 40;
    while ((key = read_keys()) == 0) {
      if (millis() - piece_delay > cur_time) {
        break;
      }
    }
    if (key > 0 && prev_key == key && millis() - cur_time < 200) {
      if (key == JOY_DOWN) delay(100);
      else delay(200);
      key = NONE;
    }
    prev_key = key;
    Serial.println(key);
    Serial.println("Lines: " + String(deleted_lines));

    //Serial.println(String(mPosX)+" "+String(mPosY)+" "+String(mPiece));
    switch (key)
    {
      case JOY_UP:
        if (board_IsPossibleMovement (mPosX, mPosY, mPiece, mRotation + 1))
          mRotation = (mRotation + 1) % 4;
        break;
      case JOY_DOWN:
        if (board_IsPossibleMovement (mPosX, mPosY + 1, mPiece, mRotation)) mPosY++;
        break;
      case JOY_LEFT:
        if (board_IsPossibleMovement (mPosX - 1, mPosY, mPiece, mRotation)) mPosX--;
        break;
      case JOY_RIGHT:
        if (board_IsPossibleMovement (mPosX + 1, mPosY, mPiece, mRotation)) mPosX++;
        break;
      case JOY_PRESSED:
        break;
      default:
        // move piece down
        {
          if (board_IsPossibleMovement (mPosX, mPosY + 1, mPiece, mRotation)) {
            mPosY++;
          } else {
            board_StorePiece (mPosX, mPosY, mPiece, mRotation);
            board_DeletePossibleLines ();

            if (board_IsGameOver()) {
              delay(5000);
              key = JOY_PRESSED;
              break;
            }

            game_CreateNewPiece();
          }
        }
    }
  }
  Serial.println("reset");

#endif

#ifdef GOL
  ClearScreen();
  fullupdate();
  GoL();
  ClearScreen();
#endif

#ifdef SNAKE
  movesnake();
  //YellowScreen();
  //ClearScreen();
  //delay(1000);
#endif

  //ClearScreen();
}

#ifdef TETRIS
key_state read_keys () {
  enum key_state key = NONE;
  int joy_x, joy_y, joy_sw;
  bool success = controller.update();  // Get new data from the controller
  if (!success) {
    Serial.println("Controller disconnected!");
    delay(1000);
  }
  boolean padUp = controller.dpadUp();
  boolean padDown = controller.dpadDown();
  boolean padLeft = controller.dpadLeft();
  boolean padRight = controller.dpadRight();
  boolean butMinus = controller.buttonMinus();
  boolean butHome = controller.buttonHome();
  boolean butPlus = controller.buttonPlus();


  if (padLeft) key = JOY_LEFT;
  if (padRight) key = JOY_RIGHT;
  if (padDown) key = JOY_DOWN;
  if (padUp) key = JOY_UP;
  if (butPlus) key = JOY_PRESSED;
  return key;

}
#endif
