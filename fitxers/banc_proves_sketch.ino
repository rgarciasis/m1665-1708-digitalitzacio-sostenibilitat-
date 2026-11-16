/*
  REBOOT · Banc de proves IoT (sessió 8)
  ESP32 simulat a Wokwi:
    - sensor de temperatura NTC  -> GPIO 34
    - bateria simulada (potenciòmetre) -> GPIO 35
    - botó per iniciar la prova  -> GPIO 4
    - semàfor LED: verd 25, groc 26, vermell 27
  Cada prova fa 5 lectures (una per segon) i acaba amb una línia JSON
  al monitor sèrie. Aquesta línia és la que es copia a l'app.
  Per canviar l'equip que es prova, escriviu el seu ID al monitor sèrie
  (per exemple T-02) i premeu Retorn.
  Convenció: identificadors i claus JSON en anglès; textos per a l'usuari en català.
*/

const int PIN_TEMP = 34;
const int PIN_BATTERY = 35;
const int PIN_BUTTON = 4;
const int PIN_GREEN = 25;
const int PIN_YELLOW = 26;
const int PIN_RED = 27;

const char *BENCH_ID = "REBOOT-BP1";
const float BETA = 3950;            // coeficient beta del sensor NTC de Wokwi
const float TEMP_MAX_C = 60.0;      // per sobre: l'equip va a diagnosi
const int BATTERY_MIN_PCT = 80;     // per sota: l'equip va a diagnosi
const int SAMPLES = 5;
const unsigned long SAMPLE_MS = 1000;

String deviceId = "T-01";
int testNumber = 0;
String inputLine = "";

void setLights(bool green, bool yellow, bool red) {
  digitalWrite(PIN_GREEN, green ? HIGH : LOW);
  digitalWrite(PIN_YELLOW, yellow ? HIGH : LOW);
  digitalWrite(PIN_RED, red ? HIGH : LOW);
}

float readTemperatureC() {
  int raw = analogRead(PIN_TEMP);
  raw = constrain(raw, 1, 4094);  // evita divisions per zero als extrems
  float celsius = 1 / (log(1 / (4095.0 / raw - 1)) / BETA + 1.0 / 298.15) - 273.15;
  return round(celsius * 10) / 10.0;  // un decimal
}

int readBatteryPct() {
  int raw = analogRead(PIN_BATTERY);
  return map(raw, 0, 4095, 0, 100);
}

bool buttonPressed() {
  static int lastState = HIGH;
  int state = digitalRead(PIN_BUTTON);
  bool pressed = (lastState == HIGH && state == LOW);
  lastState = state;
  if (pressed) delay(50);  // antirebot senzill
  return pressed;
}

void readSerialInput() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      inputLine.trim();
      inputLine.toUpperCase();
      if (inputLine.length() > 0 && inputLine.length() <= 12) {
        deviceId = inputLine;
        Serial.println("Equip seleccionat: " + deviceId + ". Premeu el botó per començar la prova.");
      } else if (inputLine.length() > 12) {
        Serial.println("ID massa llarg (màxim 12 caràcters).");
      }
      inputLine = "";
    } else {
      inputLine += c;
    }
  }
}

void runTest() {
  testNumber++;
  float temps[SAMPLES];
  long batterySum = 0;
  float maxTemp = -100;

  Serial.println();
  Serial.println("Prova " + String(testNumber) + " de l'equip " + deviceId + ": en curs...");

  for (int i = 0; i < SAMPLES; i++) {
    setLights(false, true, false);   // groc: en prova
    delay(SAMPLE_MS / 2);
    setLights(false, false, false);  // parpelleig
    delay(SAMPLE_MS / 2);

    temps[i] = readTemperatureC();
    int battery = readBatteryPct();
    batterySum += battery;
    if (temps[i] > maxTemp) maxTemp = temps[i];
    Serial.println("  Lectura " + String(i + 1) + "/" + String(SAMPLES) + ": " +
                   String(temps[i], 1) + " °C, bateria " + String(battery) + " %");
  }

  int batteryPct = round((float)batterySum / SAMPLES);
  bool tooHot = maxTemp > TEMP_MAX_C;
  bool lowBattery = batteryPct < BATTERY_MIN_PCT;
  bool ok = !tooHot && !lowBattery;
  setLights(ok, false, !ok);

  // Resum en una sola línia JSON
  String json = "{\"bench\":\"" + String(BENCH_ID) + "\"";
  json += ",\"test\":" + String(testNumber);
  json += ",\"deviceId\":\"" + deviceId + "\"";
  json += ",\"tempC\":[";
  for (int i = 0; i < SAMPLES; i++) {
    json += String(temps[i], 1);
    if (i < SAMPLES - 1) json += ",";
  }
  json += "],\"maxTempC\":" + String(maxTemp, 1);
  json += ",\"batteryPct\":" + String(batteryPct);
  json += ",\"result\":\"" + String(ok ? "Apte" : "Diagnosi") + "\"";
  json += ",\"reasons\":[";
  if (tooHot) json += "\"Temperatura alta\"";
  if (tooHot && lowBattery) json += ",";
  if (lowBattery) json += "\"Bateria baixa\"";
  json += "]}";

  Serial.println("Resultat: " + String(ok ? "APTE" : "DIAGNOSI") + ". Copieu la línia JSON següent:");
  Serial.println(json);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_YELLOW, OUTPUT);
  pinMode(PIN_RED, OUTPUT);
  analogReadResolution(12);
  setLights(false, false, false);

  Serial.println("REBOOT · Banc de proves " + String(BENCH_ID) + " a punt.");
  Serial.println("Llindars: temperatura màxima " + String(TEMP_MAX_C, 0) + " °C, bateria mínima " +
                 String(BATTERY_MIN_PCT) + " %.");
  Serial.println("Equip seleccionat: " + deviceId + ". Escriviu un altre ID per canviar-lo o premeu el botó.");
}

void loop() {
  readSerialInput();
  if (buttonPressed()) {
    runTest();
  }
  delay(10);
}
