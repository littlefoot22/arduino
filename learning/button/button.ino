//Constants
const int BUTTON_PIN = 1;     
const int LED_PIN =  0;     

//Variables
int buttonState = 0;
int flag=0;



// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

// the loop function runs over and over again forever
void loop(){
  //Read button state (pressed or not pressed?)
  buttonState = digitalRead(BUTTON_PIN);

  //If button pressed...
  if (buttonState == LOW) { 
    //...ones, turn led on!
    if ( flag == 0){
      digitalWrite(LED_PIN, HIGH);
      flag=1; //change flag variable
    }
    //...twice, turn led off!
    else if ( flag == 1){
      digitalWrite(LED_PIN, LOW);
      flag=0; //change flag variable again 
    }    
  }
  delay(200); //Small delay

}



