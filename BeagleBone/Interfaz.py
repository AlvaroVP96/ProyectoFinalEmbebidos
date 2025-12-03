from flask import Flask, render_template_string
import paho.mqtt.client as mqtt
import threading

app = Flask(__name__)

# Variable global para almacenar el último mensaje recibido
ultimo_mensaje_temp = "Esperando actualización..."
ultimo_mensaje_hum = "Esperando actualización..."
ultimo_mensaje_door1 = "Esperando actualización..."
ultimo_mensaje_door2 = "Esperando actualización..."

# Configuración MQTT
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
MQTT_TOPIC_TEMP = "Sensores/temperatura"
MQTT_TOPIC_HUM = "Sensores/humedad"
MQTT_TOPIC_DOOR_1 = "Sensores/Puertas/Puerta1"
MQTT_TOPIC_DOOR_2 = "Sensores/Puertas/Puerta2"

# Callback cuando se conecta al broker MQTT
def on_connect(client, userdata, flags, rc, properties=None):
    print(f"Conectado al broker MQTT con código: {rc}")
    client.subscribe(MQTT_TOPIC_TEMP)
    client.subscribe(MQTT_TOPIC_HUM)
    client.subscribe(MQTT_TOPIC_DOOR_1)
    client.subscribe(MQTT_TOPIC_DOOR_2)
    print(f"Suscrito al topic: {MQTT_TOPIC_TEMP}")
    print(f"Suscrito al topic: {MQTT_TOPIC_HUM}")
    print(f"Suscrito al topic: {MQTT_TOPIC_DOOR_1}")
    print(f"Suscrito al topic: {MQTT_TOPIC_DOOR_2}")



# Callback cuando se recibe un mensaje
def on_message(client, userdata, msg):
    global ultimo_mensaje_temp, ultimo_mensaje_hum,ultimo_mensaje_door2,ultimo_mensaje_door1
    
    # Verificar de qué topic viene el mensaje
    if msg.topic == MQTT_TOPIC_TEMP:
        ultimo_mensaje_temp = msg.payload.decode()
        print(f"Mensaje temperatura recibido: {ultimo_mensaje_temp}")
    elif msg.topic == MQTT_TOPIC_HUM:
        ultimo_mensaje_hum = msg.payload.decode()
        print(f"Mensaje humedad recibido: {ultimo_mensaje_hum}")
    elif msg.topic == MQTT_TOPIC_DOOR_1:
        ultimo_mensaje_door1 = msg.payload.decode()
        print(f"Mensaje humedad recibido: {ultimo_mensaje_door1}")
    elif msg.topic == MQTT_TOPIC_DOOR_2:
        ultimo_mensaje_door2 = msg.payload.decode()
        print(f"Mensaje humedad recibido: {ultimo_mensaje_door2}")

# Inicializar cliente MQTT
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

# Conectar al broker en un hilo separado
def start_mqtt():
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_forever()

# Iniciar MQTT en segundo plano
mqtt_thread = threading.Thread(target=start_mqtt, daemon=True)
mqtt_thread.start()

HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Monitor MQTT - Sistema de Sensores</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        
        header {
            background: white;
            padding: 30px;
            border-radius: 15px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            margin-bottom: 30px;
            text-align: center;
        }
        
        h1 {
            color: #333;
            font-size: 2.5em;
            margin-bottom: 10px;
        }
        
        .status {
            display: inline-block;
            padding: 8px 20px;
            background: #4CAF50;
            color: white;
            border-radius: 20px;
            font-size: 0.9em;
        }
        
        .dashboard {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin-top: 20px;
        }
        
        .card {
            background: white;
            padding: 25px;
            border-radius: 15px;
            box-shadow: 0 5px 15px rgba(0,0,0,0.1);
            transition: transform 0.3s ease;
        }
        
        .card:hover {
            transform: translateY(-5px);
            box-shadow: 0 10px 25px rgba(0,0,0,0.2);
        }
        
        .card h2 {
            color: #667eea;
            font-size: 1.3em;
            margin-bottom: 15px;
            border-bottom: 2px solid #667eea;
            padding-bottom: 10px;
        }
        
        .message-box {
            background: #f5f5f5;
            padding: 20px;
            border-radius: 10px;
            border-left: 4px solid #667eea;
            font-family: 'Courier New', monospace;
            font-size: 1.1em;
            color: #333;
            word-wrap: break-word;
        }
        
        .info {
            margin-top: 15px;
            font-size: 0.9em;
            color: #666;
        }
        
        .topic-badge {
            background: #e3f2fd;
            color: #1976d2;
            padding: 5px 10px;
            border-radius: 5px;
            font-family: monospace;
        }
        
        .timestamp {
            text-align: right;
            color: #999;
            font-size: 0.85em;
            margin-top: 10px;
        }
        
        @media (max-width: 768px) {
            h1 {
                font-size: 1.8em;
            }
            
            .dashboard {
                grid-template-columns: 1fr;
            }
        }
    </style>
    <script>
        // Auto-refresh cada 2 segundos
        setTimeout(function(){
            location.reload();
        }, 2000);
    </script>
</head>
<body>
    <div class="container">
        <header>
            <h1>Monitor MQTT</h1>
            <span class="status">Conectado</span>
        </header>
        
        <div class="dashboard">
            <div class="card">
                <h2>Sensor Temperatura</h2>
                <div class="message-box">
                    {{ mensaje_temp }}
                </div>
                <div class="info">
                    <strong>Topic:</strong> <span class="topic-badge">{{ topic_temp }}</span>
                </div>
                <div class="timestamp">
                    Actualizado automáticamente cada 2 segundos
                </div>
            </div>

            <div class="card">
                <h2>Sensor Humedad</h2>
                <div class="message-box">
                    {{ mensaje_hum }}
                </div>
                <div class="info">
                    <strong>Topic:</strong> <span class="topic-badge">{{ topic_hum }}</span>
                </div>
                <div class="timestamp">
                    Actualizado automáticamente cada 2 segundos
                </div>
            </div>
            
             <div class="card">
                <h2>Puerta Exterior</h2>
                <div class="message-box">
                    {{ mensaje_door1 }}
                </div>
                <div class="info">
                    <strong>Topic:</strong> <span class="topic-badge">{{ topic_door1 }}</span>
                </div>
                <div class="timestamp">
                    Actualizado automáticamente cada 2 segundos
                </div>
            </div>

             <div class="card">
                <h2>Puerta Interior</h2>
                <div class="message-box">
                    {{ mensaje_door2 }}
                </div>
                <div class="info">
                    <strong>Topic:</strong> <span class="topic-badge">{{ topic_door2 }}</span>
                </div>
                <div class="timestamp">
                    Actualizado automáticamente cada 2 segundos
                </div>
            </div>
            
            <!-- Aquí irán más cards cuando agregues los topics -->
        </div>
    </div>
</body>
</html>
"""

@app.route("/")
def index():
    return render_template_string(
        HTML_TEMPLATE,
        mensaje_hum=ultimo_mensaje_hum,
        mensaje_temp=ultimo_mensaje_temp,
        mensaje_door1=ultimo_mensaje_door1,
        mensaje_door2=ultimo_mensaje_door2,
        topic_temp=MQTT_TOPIC_TEMP,
        topic_hum=MQTT_TOPIC_HUM,
        topic_door1=MQTT_TOPIC_DOOR_1,
        topic_door2=MQTT_TOPIC_DOOR_2
    )

if __name__ == "__main__":
    app.run(debug=True, host='0.0.0.0', port=5000)