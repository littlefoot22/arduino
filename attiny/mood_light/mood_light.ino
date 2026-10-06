//ATtiny85 RGB color fading Mood Light NOW WITH LIGHT SENSING CAPABILITIES!!!

const int redPin = 2; 
const int grnPin = 1; 
const int bluPin = 0;
const int sensor = 3;


void setup()
{
  pinMode(redPin, OUTPUT);    
  pinMode(grnPin, OUTPUT);    
  pinMode(bluPin, OUTPUT);
  pinMode(sensor, INPUT);
}

void loop() {
  if (analogRead(sensor) <= 200)
  {
    redtoyellow();
    yellowtogreen();
    greentocyan();
    cyantoblue();
    bluetomagenta();
    magentatored();
  }
  else if (analogRead(sensor) >= 201)
  {
    digitalWrite(redPin, LOW);
    digitalWrite(grnPin, LOW);
    digitalWrite(bluPin, LOW);
  }
}

void redtoyellow()
{
  digitalWrite(redPin, HIGH);
  digitalWrite(bluPin, LOW);

  // fade up green
  for(byte i=1; i<100; i++) {
    byte on  = i;
    byte off = 100-on;
    for( byte a=0; a<100; a++ ) {
      digitalWrite(grnPin, HIGH);
      delayMicroseconds(on);
      digitalWrite(grnPin, LOW);
      delayMicroseconds(off);
    }
  }
}



void yellowtogreen()
{
  digitalWrite(grnPin, HIGH);
  digitalWrite(bluPin, LOW);

  // fade down red
  for(byte i=1; i<100; i++) {
    byte on  = 100-i;
    byte off = i;
    for( byte a=0; a<100; a++ ) {
      digitalWrite(redPin, HIGH);
      delayMicroseconds(on);
      digitalWrite(redPin, LOW);
      delayMicroseconds(off);
    }
  }
}


void greentocyan()
{
  digitalWrite(grnPin, HIGH);
  digitalWrite(redPin, LOW);

  // fade up blue
  for(byte i=1; i<100; i++) {
    byte on  = i;
    byte off = 100-on;
    for( byte a=0; a<100; a++ ) {
      digitalWrite(bluPin, HIGH);
      delayMicroseconds(on);
      digitalWrite(bluPin, LOW);
      delayMicroseconds(off);
    }
  }
}



void cyantoblue()
{
  digitalWrite(bluPin, HIGH);
  digitalWrite(redPin, LOW);

  // fade down green
  for(byte i=1; i<100; i++) {
    byte on  = 100-i;
    byte off = i;
    for( byte a=0; a<100; a++ ) {
      digitalWrite(grnPin, HIGH);
      delayMicroseconds(on);
      digitalWrite(grnPin, LOW);
      delayMicroseconds(off);
    }
  }
}


void bluetomagenta()
{
  digitalWrite(bluPin, HIGH);
  digitalWrite(grnPin, LOW);

  // fade up red
  for(byte i=1; i<100; i++) {
    byte on  = i;
    byte off = 100-on;
    for( byte a=0; a<100; a++ ) {
      digitalWrite(redPin, HIGH);
      delayMicroseconds(on);
      digitalWrite(redPin, LOW);
      delayMicroseconds(off);
    }
  }
}



void magentatored()
{
  digitalWrite(redPin, HIGH);
  digitalWrite(grnPin, LOW);

  // fade down blue
  for(byte i=1; i<100; i++) {
    byte on  = 100-i;
    byte off = i;
    for( byte a=0; a<100; a++ ) {
      digitalWrite(bluPin, HIGH);
      delayMicroseconds(on);
      digitalWrite(bluPin, LOW);
      delayMicroseconds(off);
    }
  }
}
