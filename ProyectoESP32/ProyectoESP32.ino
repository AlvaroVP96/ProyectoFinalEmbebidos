#include <ESP32Servo.h>
#include <DHT11.h>
#include <WiFi.h>
#include "esp_sleep.h"
#include "driver/touch_pad.h"
#include <PubSubClient.h>


// === Prototipos ===
void ledVerde();
void ledBlanco();
void ledAzul();
void ledRojo();
void ledApagado();
void leerSensor();
void EstadoPuerta1();
void EstadoPuerta2();
void controlPuertas();
void beep(int frecuencia, int duracion);
void goToSleep();

// === WIFI ===
const char* ssid = "OnePlus3";
const char* password = "pruebaESP32";
WiFiServer server(80);

// === DHT11 ===
#define DHT_PIN 5
DHT11 dht(DHT_PIN);
int temperatura;
int humedad;

// === Timer no bloqueante ===
unsigned long t0 = 0;
unsigned long intervalo = 2000;

unsigned long tSensor = 0;
unsigned long IntervaloSensor = 10000;

// === RGB ===
const int PIN_R = 27, PIN_G = 26, PIN_B = 25;
String estadoLED = "apagado";

// === Servos / puertas ===
Servo puerta1;
Servo puerta2;
int servo1 = 17;
int servo2 = 16;
const int ANG_CERR = 0;
const int ANG_ABR  = 90;
volatile bool puertaCerrada1 = true;
volatile bool puertaCerrada2 = true;
volatile bool abrirPuerta1 = false;
volatile bool abrirPuerta2 = false;
volatile bool error = false;

// === Botones / final de carrera ===
const byte boton1 = 13;
const byte boton2 = 14;
unsigned long debounce = 1000;
volatile unsigned long ULT_ISR1 = 0;
volatile unsigned long ULT_ISR2 = 0;

// === Buzzer ===
int buzzer = 23;

//=== MQTT ===
String mqtt_server = "10.180.135.146";
const int mqtt_port = 1883;

const char* TOPIC_PUERTA_EXTERIOR = "Sensores/Puertas/PuertaExterior";
const char* TOPIC_PUERTA_INTERIOR = "Sensores/Puertas/PuertaInterior";
const char* TOPIC_PUERTA_LED = "Sensores/LED";
const char* TOPIC_PUERTA_TEMPERATURA = "Sensores/temperatura";
const char* TOPIC_PUERTA_HUMEDAD = "Sensores/humedad";

// === Touchpad (despertar) ===
// T0 = GPIO 4 (tocar para despertar del deep sleep)
#define TOUCH_CH T0
#define TOUCH_THRESHOLD 200

// Clientes WiFi y MQTT
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  while(!Serial){;}

  // WiFi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected.");
  Serial.println(WiFi.localIP());
  
  
  // === MQTT ===
  mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
  Serial.println("\n--- Conectando a MQTT ---");
  reconnectMQTT();

  // LED RGB
  pinMode(PIN_R,OUTPUT);
  pinMode(PIN_G,OUTPUT);
  pinMode(PIN_B,OUTPUT);

  // Buzzer
  pinMode(buzzer,OUTPUT);

  // Servos
  puerta1.attach(servo1);
  puerta2.attach(servo2);
  puertaCerrada1 = true;
  puertaCerrada2 = true;
  puerta1.write(ANG_CERR);
  puerta2.write(ANG_CERR);

  // Botones / finales
  pinMode(boton1,INPUT_PULLUP);
  pinMode(boton2,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(boton1),EstadoPuerta1,FALLING);
  attachInterrupt(digitalPinToInterrupt(boton2),EstadoPuerta2,FALLING);
}


void loop() {
  if(WiFi.status() != WL_CONNECTED){
    Serial.print("Conexión WIFI perdida, revise la conexión");
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi reconectado.");
  }
    // Mantener conexi�n MQTT
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  unsigned long t_actual = millis();
  if(t_actual - t0 > intervalo){
    leerSensor();
    controlPuertas(); 
    t0 = t_actual;
  }
  
}

