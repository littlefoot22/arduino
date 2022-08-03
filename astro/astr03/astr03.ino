/*
  Astro

*/
int fadeAmount = 5;    // how many points to fade the LED by
int brightness = 175;    // how bright the LED is
int delayed = 0;
int CHILL_MODE = 0;

void setup() {
  
  randomSeed(analogRead(1));
  CHILL_MODE = random(2);
  
  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);

  
  if (CHILL_MODE > 0) {
    CHILL_MODE = 1;
  } else {
    CHILL_MODE = 0;
  }
}


void chill_mode() { 
   delayed = delayed + 5;
   // set the brightness of pin 9:
   analogWrite(0, brightness);
   analogWrite(1, brightness);
   analogWrite(2, brightness);
   analogWrite(3, brightness);
   // change the brightness for next time through the loop:
   brightness = brightness + fadeAmount;

   // reverse the direction of the fading at the ends of the fade:
   if (brightness <= 175 || brightness >= 255) {
     fadeAmount = -fadeAmount;
   }
   // wait for 30 milliseconds to see the dimming effect
   delay(75);                  // wait for a second
}

void blast_off() { 
 
  for( int i = 0; i < 10; i++) {
    digitalWrite(0, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(50);               // wait for a second
    digitalWrite(0, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(50); 
  }
  digitalWrite(0, LOW);   // turn the LED on (HIGH is the voltage level)


  for( int i = 0; i < 10; i++) {
    digitalWrite(1, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(50);               // wait for a second
    digitalWrite(1, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(50); 
  }
  digitalWrite(1, LOW);   // turn the LED on (HIGH is the voltage level)


  for( int i = 0; i < 10; i++) {
    digitalWrite(2, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(50);               // wait for a second
    digitalWrite(2, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(50); 
  }
  digitalWrite(2, LOW);   // turn the LED on (HIGH is the voltage level)


  for( int i = 0; i < 10; i++) {
    digitalWrite(3, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(50);               // wait for a second
    digitalWrite(3, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(50); 
  }
  digitalWrite(3, LOW);   // turn the LED on (HIGH is the voltage level)


  for( int i = 0; i < 10; i++) {
    digitalWrite(0, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(100);  
    digitalWrite(1, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(100);               // wait for a second
    digitalWrite(2, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(100);               // wait for a second
    digitalWrite(3, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(100);               // wait for a second
    digitalWrite(0, HIGH);   // turn the LED on (HIGH is the voltage level)
    digitalWrite(1, HIGH);   // turn the LED on (HIGH is the voltage level)
    digitalWrite(2, HIGH);   // turn the LED on (HIGH is the voltage level)
    digitalWrite(3, HIGH);   // turn the LED on (HIGH is the voltage level)
  }

  digitalWrite(0, HIGH);   // turn the LED on (HIGH is the voltage level)
  digitalWrite(1, HIGH);   // turn the LED on (HIGH is the voltage level)
  digitalWrite(2, HIGH);   // turn the LED on (HIGH is the voltage level)
  digitalWrite(3, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(500);               // wait for a second
 
}




// the loop function runs over and over again forever
void loop() {
 switch (CHILL_MODE) {
     case 0:
       blast_off();
       break;
     case 1:
      chill_mode();
      break;

 } 

}
