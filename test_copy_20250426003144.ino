// --- Подключаем необходимые библиотеки для Wi-Fi и веб-сервера ---
#include <WiFi.h>
#include <WebServer.h>

// --- Настройки Wi-Fi сети ---
const char* WIFI_SSID = "MGTS_GPON_D2D3";
const char* WIFI_PASSWORD = "afaFKhMb";

// --- Пины подключения моторов ---
#define LEFT_MOTOR_IN1   5
#define LEFT_MOTOR_IN2   14
#define RIGHT_MOTOR_IN1  32
#define RIGHT_MOTOR_IN2  33

// --- Пины подключения ультразвуковых датчиков ---
#define TRIG_FRONT 13
#define ECHO_FRONT 12
#define TRIG_LEFT  26
#define ECHO_LEFT  25
#define TRIG_RIGHT 17
#define ECHO_RIGHT 18

#define TURN_DELAY 3000
#define DIST_THRESHOLD 20
#define POSITION_UPDATE_INTERVAL 100
#define TURN_COOLDOWN (TURN_DELAY + TURN_DELAY * 1.5)  // задержка между поворотами

WebServer server(80);

const char* moveState = "STOPPED";
int directionIndex = 0;
long posX = 0, posY = 0;
unsigned long lastUpdate = 0;
unsigned long lastTurnTime = 0;

int leftSpeed = 255;
int rightSpeed = 255;

void stopMotors() {
  digitalWrite(LEFT_MOTOR_IN1, LOW);
  digitalWrite(LEFT_MOTOR_IN2, LOW);
  digitalWrite(RIGHT_MOTOR_IN1, LOW);
  digitalWrite(RIGHT_MOTOR_IN2, LOW);
}

void moveForward() {
  digitalWrite(LEFT_MOTOR_IN1, HIGH);
  digitalWrite(LEFT_MOTOR_IN2, LOW);
  digitalWrite(RIGHT_MOTOR_IN1, HIGH);
  digitalWrite(RIGHT_MOTOR_IN2, LOW);
  moveState = "FORWARD";
}

void turnLeft90() {
  digitalWrite(LEFT_MOTOR_IN1, LOW);
  digitalWrite(LEFT_MOTOR_IN2, HIGH);
  digitalWrite(RIGHT_MOTOR_IN1, HIGH);
  digitalWrite(RIGHT_MOTOR_IN2, LOW);
  moveState = "TURN_LEFT";
  delay(TURN_DELAY);
  stopMotors();
  directionIndex = (directionIndex + 3) % 4;
}

void turnRight90() {
  digitalWrite(LEFT_MOTOR_IN1, HIGH);
  digitalWrite(LEFT_MOTOR_IN2, LOW);
  digitalWrite(RIGHT_MOTOR_IN1, LOW);
  digitalWrite(RIGHT_MOTOR_IN2, HIGH);
  moveState = "TURN_RIGHT";
  delay(TURN_DELAY);
  stopMotors();
  directionIndex = (directionIndex + 1) % 4;
}

long measureDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000UL);
  if (duration == 0) return 999;
  return duration / 58;
}

