#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// Замените на свои данные Wi-Fi
const char* ssid = "SSID";
const char* password = "123456789";

ESP8266WebServer server(80);

// Глобальные переменные
String lastCommand = "";  // последнее полученное движение: "front", "left" или "right" (или "rotate")
bool rotated = false;     // false – нормальная ориентация, true – повернутая (на 180°)

// Обработчик главной страницы с canvas и JavaScript
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Управление роботом</title>"
                "<style>canvas { border: 1px solid #000; }</style></head><body>"
                "<h2>Движение робота</h2>"
                "<canvas id='canvas' width='400' height='400'></canvas>"
                "<script>"
                "var canvas = document.getElementById('canvas');"
                "var ctx = canvas.getContext('2d');"
                "var x = canvas.width/2, y = canvas.height/2;"  // стартовая позиция"
                "var step = 10;"  // шаг в пикселях"
                "var rotated = false;"  // локальный флаг ориентации"
                "function update() {"
                "  fetch('/state').then(function(response) { return response.json(); }).then(function(data) {"
                "    rotated = (data.rotated === true || data.rotated === 'true');"
                "    var cmd = data.command;"
                "    var oldX = x, oldY = y;"
                "    if(cmd === 'front') {"
                "      if(!rotated) { y -= step; } else { y += step; }"
                "    } else if(cmd === 'left') {"
                "      if(!rotated) { x -= step; } else { x += step; }"
                "    } else if(cmd === 'right') {"
                "      if(!rotated) { x += step; } else { x -= step; }"
                "    }"
                "    // Если была команда движения, рисуем линию от старой позиции к новой"
                "    if(cmd === 'front' || cmd === 'left' || cmd === 'right') {"
                "      ctx.beginPath();"
                "      ctx.moveTo(oldX, oldY);"
                "      ctx.lineTo(x, y);"
                "      ctx.stroke();"
                "    }"
                "    // Для команды \"rotate\" линию не рисуем, просто обновляем флаг"
                "  }).catch(function(err){ console.log('Error:', err); });"
                "}"
                "setInterval(update, 200);"
                "</script>"
                "</body></html>";
  server.send(200, "text/html", html);
}

// Обработчик для возврата состояния (JSON с последней командой и флагом rotated)
void handleState() {
  String json = "{";
  json += "\"command\":\"" + lastCommand + "\",";
  json += "\"rotated\":" + String(rotated ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
  // После отправки очищаем команду, чтобы повторно не рисовать одно и то же движение
  lastCommand = "";
}

void setup() {
  Serial.begin(9600);
  
  // Подключение к Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Подключение к WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi подключен, IP адрес:");
  Serial.println(WiFi.localIP());
  
  // Настройка маршрутов веб-сервера
  server.on("/", handleRoot);
  server.on("/state", handleState);
  server.begin();
  Serial.println("HTTP сервер запущен");
}

void loop() {
  server.handleClient();
  
  // Чтение команд из Serial (приходят от Arduino Nano)
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();  // удаляем лишние пробелы и символы перевода строки
    if (cmd.length() > 0) {
      Serial.print("Получена команда: ");
      Serial.println(cmd);
      
      // Если команда "rotate" – переключаем флаг ориентации
      if (cmd == "rotate") {
        rotated = !rotated;
        // Можно сохранить команду, если хотите отобразить событие
        lastCommand = "rotate";
      }
      // Если получена одна из команд движения – сохраняем её для отрисовки
      else if (cmd == "front" || cmd == "left" || cmd == "right") {
        lastCommand = cmd;
      }
    }
  }
}