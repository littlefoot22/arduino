
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#ifdef __AVR__
#include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif
#define NUMPIXELS_BAR 4
#define NUMPIXELS_FLAME 8
#define NUMPIXELS_CHARGE 3
#define PIN      6
#define PIN_CHARGE      3
#define DELAY_TIME_GRADIANT (2000)

struct charge_color
{
  int red = 255;
  int green = 165;
  int blue = 0;
};



const long delay_interval_1000 = 150;  // interval at which to blink (milliseconds)
unsigned long previousMillis_charge = 0;  // will store last time LED was updated
unsigned long previousMillis_flame = 100;  // will store last time LED was updated
unsigned long previousMillis_bar = 0;
long delay_interval_10_112 = 0;  // interval at which to blink (milliseconds)
int delayval = 100;
int delayval_bar = 250;

int state = 0;
int currentBar = 0;
int numBars = 2;

Adafruit_NeoPixel pixels(NUMPIXELS_BAR, 5, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel charges(NUMPIXELS_CHARGE, 3, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip(NUMPIXELS_FLAME, 6, NEO_GRB + NEO_KHZ800);


int brightness = 128; 
uint16_t overall_brightness = 128; 
uint16_t overall_brightness_charge = 128;

int life = 256;
int charge_bar_i = 0;
int charge_i = 0;
int buttonState = 0;

int buttonState_8 = 0;
int buttonState_7 = 0;
int buttonState_4 = 0;
int buttonState_2 = 0;

SoftwareSerial BTSerial(10, 11); // RX | TX

const int pixelCount = NUMPIXELS_FLAME;    // number of pixels

int numPixels = NUMPIXELS_FLAME;    // number of pixels

//gradiant1
int loop_i = 0;

//chase
unsigned long trailColor[pixelCount];  // array of colors for the trail
int currentPixel = 0;                 // the current pixel
int trailingPixel = pixelCount;        // the final trailing pixel
int trailDirection = 1;               // direction of the trail
const int trailLength = pixelCount/10;  // length of the fading trail



struct charge_color cc1;

struct charge_color ccc1;
struct charge_color ccc2;

char color_progression[6][10] = {"YELLOW", "ORANGE", 
                              "RED", "BLUE", "PURPLE", "RAINBOW"};


struct charge_color charge_colors[6];
     
unsigned long startTime;  
int color_progression_i = 0;
bool block = false;
bool primary = false;
bool node = false;

int delayTime = 20;




void setup() {

  pinMode(9, OUTPUT);  // this pin will pull the HC-05 pin 34 (key pin) HIGH to switch module to AT mode
  digitalWrite(9, HIGH);

  pixels.begin();
  charges.begin();
  strip.begin();

  pixels.setBrightness(64);
  charges.setBrightness(64);
  strip.setBrightness(64);

  startTime = millis();


  delay_interval_10_112 = random(10,112);
  strip.show(); // Initialize all pixels to 'off'

  pinMode(2, INPUT_PULLUP);
  pinMode(8, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(7, INPUT_PULLUP);

  //run boot 
  run_boot_lights();
  
  if (primary) {
    activate_primary();
  }

  if (node) {
    activate_node();
  }

  startTime = millis();
  activate();
  randomSeed(analogRead(0));


  for(int i = 0; i < 7; i++) {
    struct charge_color cc1;
    if (i == 0) {
      cc1.red = 255;
      cc1.green = 255;
      cc1.blue = 0;
    }
    if (i == 1) {
      cc1.red = 255;
      cc1.green = 165;
      cc1.blue = 0;
    }
    if (i == 2) {
      cc1.red = 255;
      cc1.green = 0;
      cc1.blue = 0;
    }
    if (i == 3) {
      cc1.red = 0;
      cc1.green = 0;
      cc1.blue = 255;
    }
    if (i == 4) {
      cc1.red = 128;
      cc1.green = 0;
      cc1.blue = 128;
    }
    if (i == 6) {
      cc1.red = 255;
      cc1.green = 255;
      cc1.blue = 255;
    }
    charge_colors[i] = cc1;

  }

}

void loop() {
  if (primary || node) {
    run_change();
    run_flame();
  } else {
    run_neo_pixels();
    sequenceSoloMoverBar();
  }

}

void run_neo_pixels() {
  
  buttonState_4 = digitalRead(8);
  if (buttonState_4 == LOW) {
    overall_brightness = overall_brightness*2;
    
    if (overall_brightness >= 255) {
      overall_brightness = 255;
    }    

    pixels.setBrightness(overall_brightness); 
    charges.setBrightness(overall_brightness);
    strip.setBrightness(overall_brightness);
    
    pixels.show();
    charges.show();
    strip.show();
  }

  buttonState_4 = digitalRead(4);
  if (buttonState_4 == LOW) {
    overall_brightness = overall_brightness/2;

    if(overall_brightness <= 1) {
      overall_brightness = 1;
    }
      
    pixels.setBrightness(overall_brightness); 
    charges.setBrightness(overall_brightness);
    strip.setBrightness(overall_brightness);
    
    pixels.show();
    charges.show();
    strip.show();
  }
  
  
  //activate_gradient1();
  //activate_chase();
    //if( startTime + delayTime < millis() ) {
  activate();
      // startTime = millis();  
    //}

}

void activate_gradient1() {

  if( startTime + DELAY_TIME_GRADIANT < millis() ) {
    startTime = millis();  
    int red = 128;
    int blue = 128;
    int green = 0;

    for( int i = 0; i < NUMPIXELS_BAR; i++ ) {
      loop_i++;
      pixels.setPixelColor(i, red, loop_i*4, blue );
    }
    for( int i = 0; i < NUMPIXELS_CHARGE; i++ ) {
      loop_i++;
      charges.setPixelColor(i, red, loop_i*4, blue );
    }
    for( int i = 0; i < NUMPIXELS_FLAME; i++ ) {
      loop_i++;
      strip.setPixelColor(i, red, loop_i*4, blue );
    }
    pixels.show();
    charges.show();
    strip.show();
  }
}

void run_change() {

    buttonState_4 = digitalRead(8);
    if (buttonState_4 == LOW) {
     overall_brightness_charge = overall_brightness_charge+10;
     if (overall_brightness_charge >= 255) {
        overall_brightness_charge = 0;
     }    

     pixels.setBrightness(overall_brightness_charge); 
     charges.setBrightness(overall_brightness_charge);
     strip.setBrightness(overall_brightness_charge);
    
     pixels.show();
     charges.show();
     strip.show();
     delay(100);
    }

    unsigned long currentMillis = millis();

    while (BTSerial.available()) {
      state = BTSerial.read();
      writeDamage(state);
      if (life <= 0) {
        BTSerial.write('9');
        while (true) {
          for( int i = 0; i < NUMPIXELS_FLAME; i++ ) {
            loop_i++;
            strip.setPixelColor(i, 0, 255, 255);
          }
          strip.show();
          delay(10000);
        }
      }
      for (int i = 0; i < 10; i++) {
        for( int i = 0; i < NUMPIXELS_BAR; i++ ) {
          loop_i++;
          pixels.setPixelColor(i, 255, 0, 0);
        }
        for( int i = 0; i < NUMPIXELS_CHARGE; i++ ) {
          loop_i++;
          charges.setPixelColor(i, 255, 0, 0);
        }
        
        for( int i = 0; i < NUMPIXELS_FLAME; i++ ) {
          loop_i++;
          strip.setPixelColor(i, 255, 0, 0);
        }
        pixels.show();
        charges.show();
        strip.show();
        delay(100);
        
        pixels.clear();
        charges.clear();
        strip.clear();
        pixels.show();
        charges.show();
        strip.show();
        delay(100);
      }
    }

    //Serial.println("state :: " + state);
     
    buttonState_4 = digitalRead(4);
    if (charge_i > 1 && buttonState_4 == LOW) {
      //Serial.println("buttonState");
      //BTSerial.write('1');
      writeNumber(color_progression_i);
      charge_i = 0;
      charges.clear();
      charges.show();
      color_progression_i++;
    }

    buttonState = digitalRead(7);
    if (buttonState == LOW) {
      charge_i = 0;
      color_progression_i = 0;
      block = true;
      
      //charges.clear();
      // charges.show();
      
      //pixels.clear();
      // pixels.show();

      //strip.show();
      //strip.clear();

      for (int i = 0; i < 4; i++) {
        pixels.setPixelColor(i, pixels.Color(255, 255, 255));
      }

      for (int i = 0; i < 2; i++) {
        charges.setPixelColor(i, pixels.Color(255, 255, 255));
      }
      
      pixels.show();
      charges.show();

      //strip.clear();
      return 0;    
      
    } else if (block) {
      block = false;
      
      charges.clear();
      charges.show();

    }

    if (currentMillis - previousMillis_charge >= delay_interval_1000) {
          previousMillis_charge = currentMillis;

          pixels.setPixelColor(charge_bar_i, pixels.Color(
            charge_colors[color_progression_i].red, 
            charge_colors[color_progression_i].green, charge_colors[color_progression_i].blue));
          pixels.show();
          charge_bar_i++;
          buttonState_2 = digitalRead(2);
          
          //clear if press early
          if (charge_bar_i != 4 && buttonState_2 == LOW) {
            color_progression_i = 0;
            charge_i = 0;
            charge_bar_i = 0;
            pixels.clear();
            charges.clear();
            charges.show();
          }
          
          //turn charge on
          if (charge_bar_i == 4 && buttonState_2 == LOW) {
            charges.setPixelColor(charge_i, pixels.Color(
              charge_colors[color_progression_i].red, 
              charge_colors[color_progression_i].green, charge_colors[color_progression_i].blue));
            charges.show();
            charge_i++;

          }

          //clear at end
          if(charge_bar_i == 4) {
            charge_bar_i = 0;
            pixels.clear();
          }
    }

}

void run_flame_random(int r, int g, int b) {

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis_flame >= delay_interval_10_112) {
    previousMillis_flame = currentMillis;
    //  Uncomment one of these RGB (Red, Green, Blue) values to
    //  set the base color of the flame.  The color will flickr
    //  based on the initial base color

    //  Purple flame:
    //  int r = 158, g = 8, b = 148;

    //  Green flame:
    //int r = 74, g = 150, b = 12;

    //  Flicker, based on our initial RGB values
    for (int i=0; i<strip.numPixels(); i++) {
      
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

void run_flame() {

    buttonState = digitalRead(7);
    if (buttonState == LOW) {
      
      for (int i = 0; i < 7; i++) {
        strip.setPixelColor(i, pixels.Color(255, 255, 255));
      }  
      
      strip.show();
      return 0;    
      
    }


  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis_flame >= delay_interval_10_112) {
    previousMillis_flame = currentMillis;
    //  Uncomment one of these RGB (Red, Green, Blue) values to
    //  set the base color of the flame.  The color will flickr
    //  based on the initial base color

    //  Purple flame:
    //  int r = 158, g = 8, b = 148;

    //  Green flame:
    //int r = 74, g = 150, b = 12;

    //  Flicker, based on our initial RGB values
    for (int i=0; i<strip.numPixels(); i++) {
            
      int r = charge_colors[color_progression_i].red;
      int b = charge_colors[color_progression_i].blue;
      int g = charge_colors[color_progression_i].green;
      
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

void run_boot() {
  buttonState = digitalRead(7);
  run_boot_lights();

}

void run_boot_lights() { 

  // int r = random(255);
  // int g = random(255);
  // int b = random(255);
  int r = 255;
  int g = 0;
  int b = 0;

  for(int i=0;i<NUMPIXELS_BAR;i++) {
      buttonState_2 = digitalRead(2);      
      if (buttonState_2 == LOW) {
        primary = true;
      }
      
      buttonState_7 = digitalRead(7);
      if (buttonState_7 == LOW) {
        node = true;
      }
      
      pixels.setPixelColor(i, pixels.Color(r,g,b));
      pixels.show(); // This sends the updated pixel color to the hardware.
      delay(delayval); // Delay for a period of time (in milliseconds).
  }
  
  for(int i=0;i<NUMPIXELS_CHARGE;i++) {
      buttonState_2 = digitalRead(2);      
      if (buttonState_2 == LOW) {
        primary = true;
      }
      
      buttonState_7 = digitalRead(7);
      if (buttonState_7 == LOW) {
        node = true;
      }
      
      charges.setPixelColor(i, charges.Color(r,g,b));
      charges.show(); // This sends the updated pixel color to the hardware.
      delay(delayval); // Delay for a period of time (in milliseconds).
  }
  
  for(int i=0;i<NUMPIXELS_FLAME;i++) {
      buttonState_2 = digitalRead(2);      
      if (buttonState_2 == LOW) {
        primary = true;
      }
      
      buttonState_7 = digitalRead(7);
      if (buttonState_7 == LOW) {
        node = true;
      }

      strip.setPixelColor(i, strip.Color(r,g,b));
      strip.show(); // This sends the updated pixel color to the hardware.
      delay(delayval); // Delay for a period of time (in milliseconds).
  }
}

void activate_node() {
    Serial.begin(9600);
    Serial.println("Enter AT commands:");
    BTSerial.begin(38400);  // HC-05 default speed in AT command more
    delay(500);
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
    Serial.begin(9600);
    Serial.println("Enter AT commands:");
    BTSerial.begin(38400);  // HC-05 default speed in AT command more
    delay(500);
    
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





// initial colors, you can change these
int red = 100;
int blue = 255;
int green = 255;

// stores the start time, use unsignged long to prevent overflow

//unsigned long startTime;  


// start pixel, use for gradients
int startPixel = 0;
int startPixel_bar = 0;

// flag for switches
boolean bSwitchDown = false;

 // for brightness 
//int brightness = 255; 
int brightDirection = -10;

// modulus variable
int xmasMod = 0;

// which sequence we are using and the max # of sequences
int sequenceNum = 0;
int numSequences = 6;
// switch to an new random sequence
int switchSequence() {
  int currentNum = sequenceNum;
  
  // this while loop ensures that we don't repeat the current sequence
  while( currentNum == sequenceNum ) 
    sequenceNum = random(numSequences);
    
  //strip.setBrightness(brightness);  
  return sequenceNum;
}

void switchBar() {

  int charge_r = random(255);
  int charge_g = random(255);
  int charge_b = random(255);

    
  charges.setPixelColor(0, charges.Color(charge_r, charge_g, charge_b));
  charges.setPixelColor(1, charges.Color(charge_r, charge_g, charge_b));
  charges.setPixelColor(2, charges.Color(random(255),random(255),random(255)));

  charges.show(); // This sends the updated pixel color to the hardware.
  //delay(delayval); // Delay for a period of time (in milliseconds).
  // this while loop ensures that we don't repeat the current sequence
  currentBar = random(numBars);
    
  //strip.setBrightness(brightness);  
}

// call appropiate function based on sequence number
void activate() {
    
    buttonState_2 = digitalRead(2);      
    if (buttonState_2 == LOW) {
      brightness = 255;
      ccc1.red = random(255);
      ccc1.green = random(255);
      ccc1.blue = random(255);
      
      ccc2.red = random(255);
      ccc2.green = random(255);
      ccc2.blue = random(255);
      strip.clear();
      switchSequence();
      delay(500); 
    }

    buttonState_7 = digitalRead(7);      
    if (buttonState_7 == LOW) {
      switchBar();
      delay(500); 
    }

    if( sequenceNum == 0 ) { 
      Serial.println("0");
      sequenceGradient();
      //sequenceGradientRed();
    }
    else if( sequenceNum == 1 ) {
      Serial.println("1");
      sequenceSoloMover();
    }
    else if( sequenceNum == 2 ) {
      Serial.println("2");
      sequenceBrightShift();
    }
    else if( sequenceNum == 3 ) {
      Serial.println("3");
      sequenceRandom();
    }
    else if( sequenceNum == 4 ) {
      Serial.println("4");
      //run_flame_random(ccc2.red, ccc2.green, ccc2.blue);
      sequenceXmas();
    }
    else if( sequenceNum == 5 ) {
      Serial.println("5");
      sequenceTwoRandom(ccc1, ccc2);
    }
    // else if( sequenceNum == 6 ) {
    //   Serial.println("6");
    //   run_flame();
    //   //sequenceTwoRandom(ccc1, ccc2);
    // }

    if( currentBar == 0 ) { 
      sequenceSoloMoverBar();
      //sequenceGradientRed();
    }
    else if( currentBar == 1 ) {
      sequenceRandomBar();
    }
}

//---------  SEQUENCE GRADIENT FUNCITONS -------------//
void sequenceGradient() {
   int sp = startPixel;

   for( int i = 0; i < NUMPIXELS_FLAME; i++ ) {
      strip.setPixelColor(sp, i*32, 0, blue );
      
      if( sp == numPixels )
         sp = 0;
       else
         sp++;
   }
      
  strip.show();
  
  startPixel++;
  if( startPixel == numPixels )
    startPixel = 0;
    
  delay(100);

}

//---------  SEQUENCE GRADIENT FUNCITONS -------------//
void sequenceGradientRed() {
   int sp = startPixel;

   for( int i = 0; i < NUMPIXELS_FLAME; i++ ) {
      strip.setPixelColor(sp, red, green, i*32 );
      
      if( sp == numPixels )
         sp = 0;
       else
         sp++;
   }
      
  strip.show();
  
  startPixel++;
  if( startPixel == numPixels )
    startPixel = 0;
    
  delay(100);

}

//---------  SEQUENCE SOLO MOVER: a single pixel moves around the strip -------------//
void sequenceSoloMover() {
   for( int i = 0; i < numPixels; i++ ) {
     if( i == startPixel && i != 7)
         strip.setPixelColor(i, 255,0,0);
     else
          strip.setPixelColor(i, 0,255,255);
   }
      
  strip.show();
  
  startPixel++;
  if( startPixel == numPixels )
    startPixel = 0;
  
  delay(100);
}


//---------  SEQUENCE SOLO MOVER: a single pixel moves around the strip -------------//
void sequenceSoloMoverBar() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis_bar >= delayval_bar) {
   
   previousMillis_bar = currentMillis;
   for( int i = 0; i < NUMPIXELS_BAR; i++ ) {
     if( i == startPixel_bar )
         pixels.setPixelColor(i, 255,0,0);
     else
        pixels.setPixelColor(i, 0,255,255);
   }
      
  pixels.show();
  
  startPixel_bar++;
  if( startPixel_bar == NUMPIXELS_BAR )
    startPixel_bar = 0;
  }
  //delay(100);
}

//---------  SEQUENCE RANDOM: Each pixel is a random color -------------//
void sequenceRandomBar() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis_bar >= delayval_bar) {
   previousMillis_bar = currentMillis;
   for( int i = 0; i < NUMPIXELS_BAR; i++ )
     pixels.setPixelColor(i, random(255), random(255) , random(255));
   
    pixels.show();
  }
  //delay(100);
}


//---------  SEQUENCE BRIGHT SHIFT: brightness fades up/down -------------//
void sequenceBrightShift() {
   for( int i = 0; i < numPixels; i++ ) 
     strip.setPixelColor(i, 255,0,0);
     
  strip.setBrightness(brightness);  
  strip.show();
  
  brightness = brightness + brightDirection;
  if( brightness < 0 ) {
     brightness = 0;
     brightDirection = -brightDirection;
  }
  else if( brightness > 255 ) {
     brightness = 255;
     brightDirection = -brightDirection;
  }
  delay(100);
}

//---------  SEQUENCE RANDOM: Each pixel is a random color -------------//
void sequenceRandom() {
   for( int i = 0; i < numPixels; i++ )
     strip.setPixelColor(i, random(255), random(255) , random(255));
   
  strip.show();
  delay(100);
}

//---------  SEQUENCE XMAS: Alternate green and red pixels -------------//
void sequenceXmas() {
   for( int i = 0; i < numPixels; i++ ) {
     if( i % 2 == xmasMod )
        strip.setPixelColor(i, 255,0,0);
     else
        strip.setPixelColor(i, 0,255,0); 
   }
   
   if( xmasMod == 0 )
     xmasMod = 1;
   else
     xmasMod = 0;
     
  strip.show();
  delay(100);
}

void sequenceTwoRandom(charge_color color1, charge_color color2) {
   
   for( int i = 0; i < numPixels; i++ ) {
     if( i % 2 == xmasMod )
        //strip.setPixelColor(i, color1.red, color1.green, color1.blue);
        setPixelColor(2, i, color1.red, color1.green, color1.blue);
     else
        setPixelColor(2, i, color2.red, color2.green, color2.blue); 
   }
   
   if( xmasMod == 0 )
     xmasMod = 1;
   else
     xmasMod = 0;
     
  strip.show();
  delay(100);
}

void writeNumber(int color) {
  switch(color) {
   case 0:
      Serial.println("wrote 0");
      BTSerial.write('0');
      break; 
   case 1:
      Serial.println("wrote 1");
      BTSerial.write('1');
      break; 
   case 2:
      Serial.println("wrote 2");
      BTSerial.write('2');
      break; 
   case 3:
      Serial.println("wrote 3");
      BTSerial.write('3');
      break; 
   case 4:
      Serial.println("wrote 4");
      BTSerial.write('4');
      break; 
   default : /* Optional */
      BTSerial.write('1');
  }
}

void writeDamage(char attack) {
  switch(attack) {
   case '0':
      life = life -2;
      break; 
   case '1':
      life = life -4;
      break; 
   case '2':
      life = life -8;
      break; 
   case '3':
      life = life -16;
      break; 
   case '4':
      life = life -32;
      break; 
   default : /* Optional */
      BTSerial.write('1');
  }
}

void setPixelColor(int pix, int i, int r, int g, int b) {

  if (pix == 0) {
    pixels.setPixelColor(i, r, g, b);
    pixels.show();
  } else if (pix == 1) {
    charges.setPixelColor(i, r, g, b);
    charges.show();
  } else if (pix == 2) {
    strip.show();
    strip.setPixelColor(i, r, g, b);
  }
}
