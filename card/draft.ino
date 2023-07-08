/*
NeoPixel LEDs

modified on 7 May 2019
by Saeed Hosseini @ Electropeak
**This code is based on Adafruit NeoPixel library Example**
https://electropeak.com/learn/

*/

#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#ifdef __AVR__
#include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

#define PIN_6     6
#define NUMPIXELS 7

SoftwareSerial BTSerial(10, 11); // RX | TX
Adafruit_NeoPixel pixels(NUMPIXELS, PIN_6, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip(4, 5, NEO_GRB + NEO_KHZ800);

#define DELAYVAL 500 // Time (in milliseconds) to pause between pixels
int buttonState = 0;  // variable for reading the pushbutton status
int color = 0;  // variable for reading the pushbutton status
const long delay_interval_1000 = 100;  // interval at which to blink (milliseconds)
const long delay_interval_push = 1000;  // interval at which to blink (milliseconds)
unsigned long previousMillis_charge = 0;  // will store last time LED was updated
unsigned long previousMillis_push = 0;  // will store last time LED was updated
int charge_i = 0;
int rainbow_i = 0;
long delay_interval_10_112 = 0;  // interval at which to blink (milliseconds)
unsigned long previousMillis_flame = 100;  // will store last time LED was updated
int firstPixelHue = 0;
int previousPixelHue = 0;

int pixelHue_1 = 0;


const int buttonPin = 2;  // the number of the pushbutton pin
int charges = 0;
int pixelHue = 0;

int charge_0 = 0;
int charge_1 = 0;
int charge_2 = 0;
int charge_3 = 0;

int charge_4 = 0;
int charge_5 = 0;

int charge_cache = 4;


bool push = false;


void setup() {
  pixels.begin();
  pinMode(buttonPin, INPUT);
  pinMode(2, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(7, INPUT_PULLUP);
  pinMode(8, INPUT_PULLUP);

  strip.begin();
  //strip.setBrightness(brightness);
  delay_interval_10_112 = random(10,112);
  strip.show(); // Initialize all pixels to 'off'
  pixelHue = firstPixelHue + (rainbow_i * 65536L / strip.numPixels());

  pinMode(9, OUTPUT);  // this pin will pull the HC-05 pin 34 (key pin) HIGH to switch module to AT mode
  digitalWrite(9, HIGH);
  Serial.begin(9600);
  Serial.println("Enter AT commands:");
  BTSerial.begin(38400);  // HC-05 default speed in AT command more

}

void loop() {
  if (charge_cache != -1) {
    run_charge();
  } else {
    run_color_combine(0);
  }
  run_flame(0);

}

void run_charge() {
    unsigned long currentMillis = millis();

    buttonState = digitalRead(2);
    // if (charge_i != 5 && buttonState == LOW) {
    if (buttonState == LOW && push == false) {

       Serial.println("charge_cache " + String(charge_cache));
      Serial.println("charge_0 " + String(charge_0));

       push = true;
       if (charge_cache == 4) {
          pixels.setPixelColor(charge_cache, pixels.gamma32(pixels.ColorHSV(charge_0)));
          pixels.show();
          charge_4 = charge_0;
          charge_cache = 5;
       } else if (charge_cache == 5) {
        pixels.setPixelColor(charge_cache, pixels.gamma32(pixels.ColorHSV(charge_0)));
        pixels.show();
        charge_5 = charge_0;
        charge_cache = -1;
       }
       //clear_charge();
    } 

    buttonState = digitalRead(4);
    // if (charge_i != 5 && buttonState == LOW) {
    if (buttonState == LOW && push == false) {
       push = true;
       pixels.setPixelColor(charge_cache, pixels.gamma32(pixels.ColorHSV(charge_1)));
       if (charge_cache == 4) {
          charge_4 = charge_1;
          charge_cache = 5;
       } else if (charge_cache == 5) {
        charge_5 = charge_1;
        charge_cache = -1;
       }       //clear_charge();
    } 

    buttonState = digitalRead(7);
    // if (charge_i != 5 && buttonState == LOW) {
    if (buttonState == LOW && push == false) {
       push = true;
       pixels.setPixelColor(charge_cache, pixels.gamma32(pixels.ColorHSV(charge_2)));
       if (charge_cache == 4) {
          charge_4 = charge_2;
          charge_cache = 5;
       } else if (charge_cache == 5) {
        charge_5 = charge_2;
        charge_cache = -1;
       }       //clear_charge();
    } 

    buttonState = digitalRead(8);
    // if (charge_i != 5 && buttonState == LOW) {
    if (buttonState == LOW && push == false) {
       push = true;
       pixels.setPixelColor(charge_cache, pixels.gamma32(pixels.ColorHSV(charge_3)));
       if (charge_cache == 4) {
          charge_4 = charge_3;
          charge_cache = 5;
       } else if (charge_cache == 5) {
        charge_5 = charge_3;
        charge_cache = -1;
       }       //clear_charge();
    } 


  if (currentMillis - previousMillis_push >= delay_interval_push) {
    previousMillis_push = currentMillis;
    push = false;
  }
  if (currentMillis - previousMillis_charge >= delay_interval_1000) {
    previousMillis_charge = currentMillis;
    pixelHue = firstPixelHue + (rainbow_i * 65536L / strip.numPixels());
    //Serial.println(pixelHue);
    //Serial.println("> " + pixels.ColorHSV(pixelHue));
    firstPixelHue += 256;
    unsigned int asdas = 0;
    rainbow_i++;

    switch(charge_i) {
      case 0:
        pixels.setPixelColor(charge_i, pixels.gamma32(pixels.ColorHSV(pixelHue)));
        charge_0 = pixelHue;
        break;
      case 1:
        pixels.setPixelColor(charge_i, pixels.gamma32(pixels.ColorHSV(pixelHue)));
        charge_1 = pixelHue;
        break;
      case 2:
        pixels.setPixelColor(charge_i, pixels.gamma32(pixels.ColorHSV(pixelHue)));
        charge_2 = pixelHue;
        break;
      case 3:
        pixels.setPixelColor(charge_i, pixels.gamma32(pixels.ColorHSV(pixelHue)));
        charge_3 = pixelHue;
        break;
    }

    asdas = pixels.getPixelColor(0);
    int red = (asdas >> 16) & 0xFF;
    int green = (asdas >> 8) & 0xFF;
    int blue = asdas & 0xFF;
    //Serial.println("asdas " + String(asdas)
     //+ " r " + String(red) 
     //+ " g " + String(green) 
     //+ " b " + String(blue));

    //pixels.setPixelColor(charge_i, pixels.gamma32(pixels.ColorHSV(pixelHue)));
    //pixels.setPixelColor(charge_i, pixels.Color(0, 255, 0));
    pixels.show();
    //delay(100);
    if (charge_i == 4) {
      clear_charge();
    } else {
      charge_i++;
    }

  }
}

void clear_charge() {
  //firstPixelHue = 0;
  rainbow_i = 0;
  charge_i = 0;
  pixelHue_1 = 0;
  //for (int i=0; i <= 4; i++) {
  //  pixels.setPixelColor(i, pixels.Color(0, 0, 0));
  //  pixels.show();
  //}
}

void run_color_combine(int color) {
  
    buttonState = digitalRead(2);
    // if (charge_i != 5 && buttonState == LOW) {
    if (buttonState == LOW && push == false) {
       push = true;

       unsigned int charge_color_4 = pixels.getPixelColor(4);
       int red = (charge_color_4 >> 16) & 0xFF;
       int green = (charge_color_4 >> 8) & 0xFF;
       int blue = charge_color_4 & 0xFF;
       Serial.println("asdas " + String(charge_color_4)
        + " r " + String(red) 
        + " g " + String(green) 
        + " b " + String(blue));

 
       //charge_5 + charge_6;
       //clear_charge();
    } 

    buttonState = digitalRead(4);
    // if (charge_i != 5 && buttonState == LOW) {
    if (buttonState == LOW && push == false) {
       push = true;
    } 

    buttonState = digitalRead(7);
    // if (charge_i != 5 && buttonState == LOW) {
    if (buttonState == LOW && push == false) {
       push = true;
        //clear_charge();
    } 

    buttonState = digitalRead(8);
    // if (charge_i != 5 && buttonState == LOW) {
    if (buttonState == LOW && push == false) {
       push = true;
        //clear_charge();
    } 
  

}

void run_flame(int color) {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis_flame >= delay_interval_10_112) {
    previousMillis_flame = currentMillis;
    //  Uncomment one of these RGB (Red, Green, Blue) values to
    //  set the base color of the flame.  The color will flickr
    //  based on the initial base color
 
    //  Regular (orange) flame:
    int r = 226, g = 121, b = 35;



    //  Purple flame:
    //  int r = 158, g = 8, b = 148;

    //  Green flame:
    //int r = 74, g = 150, b = 12;

    //  Flicker, based on our initial RGB values
    for(int i=0; i<strip.numPixels(); i++) {
      int flicker = random(0,55);
      int r1 = r-flicker;
      int g1 = g-flicker;
      int b1 = b-flicker;
      if(g1<0) g1=0;
      if(r1<0) r1=0;
      if(b1<0) b1=0;
      strip.setPixelColor(i,r1,g1, b1);
    }
    strip.show();

    //  Adjust the delay here, if you'd like.  Right now, it randomizes the
    //  color switch delay to give a sense of realism
    delay_interval_10_112 = random(10,200);
  }
  return 0;
}

void activate_node() {

    Serial.println("AT+RMAAD");
    BTSerial.write("AT+RMAAD\r\n");
    delay(500); // wait for change
    while(BTSerial.available()) {
      Serial.write(BTSerial.read());
    }
    
    Serial.println("AT+ROLE=0");
    BTSerial.write("AT+ROLE=0\r\n");
    delay(500); // wait for change
    while(BTSerial.available()) {
      Serial.write(BTSerial.read());
    }
    
//    Serial.println("AT+CMODE1");
//    BTSerial.write("AT+CMODE=1\r\n");
//    delay(500); // wait for change
//    while(BTSerial.available()) {
//      Serial.write(BTSerial.read());
//    }
//    
    Serial.println("AT+UART=38400,0,0");
    BTSerial.write("AT+UART=38400,0,0\r\n");
    delay(500); // wait for change
    while(BTSerial.available()) {
      Serial.write(BTSerial.read());
    }
    //delay(500); // wait for change
    //Serial.write(BTSerial.read());
}

void activate_primary() {
    Serial.println("AT+RMAAD");
    BTSerial.write("AT+RMAAD\r\n");
    delay(500); // wait for change
    while(BTSerial.available()) {
      Serial.write(BTSerial.read());
    }
    
    Serial.println("AT+ROLE=1");
    BTSerial.write("AT+ROLE=1\r\n");
    delay(500); // wait for change
    while(BTSerial.available()) {
      Serial.write(BTSerial.read());
    }
    
    Serial.println("AT+CMODE1");
    BTSerial.write("AT+CMODE=1\r\n");
    delay(500); // wait for change
    while(BTSerial.available()) {
      Serial.write(BTSerial.read());
    }
    
    Serial.println("AT+UART=38400,0,0");
    BTSerial.write("AT+UART=38400,0,0\r\n");
    delay(500); // wait for change
    while(BTSerial.available()) {
      Serial.write(BTSerial.read());
    }
    //delay(500); // wait for change
    //Serial.write(BTSerial.read());
}
