/*
  Astro

*/


void setup() {
  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
}

// the loop function runs over and over again forever
void loop() {

  for( int i = 0; i < 50; i++) {
    digitalWrite(0, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(50);               // wait for a second
    digitalWrite(0, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(50); 
  }
  digitalWrite(0, LOW);   // turn the LED on (HIGH is the voltage level)


  for( int i = 0; i < 50; i++) {
    digitalWrite(1, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(50);               // wait for a second
    digitalWrite(1, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(50); 
  }
  digitalWrite(1, LOW);   // turn the LED on (HIGH is the voltage level)


  for( int i = 0; i < 50; i++) {
    digitalWrite(2, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(50);               // wait for a second
    digitalWrite(2, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(50); 
  }
  digitalWrite(2, LOW);   // turn the LED on (HIGH is the voltage level)


  for( int i = 0; i < 50; i++) {
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
  }

  digitalWrite(0, HIGH);   // turn the LED on (HIGH is the voltage level)
  digitalWrite(1, HIGH);   // turn the LED on (HIGH is the voltage level)
  digitalWrite(2, HIGH);   // turn the LED on (HIGH is the voltage level)
  digitalWrite(3, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(500);               // wait for a second

}
