/* main_serial.ino
 Serial IoT device for ESP32
 - Kábel (USB) kapcsolaton keresztül kommunikál a PC-vel (Serial over USB).
 - Küld newline-terminated JSON telemetriát: {"light":..., "motion":0/1, "master":0/1}
 - Fogad egyszerű parancsokat sorosról: "MASTER ON", "MASTER OFF", "GET STATUS"
 - Helyi gomb: toggle master állapot, és küld STATUS üzenetet a host felé.
*/

const int ldrPin = 34;     // ADC1_CH6
const int pirPin = 14;     // digital input
const int ledPin = 23;     // output
const int buttonPin = 22;  // local button (INPUT_PULLUP -> press = LOW)

const int samples = 8;
const int lightThreshold = 1500;
const unsigned long holdMs = 5000;

bool masterEnabled = true;
unsigned long lastMotionMillis = 0;
bool ledState = false;

// debouncing
const unsigned long debounceDelay = 50;
int lastButtonReading = HIGH;
int buttonState = HIGH;
unsigned long lastDebounceTime = 0;

int readLdrAverage() {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(ldrPin);
    delay(5);
  }
  return (int)(sum / samples);
}

void handleButtonToggle() {
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        masterEnabled = !masterEnabled;
        // send immediate status to host
        Serial.print("STATUS ");
        Serial.println(masterEnabled ? "ON" : "OFF");
      }
    }
  }
  lastButtonReading = reading;
}

void handleSerialCommands() {
  // if there's data, read complete line(s)
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    if (line.equalsIgnoreCase("MASTER ON")) {
      masterEnabled = true;
      Serial.println("ACK MASTER ON");
    } else if (line.equalsIgnoreCase("MASTER OFF")) {
      masterEnabled = false;
      Serial.println("ACK MASTER OFF");
    } else if (line.equalsIgnoreCase("GET STATUS")) {
      Serial.print("STATUS ");
      Serial.println(masterEnabled ? "ON" : "OFF");
    } else {
      Serial.print("ERR Unknown command: ");
      Serial.println(line);
    }
  }
}

unsigned long lastTelemetry = 0;
const unsigned long telemetryInterval = 2000; // ms

void setup() {
  Serial.begin(115200);
  // some boards need a while( !Serial ) {} - Wokwi és az ESP32-n általában nem
  pinMode(ldrPin, INPUT);
  pinMode(pirPin, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  digitalWrite(ledPin, LOW);
  Serial.println("Serial IoT device started");
}

void loop() {
  handleButtonToggle();
  handleSerialCommands();

  int lightValue = readLdrAverage();
  int motion = digitalRead(pirPin);
  if (motion == HIGH) lastMotionMillis = millis();
  bool dark = (lightValue < lightThreshold);

  if (!masterEnabled) {
    ledState = false;
  } else {
    if (dark && (millis() - lastMotionMillis <= holdMs)) ledState = true;
    else ledState = false;
  }
  digitalWrite(ledPin, ledState ? HIGH : LOW);

  // Telemetry every interval as newline-delimited JSON
  if (millis() - lastTelemetry > telemetryInterval) {
    lastTelemetry = millis();
    // produce a compact JSON line
    Serial.print("{\"light\":");
    Serial.print(lightValue);
    Serial.print(",\"motion\":");
    Serial.print(motion == HIGH ? 1 : 0);
    Serial.print(",\"master\":");
    Serial.print(masterEnabled ? 1 : 0);
    Serial.println("}");
  }

  delay(10);
}