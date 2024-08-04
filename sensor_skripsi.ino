#include <Fuzzy.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

// Kredensial WiFi
const char* ssid = "rednot8pro"; //sesuaikan dengan ssid yang di gunakan
const char* password = "12345678"; //sesuaikan dengan password dengan ssid

// Detail server
const char* host = "192.168.97.112"; // Alamat IP atau nama domain server
const int httpPort = 8081; // Port untuk komunikasi HTTP
const String endpoint = "/Monitoring/kirimdata.php"; // Endpoint untuk mengirim data

// Pin sensor ultrasonik
const int trigPin = D5;
const int echoPin = D6;

// Pin sensor kelembaban tanah
const int SOIL_MOISTURE_PIN = A0;

// Pin solenoid
#define SOLENOID_PIN D7  //D1

// Nilai kalibrasi untuk sensor kelembaban tanah
const int dryValue = 656;
const int wetValue = 270;

// Objek logika fuzzy
Fuzzy* fuzzy = new Fuzzy();
FuzzyInput* soilMoistureInput;
FuzzyOutput* waterPump;
FuzzyInput* waterLevel;
FuzzySet* kering;
FuzzySet* lembab;
FuzzySet* basah;
FuzzySet* rendah;
FuzzySet* sedang;
FuzzySet* tinggi;
FuzzySet* slow;
FuzzySet* moderate;
FuzzySet* fast;

long duration; // Variabel untuk pengukuran ultrasonik
int air; // Variabel untuk jarak
int tanah; // Variabel untuk pembacaan kelembaban tanah

WiFiClient client;
HTTPClient http;

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(SOLENOID_PIN, OUTPUT);
  
  // Menghubungkan ke jaringan WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung");

  // Inisialisasi logika fuzzy
  initFuzzyLogic();
}

void loop() {
  Serial.println("");
  int air = ultrasonic();
  int tanah = readSoilMoisture();

  // Mengatur input untuk logika fuzzy
  fuzzy->setInput(1, tanah);
  fuzzy->setInput(2, air);

  // Melakukan fuzzifikasi input
  fuzzy->fuzzify();

  // Mendapatkan output dari logika fuzzy
  float valveControl = fuzzy->defuzzify(1);

  // Definisikan toleransi
  float tolerance = 0.01;

  // Mengontrol selenoid valve berdasarkan output fuzzy
  controlSolenoid(valveControl);

  // Kirim data sensor
  sendSensorData(air, tanah, valveControl);

  Serial.println("==========");

  delay(5000); // Sesuaikan delay sesuai kebutuhan
}

void initFuzzyLogic() {
  // Inisialisasi input dan output fuzzy
  soilMoistureInput = new FuzzyInput(1);
  waterLevel = new FuzzyInput(2);
  waterPump = new FuzzyOutput(1);

  // Set fuzzy untuk kelembaban tanah
  kering = new FuzzySet(0, 0, 20, 40);
  lembab = new FuzzySet(30, 50, 50, 70);
  basah = new FuzzySet(60, 80, 100, 100);
  soilMoistureInput->addFuzzySet(kering);
  soilMoistureInput->addFuzzySet(lembab);
  soilMoistureInput->addFuzzySet(basah);
  fuzzy->addFuzzyInput(soilMoistureInput);

  // Set fuzzy untuk level air
  rendah = new FuzzySet(0, 0, 15, 30);
  sedang = new FuzzySet(20, 35, 35, 50);
  tinggi = new FuzzySet(40, 55, 70, 70);
  waterLevel->addFuzzySet(rendah);
  waterLevel->addFuzzySet(sedang);
  waterLevel->addFuzzySet(tinggi);
  fuzzy->addFuzzyInput(waterLevel);

  // Set fuzzy untuk kontrol pompa air
  slow = new FuzzySet(0, 0, 20, 40);
  moderate = new FuzzySet(30, 50, 50, 70);
  fast = new FuzzySet(60, 80, 100, 100);
  waterPump->addFuzzySet(slow);
  waterPump->addFuzzySet(moderate);
  waterPump->addFuzzySet(fast);
  fuzzy->addFuzzyOutput(waterPump);

  // Menambahkan aturan fuzzy
  addFuzzyRules();
}

