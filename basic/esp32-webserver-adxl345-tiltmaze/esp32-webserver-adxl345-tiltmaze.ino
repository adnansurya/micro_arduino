#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_ADXL345_U.h>

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
WebServer server(80);

const char* ssid = "MIKRO";
const char* password = "1DEAlist";

String htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Tilt Maze Game - ADXL345</title>
    <style>
        #gameCanvas {
            border: 2px solid black;
            background: #eee;
        }
        body {
            text-align: center;
            font-family: Arial, sans-serif;
        }
        .sensor-data {
            margin: 10px;
            padding: 10px;
            background: #f0f0f0;
            border-radius: 5px;
        }
    </style>
</head>
<body>
    <h1>🎮 Tilt Maze Game</h1>
    <p>Miringkan ADXL345 untuk menggerakkan bola!</p>
    
    <div class="sensor-data">
        <strong>Sensor Data:</strong> 
        <span id="sensorValues">Waiting...</span>
    </div>
    
    <canvas id="gameCanvas" width="400" height="400"></canvas>
    <p id="status">Connecting to ESP32...</p>
    <button onclick="resetGame()">Reset Game</button>
    
    <script>
        const canvas = document.getElementById('gameCanvas');
        const ctx = canvas.getContext('2d');
        const statusText = document.getElementById('status');
        const sensorValues = document.getElementById('sensorValues');
        
        let ball = { x: 50, y: 50, radius: 15 };
        let target = { x: 350, y: 350, size: 30 };
        let gameActive = true;

        function drawGame() {
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            
            // Draw ball with shadow effect
            ctx.beginPath();
            ctx.arc(ball.x, ball.y, ball.radius, 0, Math.PI * 2);
            ctx.fillStyle = '#3498db';
            ctx.fill();
            ctx.strokeStyle = '#2980b9';
            ctx.lineWidth = 2;
            ctx.stroke();
            
            // Draw target
            ctx.fillStyle = '#2ecc71';
            ctx.fillRect(target.x, target.y, target.size, target.size);
            ctx.strokeStyle = '#27ae60';
            ctx.lineWidth = 2;
            ctx.strokeRect(target.x, target.y, target.size, target.size);
            
            // Check win condition
            const dist = Math.sqrt(
                Math.pow(ball.x - (target.x + target.size/2), 2) + 
                Math.pow(ball.y - (target.y + target.size/2), 2)
            );
            
            if (dist < ball.radius + target.size/2 && gameActive) {
                ctx.fillStyle = '#f39c12';
                ctx.font = 'bold 30px Arial';
                ctx.fillText('🎉 YOU WIN!', 120, 200);
                gameActive = false;
            }
        }

        function updateSensorData() {
            fetch('/data')
                .then(response => response.text())
                .then(data => {
                    statusText.textContent = 'Connected to ESP32';
                    const [tiltX, tiltY] = data.split(',').map(parseFloat);
                    sensorValues.textContent = `Tilt X: ${tiltX.toFixed(1)}°, Tilt Y: ${tiltY.toFixed(1)}°`;
                    
                    if (gameActive) {
                        // Move ball based on tilt (adjust sensitivity as needed)
                        ball.x -= tiltY * 0.2;
                        ball.y -= tiltX * 0.2;
                        
                        // Boundary checking
                        ball.x = Math.max(ball.radius, Math.min(canvas.width - ball.radius, ball.x));
                        ball.y = Math.max(ball.radius, Math.min(canvas.height - ball.radius, ball.y));
                        
                        drawGame();
                    }
                })
                .catch(error => {
                    statusText.textContent = 'Error fetching data - ' + error;
                    sensorValues.textContent = 'Error';
                });
        }

        function resetGame() {
            ball.x = 50;
            ball.y = 50;
            gameActive = true;
            drawGame();
            statusText.textContent = 'Game Reset!';
        }

        // Update sensor data every 100ms
        setInterval(updateSensorData, 100);
        drawGame();
    </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(9600);
  
  // Initialize ADXL345
  Serial.println("Initializing ADXL345...");
  if (!accel.begin()) {
    Serial.println("Could not find a valid ADXL345 sensor, check wiring!");
    while (1);
  }
  Serial.println("ADXL345 Found!");
  
  // Set sensitivity (2_G is good for tilt games)
  accel.setRange(ADXL345_RANGE_2_G);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
  
  // Setup web server routes
  server.on("/", []() {
    server.send(200, "text/html", htmlPage);
  });
  
  server.on("/data", []() {
    sensors_event_t event; 
    accel.getEvent(&event);
    
    float tiltX = atan2(event.acceleration.y, event.acceleration.z) * 180/PI;
    float tiltY = atan2(event.acceleration.x, event.acceleration.z) * 180/PI;
    
    server.send(200, "text/plain", String(tiltX) + "," + String(tiltY));
  });
  
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  delay(10);
}