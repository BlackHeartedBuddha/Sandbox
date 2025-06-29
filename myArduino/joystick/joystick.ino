#define VRX_PIN A1
#define VRY_PIN A0
#define SW_PIN  2

int xValue = 0;
int yValue = 0;
int bValue = 0;

void setup() {
  Serial.begin(9600);
  pinMode(SW_PIN, INPUT_PULLUP); // use internal pull-up resistor
}

void loop() {
  xValue = analogRead(VRX_PIN);
  yValue = analogRead(VRY_PIN);
  bValue = digitalRead(SW_PIN); // LOW = pressed, HIGH = released

  if (bValue == LOW) {
    Serial.println("Button Pressed");
    // You can add delay(50) for basic debouncing
  }

  Serial.print("x = ");
  Serial.print(xValue);
  Serial.print(", y = ");
  Serial.print(yValue);
  Serial.print(" : button = ");
  Serial.println(bValue);

  delay(100); // avoid flooding the serial output
}