void adjustMotorSpeeds(long distLeft, long distRight) {
  int delta = distRight - distLeft;
  leftSpeed = constrain(255 + delta / 2, 180, 255);
  rightSpeed = constrain(255 - delta / 2, 180, 255);
}

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Управление роботом</title>
  <style>
    canvas { border: 1px solid #000; }
  </style>
</head>
<body>
  <h1>Состояние робота: <span id="status">—</span></h1>
  <h2>Движение робота</h2>
  <canvas id="canvas" width="400" height="400"></canvas>
  <br><button onclick="clearRoute()">Очистить маршрут</button>
  <p>Передний датчик: <span id="front">—</span> см</p>
  <p>Левый датчик: <span id="left">—</span> см</p>
  <p>Правый датчик: <span id="right">—</span> см</p>
  <script>
    var canvas = document.getElementById('canvas');
    var ctx = canvas.getContext('2d');
    var route = [];
    if (localStorage.getItem('route')) {
      route = JSON.parse(localStorage.getItem('route'));
      for (let i = 1; i < route.length; i++) {
        ctx.beginPath();
        ctx.moveTo(route[i - 1][0], route[i - 1][1]);
        ctx.lineTo(route[i][0], route[i][1]);
        ctx.stroke();
      }
    }
    var x = canvas.width / 2, y = canvas.height / 2;
    var lastX = x, lastY = y;
    var step = 10;
    var rotated = false;

    function update() {
      fetch('/state').then(function(response) {
        return response.text();
      }).then(function(text) {
        let parts = text.split(',');
        var cmd = parts[0];
        var dx = parseInt(parts[1]);
        var dy = parseInt(parts[2]);
        var front = parts[3];
        var left = parts[4];
        var right = parts[5];
        document.getElementById('front').innerText = front;
        document.getElementById('left').innerText = left;
        document.getElementById('right').innerText = right;
        var cmd = parts[0];
        var dx = parseInt(parts[1]);
        var dy = parseInt(parts[2]);
        var newX = 200 + dx;
        var newY = 200 - dy;

        if (cmd === 'FORWARD') {
          var dirX = newX - lastX;
          var dirY = newY - lastY;
          var len = Math.sqrt(dirX * dirX + dirY * dirY);
          if (len > 0) {
            var ux = dirX / len;
            var uy = dirY / len;
            var shortX = lastX + ux * step;
            var shortY = lastY + uy * step;

            ctx.beginPath();
            ctx.moveTo(lastX, lastY);
            ctx.lineTo(shortX, shortY);
            ctx.stroke();
            route.push([shortX, shortY]);
            localStorage.setItem('route', JSON.stringify(route));

            lastX = shortX;
            lastY = shortY;
          }
        } else {
          lastX = newX;
          lastY = newY;
        }
      }).catch(function(err) {
        console.log('Error:', err);
      });
    }

    setInterval(update, 100);

    function clearRoute() {
      localStorage.removeItem('route');
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      route = [];
      x = canvas.width / 2;
      y = canvas.height / 2;
      lastX = x;
      lastY = y;
    }
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", HTML_PAGE);
}

void handleState() {
  long distFront = measureDistance(TRIG_FRONT, ECHO_FRONT);
  long distLeft = measureDistance(TRIG_LEFT, ECHO_LEFT);
  long distRight = measureDistance(TRIG_RIGHT, ECHO_RIGHT);

  char buffer[100];
  snprintf(buffer, sizeof(buffer), "%s,%ld,%ld,%ld,%ld,%ld", moveState, posX, posY, distFront, distLeft, distRight);
  server.send(200, "text/plain", buffer);
}

void setup() {
  Serial.begin(115200);

  pinMode(LEFT_MOTOR_IN1, OUTPUT);
  pinMode(LEFT_MOTOR_IN2, OUTPUT);
  pinMode(RIGHT_MOTOR_IN1, OUTPUT);
  pinMode(RIGHT_MOTOR_IN2, OUTPUT);

  pinMode(TRIG_FRONT, OUTPUT);
  pinMode(ECHO_FRONT, INPUT);
  pinMode(TRIG_LEFT, OUTPUT);
  pinMode(ECHO_LEFT, INPUT);
  pinMode(TRIG_RIGHT, OUTPUT);
  pinMode(ECHO_RIGHT, INPUT);

  stopMotors();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/state", handleState);
  server.begin();
}

void loop() {
  server.handleClient();
  Serial.println(moveState);
  long distFront = measureDistance(TRIG_FRONT, ECHO_FRONT);
  long distLeft = measureDistance(TRIG_LEFT, ECHO_LEFT);
  long distRight = measureDistance(TRIG_RIGHT, ECHO_RIGHT);

  adjustMotorSpeeds(distLeft, distRight);

  unsigned long now = millis();
  if (now - lastTurnTime < TURN_COOLDOWN) {
    return;
  }

  if (distLeft > DIST_THRESHOLD) {
    lastTurnTime = now;
    turnLeft90();
  } else if (distFront > DIST_THRESHOLD) {
    moveForward();
    if (now - lastUpdate > POSITION_UPDATE_INTERVAL) {
      lastUpdate = now;
      switch (directionIndex) {
        case 0: posY += 1; break;
        case 1: posX += 1; break;
        case 2: posY -= 1; break;
        case 3: posX -= 1; break;
      }
    }
  } else if (distRight > DIST_THRESHOLD) {
    lastTurnTime = now;
    turnRight90();
  } else {
    lastTurnTime = now;
    turnLeft90(); delay(100); turnLeft90();
    moveState = "TURN_AROUND";
    directionIndex = (directionIndex + 2) % 4;
  }

  delay(1);
}