void controlPuertas() {
  if ((abrirPuerta1 && puertaCerrada1) || error) {
    if (puertaCerrada2) {
      puerta1.write(ANG_ABR);
    } else {
      beep(1000, 250);
      error = false;
    }
  }
  else if (!abrirPuerta1 && !puertaCerrada1) {
    puerta1.write(ANG_CERR);
  }

  if ((abrirPuerta2 && puertaCerrada2) || error) {
    if (puertaCerrada1) {
      puerta2.write(ANG_ABR);
    } else {
      beep(1000, 250);
      error = false;
    }
  }
  else if (!abrirPuerta2 && !puertaCerrada2) {
    puerta2.write(ANG_CERR);
  }

  if (puertaCerrada1 && puertaCerrada2) {
    ledVerde(); noTone(buzzer);
  } else if (!puertaCerrada1 && !puertaCerrada2) {
    tone(buzzer,2500); ledRojo();
  } else {
    ledAzul(); noTone(buzzer);
  }
}

void ledVerde(){  analogWrite(PIN_R,0);   analogWrite(PIN_G,255); analogWrite(PIN_B,0);   estadoLED = "Verde"; }
void ledRojo(){   analogWrite(PIN_R,255); analogWrite(PIN_G,0);   analogWrite(PIN_B,0);   estadoLED = "Rojo"; }
void ledApagado(){analogWrite(PIN_R,0);   analogWrite(PIN_G,0);   analogWrite(PIN_B,0);   estadoLED = "Apagado"; }
void ledBlanco(){ analogWrite(PIN_R,255); analogWrite(PIN_G,255); analogWrite(PIN_B,255); estadoLED = "Blanco"; }
void ledAzul(){   analogWrite(PIN_R,0);   analogWrite(PIN_G,0);   analogWrite(PIN_B,255); estadoLED = "Azul"; }

// ---------------- Sensores ----------------
void leerSensor(){  
  temperatura = dht.readTemperature();
  humedad = dht.readHumidity();

  unsigned long tSensorActual = millis();
  if(tSensorActual - tSensor > IntervaloSensor){
    char temp[8];
    itoa(temperatura,temp,10);
    mqttClient.publish(TOPIC_PUERTA_TEMPERATURA,temp);
    char hum[8];
    itoa(humedad,hum,10);
    mqttClient.publish(TOPIC_PUERTA_HUMEDAD,hum);

    tSensor = tSensorActual;
  }
  

}

void EstadoPuerta1(){
  unsigned long t = millis();
  if(t - ULT_ISR1 > debounce){
    puertaCerrada1 = !puertaCerrada1; 
    ULT_ISR1 = t;
  }

}
void EstadoPuerta2(){
  unsigned long t = millis();
  if(t - ULT_ISR2 > debounce){
    puertaCerrada2 = !puertaCerrada2;
    ULT_ISR2 = t;
  }
}

void beep(int frecuencia, int duracion) {
  tone(buzzer, frecuencia);
  delayMicroseconds(duracion * 1000);
  noTone(buzzer);
}

void goToSleep(){
  // Estado seguro
  noTone(buzzer);
  ledApagado();
  puerta1.write(ANG_CERR);
  puerta2.write(ANG_CERR);
  delay(150);
  // Quitar interrupciones de botones para evitar ruido al dormir
  detachInterrupt(digitalPinToInterrupt(boton1));
  detachInterrupt(digitalPinToInterrupt(boton2));

  // Apagar WiFi para bajar consumo
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // Configurar touchpad
  int THRESHOLD = touchRead(TOUCH_CH) - TOUCH_THRESHOLD;
  touchAttachInterrupt(TOUCH_CH, nullptr, THRESHOLD);
  esp_sleep_enable_touchpad_wakeup();

  Serial.println("Deep sleep activado. Toca el pin táctil GPIO 4 (T0) para despertar.");
  delay(50);
  esp_deep_sleep_start();
}

// Conectar/Reconectar al broker MQTT
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando a MQTT (");
    Serial.print(mqtt_server);
    Serial.print(")...");
    
    String clientId = "ESP32Sensores-";
    clientId += String(random(0xffff), HEX);
    
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("Conectado!");
      Serial.println("Publicando en topics:");
      Serial.println("  - " + String(TOPIC_PUERTA_EXTERIOR));
      Serial.println("  - " + String(TOPIC_PUERTA_INTERIOR));
      Serial.println("  - " + String(TOPIC_PUERTA_LED));
      Serial.println("  - " + String(TOPIC_PUERTA_TEMPERATURA));
      Serial.println("  - " + String(TOPIC_PUERTA_HUMEDAD));
    } else {
      Serial.print("Fallo, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" reintentando en 5s");
      delay(5000);
    }
  }
}



