const int photoResistorPin = A0; // Analog pin the photoresistor is connected to

void setup() {
  Serial.begin(9600); // Initialize serial communication
}

void loop() {
  // Read the analog value from the photoresistor
  int lightValue = analogRead(photoResistorPin);

  // Print the raw analog value to the serial monitor
  Serial.print("Raw Light Value: ");
  Serial.println(lightValue);

  delay(500); // Delay for readability in serial monitor
}
