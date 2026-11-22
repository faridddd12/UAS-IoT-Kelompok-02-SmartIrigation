#include <DHT.h>
#include <Servo.h>
#include <SoftwareSerial.h>

#define DHTPIN 4
#define DHTTYPE DHT11
const int soilPin  = A0;
const int ldrPin   = A1;
const int relayPin = 7;
const int servoPin = 9;

// Urutan Pin 7-Segment 5161BS: {A, B, C, D, E, F, G}
const int segPins[7] = {3, 2, 12, 13, 8, 6, 5};

const byte ANGKA_0[7] = {LOW, LOW, LOW, LOW, LOW, LOW, HIGH};    // 0: Pompa OFF
const byte ANGKA_1[7] = {HIGH, LOW, LOW, HIGH, HIGH, HIGH, HIGH}; // 1: Pompa ON
const byte HURUF_E[7] = {LOW, HIGH, HIGH, LOW, LOW, LOW, LOW};    // E: Error Sensor

SoftwareSerial espSerial(10, 11);
DHT dht(DHTPIN, DHTTYPE);
Servo nozzleServo;

// Histeresis Kelembapan Tanah (%)
const int BATAS_SIRAM_ON  = 40;
const int BATAS_SIRAM_OFF = 55;

// Moving Average (Rata-rata bergerak 5 sampel)
const int JML_SAMPEL = 5;
int bufferTanah[JML_SAMPEL];
int indeksSampel = 0;

int modeKontrol = 0; // 0 = AUTO, 1 = PAKSA ON, 2 = PAKSA OFF
bool sedangMenyiram = false;

int sudutServo  = 90;
int arahServo   = 5;
bool servoAktif = false;
unsigned long lastServoMove = 0;

float suhuTerakhir = 30.0;
float humTerakhir  = 70.0;

unsigned long lastDhtRead = 0;
unsigned long lastSend    = 0;

void tampil7Seg(const byte pola[7]) {
  for (int i = 0; i < 7; i++) {
    digitalWrite(segPins[i], pola[i]);
  }
}

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  dht.begin();

  for (int i = 0; i < 7; i++) {
    pinMode(segPins[i], OUTPUT);
    digitalWrite(segPins[i], HIGH);
  }

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);

  int awalSoil = analogRead(soilPin);
  for (int i = 0; i < JML_SAMPEL; i++) {
    bufferTanah[i] = awalSoil;
  }

  nozzleServo.attach(servoPin);
  nozzleServo.write(90);
  delay(400);
  nozzleServo.detach();
}

void loop() {
  if (espSerial.available()) {
    String cmd = espSerial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "PUMP_ON") modeKontrol = 1;
    else if (cmd == "PUMP_OFF") modeKontrol = 2;
    else if (cmd == "PUMP_AUTO") modeKontrol = 0;
  }

  if (millis() - lastDhtRead >= 2000) {
    lastDhtRead = millis();
    float s = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(s) && !isnan(h)) {
      suhuTerakhir = s;
      humTerakhir  = h;
    }
  }

  int soilRaw = analogRead(soilPin);
  int ldrRaw  = analogRead(ldrPin);

  bool sensorError = (soilRaw <= 5);

  if (!sensorError) {
    bufferTanah[indeksSampel] = soilRaw;
    indeksSampel = (indeksSampel + 1) % JML_SAMPEL;
  }

  long totalSoil = 0;
  for (int i = 0; i < JML_SAMPEL; i++) {
    totalSoil += bufferTanah[i];
  }
  int soilRataRata = totalSoil / JML_SAMPEL;
  int tanahPersen  = constrain(map(soilRataRata, 1020, 300, 0, 100), 0, 100);

  if (sensorError) {
    sedangMenyiram = false;
    digitalWrite(relayPin, HIGH);
    tampil7Seg(HURUF_E);
  } else {
    if (modeKontrol == 1) {
      sedangMenyiram = true;
    } else if (modeKontrol == 2) {
      sedangMenyiram = false;
    } else {
      if (!sedangMenyiram && tanahPersen < BATAS_SIRAM_ON) {
        sedangMenyiram = true;
      } else if (sedangMenyiram && tanahPersen >= BATAS_SIRAM_OFF) {
        sedangMenyiram = false;
      }
    }

    if (sedangMenyiram) {
      digitalWrite(relayPin, LOW);
      tampil7Seg(ANGKA_1);
    } else {
      digitalWrite(relayPin, HIGH);
      tampil7Seg(ANGKA_0);
    }
  }

  if (sedangMenyiram) {
    if (!servoAktif) {
      nozzleServo.attach(servoPin);
      servoAktif = true;
    }
    if (millis() - lastServoMove >= 60) {
      lastServoMove = millis();
      sudutServo += arahServo;
      if (sudutServo >= 150 || sudutServo <= 30) {
        arahServo = -arahServo;
      }
      nozzleServo.write(sudutServo);
    }
  } else {
    if (servoAktif) {
      sudutServo = 90;
      nozzleServo.write(90);
      delay(300);
      nozzleServo.detach();
      servoAktif = false;
    }
  }

  if (millis() - lastSend >= 2000) {
    lastSend = millis();
    String json = "{\"id\":\"smartirrigate-01\",\"tanah\":" + String(tanahPersen) +
                  ",\"suhu\":" + String(suhuTerakhir, 1) +
                  ",\"udara\":" + String(humTerakhir, 1) +
                  ",\"cahaya\":" + String(ldrRaw) +
                  ",\"pompa\":" + String(sedangMenyiram ? 1 : 0) +
                  ",\"mode\":" + String(modeKontrol) +
                  ",\"error\":" + String(sensorError ? 1 : 0) + "}";
    espSerial.println(json);
    Serial.println(json);
  }
}