void addFuzzyRules() {
  // Aturan fuzzy
  addFuzzyRule(1, kering, tinggi, fast);
  addFuzzyRule(2, kering, sedang, fast);
  addFuzzyRule(3, kering, rendah, moderate);
  addFuzzyRule(4, lembab, tinggi, fast);
  addFuzzyRule(5, lembab, sedang, moderate);
  addFuzzyRule(6, lembab, rendah, slow);
  addFuzzyRule(7, basah, tinggi, moderate);
  addFuzzyRule(8, basah, sedang, slow);
  addFuzzyRule(9, basah, rendah, slow);
}

void addFuzzyRule(int ruleNumber, FuzzySet* moisture, FuzzySet* level, FuzzySet* pump) {
  FuzzyRuleAntecedent* antecedent = new FuzzyRuleAntecedent();
  antecedent->joinWithAND(moisture, level);
  FuzzyRuleConsequent* consequent = new FuzzyRuleConsequent();
  consequent->addOutput(pump);
  fuzzy->addFuzzyRule(new FuzzyRule(ruleNumber, antecedent, consequent));
}

void controlSolenoid(float valveControl) {
  if (valveControl >= 60) { // Corresponds to fast
    digitalWrite(SOLENOID_PIN, HIGH); // Buka valve
    delay(500); // Buka valve selama 0.5 detik
    Serial.println("cepat");
    digitalWrite(SOLENOID_PIN, LOW); // Tutup valve
  } else if (valveControl >= 30) { // Corresponds to moderate
    digitalWrite(SOLENOID_PIN, HIGH); // Buka valve
    delay(1000); // Buka valve selama 1 detik
    Serial.println("sedang");
    digitalWrite(SOLENOID_PIN, LOW); // Tutup valve
  } else { // Corresponds to slow
    digitalWrite(SOLENOID_PIN, HIGH); // Buka valve
    delay(2000); // Buka valve selama 2 detik
    Serial.println("lambat");
    digitalWrite(SOLENOID_PIN, LOW); // Tutup valve
  }
}

void sendSensorData(int air, int tanah, float valveControl) {
  String keterangan = getKeterangan(valveControl);
  String url = "http://" + String(host) + ":" + String(httpPort) + String(endpoint);
  String postData = "air=" + String(air) + "&tanah=" + String(tanah) + "&Keterangan=" + keterangan;

  Serial.print("Menghubungkan ke URL: ");
  Serial.println(url);

  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpResponseCode = http.POST(postData);
    Serial.print("Kode Respon HTTP: ");
    Serial.println(httpResponseCode);
    http.end();
  } else {
    Serial.println("Tidak dapat terhubung ke server");
  }
}

String getKeterangan(float valveControl) {
  if (valveControl >= 60) { // Corresponds to fast
    return "cepat";
  } else if (valveControl >= 30) { // Corresponds to moderate
    return "sedang";
  } else { // Corresponds to slow
    return "lambat";
  }
}

int ultrasonic() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2); 
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10); 
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  air = (duration / 2) / 29.1;
  
  //mengambil data raw berupa centimeter

  // Kalibrasi jarak ke persentase
  float percentage = map(air, 1, 37, 100, 1); // Convert to percentage

  Serial.print("Persentase: ");
  Serial.print(percentage);
  Serial.print("% - ");
  
  // Menentukan kondisi berdasarkan jarak
  if (percentage >= 70) {
    Serial.println("Tinggi");
  } else if (percentage >= 40) {
    Serial.println("Sedang");
  } else if (percentage >= 10) {
    Serial.println("Rendah");
  } else {
    Serial.println("error");
  }
  
  return percentage; // Return the percentage value
}

int readSoilMoisture() {
  int tanah = analogRead(SOIL_MOISTURE_PIN);
  
  tanah = map(tanah, dryValue, wetValue, 1, 100); // Map dryValue to 1% and wetValue to 100%
  //mengmbil data raw
  Serial.print("Kelembaban Tanah: ");
  Serial.print(tanah);
  Serial.println("%");
  return tanah;
}