#define BLYNK_TEMPLATE_ID "TMPL2xkNW7yUE"
#define BLYNK_TEMPLATE_NAME "LeNer"
#define BLYNK_AUTH_TOKEN "OIMZFQFJfxVGYXzFLvQ9YXAbwr-qFa6t"

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

#define EMG_PIN 34  // Pino de leitura do sinal EMG
#define FILTER_ORDER 10

char ssid[] = "TP-LinkVZ";
char pass[] = "57624532";
char auth[] = BLYNK_AUTH_TOKEN;

// Variáveis globais para aquisição de dados EMG
volatile unsigned int emgData[FILTER_ORDER];  // Buffer circular para os dados EMG
volatile int emgIndex = 0;  // Índice de escrita no buffer EMG
volatile bool newDataAvailable = false;  // Flag para indicar novos dados disponíveis

// Variáveis globais para filtragem
float filterCoeff[FILTER_ORDER + 1] = {0.0149, 0.0627, 0.1541, 0.2224, 0.2224, 0.1541, 0.0627, 0.0149, -0.0056, -0.0143, -0.0152};
float filteredValue = 0.0;

// Variáveis para controle de tempo e threshold
unsigned long startTime = 0;
volatile int threshold = 1000;  // Valor inicial do threshold 

// Thread de aquisição e filtragem de dados EMG
void emgThread(void *param) {
  (void)param;
  while (true) {
    if (newDataAvailable) {
      noInterrupts();
      newDataAvailable = false;
      interrupts();
      
      filterEMG();
    }
    vTaskDelay(1); // Aguarda 1ms (taxa de amostragem de 1ms)
  }
}

// Thread para enviar dados para o Blynk
void blynkThread(void *param) {
  (void)param;
  while (true) {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - startTime;
    
    Blynk.virtualWrite(V1, elapsedTime / 1000); // Envie o tempo decorrido para o pino V1 no Blynk
    Blynk.virtualWrite(V2, filteredValue); // Envie os dados filtrados para o pino V2 no Blynk
    Blynk.virtualWrite(V3, threshold); // Envie o valor do threshold para o pino V3 no Blynk
    
    vTaskDelay(100);  // Aguarde 100ms antes de enviar novamente
  }
}

// Função de interrupção para captura de dados EMG
void IRAM_ATTR emgInterrupt() {
  int emgValue = analogRead(EMG_PIN);
  
  if (emgValue > threshold) {
    emgData[emgIndex] = emgValue;
    emgIndex = (emgIndex + 1) % FILTER_ORDER;
    newDataAvailable = true;
  }
}

// Função para filtragem dos dados EMG
void filterEMG() {
  float sum = 0.0;
  for (int i = 0; i < FILTER_ORDER; i++) {
    sum += emgData[i] * filterCoeff[FILTER_ORDER - i];
  }
  filteredValue = sum;
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  Blynk.begin(auth, ssid, pass);
  Blynk.connect();

  pinMode(EMG_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(EMG_PIN), emgInterrupt, RISING);

  startTime = millis();  // Inicializa o tempo decorrido

  xTaskCreatePinnedToCore(emgThread, "EMG Task", 10000, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(blynkThread, "Blynk Task", 10000, NULL, 1, NULL, 0);
}

void loop() {
  Blynk.run();
}
