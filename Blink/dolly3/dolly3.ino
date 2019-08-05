/*
  Blink

  Turns an LED on for one second, then off for one second, repeatedly.

  Most Arduinos have an on-board LED you can control. On the UNO, MEGA and ZERO
  it is attached to digital pin 13, on MKR1000 on pin 6. LED_BUILTIN is set to
  the correct LED pin independent of which board is used.
  If you want to know what pin the on-board LED is connected to on your Arduino
  model, check the Technical Specs of your board at:
  https://www.arduino.cc/en/Main/Products

  modified 8 May 2014
  by Scott Fitzgerald
  modified 2 Sep 2016
  by Arturo Guadalupi
  modified 8 Sep 2016
  by Colby Newman

  This example code is in the public domain.

  http://www.arduino.cc/en/Tutorial/Blink
*/

int brightness = 150;    // how bright the LED is
int fadeAmount = 5;    // how many points to fade the LED by
int mode = 0;
int delayed = 0;
int CHILL_MODE = 0;
// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(0, OUTPUT);
   // initialize digital pin LED_BUILTIN as an output.
  //pinMode(1, OUTPUT);
  randomSeed(analogRead(1));
  CHILL_MODE = random(2);
  if (CHILL_MODE > 0) {
    mode = 5;
  } else {
    mode = 0;
  }
}

// the loop function runs over and over again forever
void loop() {
  // print a random number from 0 to 299
  //int randNumber = random(300);

  if (delayed > 3000 && mode != 5) {
    mode = random(4);
    delayed = 0;
  }

  switch (mode) {
     case 0:
      delayed = delayed + 50;
      digitalWrite(0, HIGH);   // turn the LED on (HIGH is the voltage level)
      delay(25);               // wait for a second
      analogWrite(0, 225);    // turn the LED off by making the voltage LOW
      delay(25);               // wait for a second
      break;
    case 1:
      delayed = delayed + 1000;
      digitalWrite(0, HIGH);   // turn the LED on (HIGH is the voltage level)
      delay(500);               // wait for a second
      analogWrite(0, 225);    // turn the LED off by making the voltage LOW
      delay(500);               // wait for a second
      break;
    case 2:
      delayed = delayed + 5;
      // set the brightness of pin 9:
      analogWrite(0, brightness);
      // change the brightness for next time through the loop:
      brightness = brightness + fadeAmount;

      // reverse the direction of the fading at the ends of the fade:
      if (brightness <= 150 || brightness >= 255) {
        fadeAmount = -fadeAmount;
      }
      // wait for 30 milliseconds to see the dimming effect
      delay(30);                  // wait for a second
      break;
     case 3:
      delayed = delayed + 6000;
      analogWrite(0, 225);   // turn the LED on (HIGH is the voltage level)
      delay(3000);
      break;
     case 4:
      delayed = delayed + 500;
      digitalWrite(0, HIGH);   // turn the LED on (HIGH is the voltage level)
      delay(250);               // wait for a second
      analogWrite(0, 225);    // turn the LED off by making the voltage LOW
      delay(250);               // wait for a second
      break;
     case 5:
      delayed = delayed + 5;
      // set the brightness of pin 9:
      analogWrite(0, brightness);
      // change the brightness for next time through the loop:
      brightness = brightness + fadeAmount;

      // reverse the direction of the fading at the ends of the fade:
      if (brightness <= 150 || brightness >= 255) {
        fadeAmount = -fadeAmount;
      }
      // wait for 30 milliseconds to see the dimming effect
      delay(75);                  // wait for a second
      break;
  }
}
