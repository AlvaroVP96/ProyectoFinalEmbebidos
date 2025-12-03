#include <ESP32Servo.h>
#include <DHT11.h>
#include <WiFi.h>
#include "esp_sleep.h"
#include "driver/touch_pad.h"


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
void manejarControl(WiFiClient &client, String request);
void manejarEstados(WiFiClient &client);
void manejarSleep(WiFiClient &client);
void goToSleep();

// === WIFI ===
const char* ssid = "OnePlus3";
const char* password = "wifioneplus7T";
WiFiServer server(80);

// === DHT11 ===
#define DHT_PIN 5
DHT11 dht(DHT_PIN);
int temperatura;
int humedad;

// === Timer no bloqueante ===
unsigned long t0 = 0;
unsigned long intervalo = 2000;

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

// === Touchpad (despertar) ===
// T0 = GPIO 4 (tocar para despertar del deep sleep)
#define TOUCH_CH T0
#define TOUCH_THRESHOLD 200


// ---------------- HTML ----------------
String htmlTest() {
  String html = R"HTML(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Control de Puertas ESP32</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }
    .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
    h1 { text-align: center; color: #333; margin-bottom: 30px; }
    .puertas-container { display: flex; justify-content: space-between; gap: 20px; margin-bottom: 30px; }
    .puerta { background: #f8f9fa; padding: 20px; border-radius: 8px; border: 2px solid #e9ecef; flex: 1; text-align: center; }
    .puerta h2 { color: #495057; margin-bottom: 15px; font-size: 1.2em; }
    .estado-puerta { background: white; padding: 10px; border-radius: 5px; margin: 15px 0; border: 1px solid #dee2e6; }
    .estado { font-weight: bold; font-size: 1.1em; }
    .abierta { color: #4CAF50; }
    .cerrada { color: #f44336; }
    .botones { display: flex; gap: 10px; justify-content: center; margin-top: 15px; flex-wrap: wrap; }
    button { padding: 12px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 0.9em; font-weight: bold; }
    .abrir { background: #4CAF50; color: white; }
    .abrir:hover { background: #45a049; }
    .cerrar { background: #f44336; color: white; }
    .cerrar:hover { background: #da190b; }
    .sleep { background: #555; color: white; }
    .sleep:hover { background: #444; }
    .led-section { background: #f8f9fa; padding: 20px; border-radius: 8px; border: 2px solid #e9ecef; text-align: center; margin-bottom: 30px; }
    .led-label { font-weight: bold; margin-bottom: 10px; color: #495057; font-size: 1.1em; }
    .led-indicator { width: 80px; height: 80px; border-radius: 50%; border: 3px solid #333; margin: 10px auto; transition: background-color 0.3s ease; }
    .led-text { font-weight: bold; margin-top: 5px; }
    .sensor-section { background: #f8f9fa; padding: 20px; border-radius: 8px; border: 2px solid #e9ecef; text-align: center; }
    .sensor-section h2 { color: #495057; margin-bottom: 15px; }
    .sensor-data { display: flex; justify-content: center; gap: 30px; }
    .sensor-item { background: white; padding: 15px; border-radius: 5px; border: 1px solid #dee2e6; min-width: 120px; }
    .sensor-item .label { font-weight: bold; color: #666; margin-bottom: 5px; }
    .sensor-item .value { font-size: 1.2em; font-weight: bold; color: #333; }
    .debug { display: none; }
  </style>
</head>
<body>
  <div class="container">
    <h1>Control de Puertas</h1>

    <!-- Sección LED -->
    <div class="led-section">
      <div class="led-label">LED</div>
      <div id="ledIndicator" class="led-indicator" style="background-color: #000000;"></div>
      <div id="ledText" class="led-text">Apagado</div>
    </div>

    <!-- Contenedor de puertas -->
    <div class="puertas-container">
      <!-- Puerta 1 -->
      <div class="puerta">
        <h2>Puerta Exterior</h2>
        <div class="estado-puerta">
          <div class="label">Estado</div>
          <div class="estado" id="estado1">Cargando...</div>
        </div>
        <div class="botones">
          <button class="abrir" onclick="controlPuerta(1, 'abrir')">Abrir</button>
          <button class="cerrar" onclick="controlPuerta(1, 'cerrar')">Cerrar</button>
        </div>
      </div>

      <!-- Puerta 2 -->
      <div class="puerta">
        <h2>Puerta Interior</h2>
        <div class="estado-puerta">
          <div class="label">Estado</div>
          <div class="estado" id="estado2">Cargando...</div>
        </div>
        <div class="botones">
          <button class="abrir" onclick="controlPuerta(2, 'abrir')">Abrir</button>
          <button class="cerrar" onclick="controlPuerta(2, 'cerrar')">Cerrar</button>
        </div>
      </div>
    </div>

    <!-- Sección Sensores -->
    <div class="sensor-section">
      <h2>Sensores</h2>
      <div class="sensor-data">
        <div class="sensor-item">
          <div class="label">Temperatura</div>
          <div class="value" id="temp">--</div>
        </div>
        <div class="sensor-item">
          <div class="label">Humedad</div>
          <div class="value" id="hum">--</div>
        </div>
      </div>
    </div>

    <!-- Botón de dormir -->
    <div style="text-align:center; margin-top:25px;">
      <button class="sleep" onclick="dormir()">Dormir ESP32</button>
    </div>

    <div class="debug" id="debugInfo"></div>
  </div>

  <script>
    function controlPuerta(numero, accion) {
      fetch('/control?puerta=' + numero + '&accion=' + accion)
        .then(r => r.text())
        .then(_ => actualizarEstados())
        .catch(console.error);
    }

    function dormir() {
      fetch('/sleep')
        .then(r => r.text())
        .then(txt => { alert(txt); })
        .catch(console.error);
    }

    function actualizarEstados() {
      fetch('/estados')
        .then(r => { if (!r.ok) throw new Error('HTTP ' + r.status); return r.text(); })
        .then(text => {
          const data = JSON.parse(text);
          document.getElementById('estado1').textContent = data.puerta1 ? 'CERRADA' : 'ABIERTA';
          document.getElementById('estado2').textContent = data.puerta2 ? 'CERRADA' : 'ABIERTA';
          document.getElementById('estado1').className = 'estado ' + (data.puerta1 ? 'cerrada' : 'abierta');
          document.getElementById('estado2').className = 'estado ' + (data.puerta2 ? 'cerrada' : 'abierta');
          document.getElementById('temp').textContent = (data.temperatura !== undefined ? data.temperatura + '°C' : '--');
          document.getElementById('hum').textContent = (data.humedad !== undefined ? data.humedad + '%' : '--');
          const ledIndicator = document.getElementById('ledIndicator');
          const ledText = document.getElementById('ledText');
          if (data.led_color) ledIndicator.style.backgroundColor = data.led_color;
          if (data.led_text)  ledText.textContent = data.led_text;
        })
        .catch(console.error);
    }

    setInterval(actualizarEstados, 3000);
    actualizarEstados();
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

  if (WiFi.status() == WL_CONNECTED){
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
      else if (request.indexOf("/control") != -1) {
        manejarControl(client, request);
      }
      else if (request.indexOf("/estados") != -1) {
        manejarEstados(client);
      }
      else if (request.indexOf("/sleep") != -1) {
        manejarSleep(client);
      }

      client.stop();
    }

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

void manejarSleep(WiFiClient &client){
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.println("Entrando en deep sleep. Toca el pin táctil GPIO 4 (T0) para despertar.");
  client.flush();

  delay(150);
  goToSleep();
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



