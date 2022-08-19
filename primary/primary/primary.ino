#include <SoftwareSerial.h>

SoftwareSerial BTSerial(10, 11); // RX | TX


boolean master = false;
const int buttonPin = 2;     // the number of the pushbutton pin

int buttonState = 0;         // variable for reading the pushbutton status

void setup()
{
  pinMode(buttonPin, INPUT);
  pinMode(9, OUTPUT);  // this pin will pull the HC-05 pin 34 (key pin) HIGH to switch module to AT mode
  digitalWrite(9, HIGH);
  Serial.begin(9600);
  Serial.println("Enter AT commands:");
  BTSerial.begin(38400);  // HC-05 default speed in AT command more
}

void loop()
{
//  // Keep reading from HC-05 and send to Arduino Serial Monitor
//  if (BTSerial.available())
//    Serial.write(BTSerial.read());
//
//  // Keep reading from Arduino Serial Monitor and send to HC-05
//  if (Serial.available())
      //BTSerial.write(Serial.read());
  
  buttonState = digitalRead(buttonPin);
  delay(500); // wait for change
  if (buttonState == HIGH) {
    // turn LED on:
    //Serial.println("HIGH");
  } else {
    // turn LED off:
   // Serial.println("LOW");
  }  

  if (!master) { 
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
    master = true;
  }


}
