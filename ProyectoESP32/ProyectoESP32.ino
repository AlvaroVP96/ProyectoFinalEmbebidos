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
void manejarControl();
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
String mqtt_server = "10.180.135.245";
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


// ---------------- HTML ----------------
String htmlTest() {
  String html = R"HTML(
<!DOCTYPE html>
<html lang=\"es\">
<head>
  <meta charset=\"utf-8\">
  <meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">
  <title>Mando Garaje</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 0; background: #111; color: #fff; display: flex; align-items: center; justify-content: center; height: 100vh; }
    .wrap { text-align: center; }
    .btn { display: block; width: 240px; padding: 18px 24px; margin: 12px auto; border: none; border-radius: 12px; font-size: 20px; font-weight: 700; cursor: pointer; transition: transform .05s ease, opacity .2s ease; }
    .btn:active { transform: scale(0.98); }
    .open { background: #2ecc71; color: #0b2e1b; }
    .close { background: #e74c3c; color: #2b0d0b; }
  </style>
</head>
<body>
    <!-- Contenedor de puertas -->
    <div class="puertas-container">
      <!-- Puerta 1 -->
      <div class="botones">
      <h2>Puerta Exterior</h2>
        <button class="abrir" onclick="controlPuerta(1, 'abrir')">Abrir</button>
        <button class="cerrar" onclick="controlPuerta(1, 'cerrar')">Cerrar</button>
      </div>
      <!-- Puerta 2 -->
      <div class="botones">
      <h2>Puerta Interior</h2>
        <button class="abrir" onclick="controlPuerta(2, 'abrir')">Abrir</button>
        <button class="cerrar" onclick="controlPuerta(2, 'cerrar')">Cerrar</button>
      </div>
      
    </div>

  <script>

        function controlPuerta(numero, accion) {
      fetch('/control?puerta=' + numero + '&accion=' + accion)
        .then(r => r.text())
        .then(_ => actualizarEstados())
        .catch(console.error);
    }
  </script>

</body>
</html>
)HTML";
  return html;
}

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
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
  server.begin();
  
  
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
  WiFiClient client = server.available();
  if(WiFi.status() == WL_CONNECTED){
      if (client) {
      String request = client.readStringUntil('\r');
      client.flush();

        if (request.indexOf("GET / ") != -1) {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html; charset=utf-8");
          client.println("Connection: close");
          client.println();
          client.println(htmlTest());
        }
        else if(request.indexOf("/control") != -1) {
          manejarControl(client, request);
          Serial.println("Aqui llega");
        }
        
        client.stop();
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
  }else{
    Serial.print("Conexión WIFI perdida, revise la conexión");
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi reconectado.");
  }

}

void manejarControl(WiFiClient &client, String request) {
  int puerta = 0;
  String accion = "";
  String respuesta = "Comando ejecutado";

  if (request.indexOf("puerta=1") != -1) puerta = 1;
  else if (request.indexOf("puerta=2") != -1) puerta = 2;

  if (request.indexOf("accion=abrir") != -1) accion = "abrir";
  else if (request.indexOf("accion=cerrar") != -1) accion = "cerrar";

  if (puerta == 1 && accion == "abrir") {
    if(puertaCerrada2){
      abrirPuerta1 = true;
      respuesta = "Abriendo puerta exterior";
    }else{
      respuesta = "ERROR: No se puede abrir la puerta 1 - Puerta 2 abierta";
      error = true;
    }
  }
  else if (puerta == 1 && accion == "cerrar") {
    abrirPuerta1 = false;
    respuesta = "Cerrando puerta exterior";
  }
  else if (puerta == 2 && accion == "abrir") {
    if(puertaCerrada1){
      abrirPuerta2 = true;
      respuesta = "Abriendo puerta interior";
    }else{
      respuesta = "ERROR: No se puede abrir la puerta 2 - Puerta 1 abierta";
      error = true;
    }
  }
  else if (puerta == 2 && accion == "cerrar") {
    abrirPuerta2 = false;
    respuesta = "Cerrando puerta interior";
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.println(respuesta);
  Serial.println(respuesta);
}

void manejarEstados(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();

  String json = "{";
  json += "\"puerta1\":"; json += puertaCerrada1 ? "true" : "false";
  json += ",\"puerta2\":"; json += puertaCerrada2 ? "true" : "false";
  json += ",\"temperatura\":"; json += String(temperatura);
  json += ",\"humedad\":"; json += String(humedad);
  json += ",\"led_color\":\"";

  if (estadoLED == "Verde")      json += "#00ff00";
  else if (estadoLED == "Rojo")  json += "#ff0000";
  else if (estadoLED == "Azul")  json += "#0000ff";
  else if (estadoLED == "Blanco")json += "#ffffff";
  else                           json += "#000000";

  json += "\",\"led_text\":\"";
  json += estadoLED;
  json += "\"}";

  client.println(json);
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



