// include used libraries
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <EasyUltrasonic.h>
#include "DHT.h"
#include <Ticker.h>

// defining sensors properties
#define DHTTYPE DHT22
#define DHTPin D2

#define TRIGPIN D5
#define ECHOPIN D6

//starting libraries and sensors
Ticker timer;
EasyUltrasonic ultrasonic;
DHT dht(DHTPin, DHTTYPE);
bool get_data = false;

// Connecting to the Internet
char * ssid = "ssid";
char * password = "password";

// Running a web server
ESP8266WebServer server;

// Adding a websocket to the server
WebSocketsServer webSocket = WebSocketsServer(81);

// Serving a web page (from flash memory)
// formatted as a string literal!
char webpage[] PROGMEM = R"=====(
  <html>
  <!-- Adding a data chart using Chart.js -->
    <head>
      <script src='https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.5.0/Chart.min.js'></script>
    </head>
    <body onload="javascript:init()">
      <!-- Adding a slider for controlling data rate -->
      <div>
        <input type="range" min="1" max="10" value="5" id="dataRateSlider" oninput="sendDataRate()" />
        <label for="dataRateSlider" id="dataRateLabel">Rate: 0.2Hz</label>
      </div>
      <hr/>
      <div>
        <canvas id="temp-chart" width="400" height="300"></canvas>
      </div>
      <div>
        <canvas id="dist-chart" width="400" height="300"></canvas>
      </div>
      <div>
        <canvas id="related-chart" width="400" height="300"></canvas>
      </div>

      <!-- Adding a websocket to the client (webpage) -->
      <script>
        var webSocket, tempPlot, distPlot, relatedPlot;
        var maxDataPoints = 20;
        function removeData(){
          tempPlot.data.labels.shift();
          tempPlot.data.datasets[0].data.shift();

          distPlot.data.labels.shift();
          distPlot.data.datasets[0].data.shift();

          relatedPlot.data.labels.shift();
          relatedPlot.data.datasets[0].data.shift();    
        }
        function addData(t, temp, dist) {
          if(tempPlot.data.labels.length > maxDataPoints) removeData();
          
          tempPlot.data.labels.push(t);
          tempPlot.data.datasets[0].data.push(temp);
          tempPlot.update();

          distPlot.data.labels.push(t);
          distPlot.data.datasets[0].data.push(dist);
          distPlot.update();

          relatedPlot.data.labels.push(temp);
          relatedPlot.data.datasets[0].data.push(dist);
          relatedPlot.update();        
        }
        function init() {
          webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');
          tempPlot = new Chart(document.getElementById("temp-chart"), {
            type: 'line',
            data: {
              labels: [],
              datasets: [{
                data: [],
                label: "Temperature (°C)",
                borderColor: "#3e95cd",
                fill: false
              }]
            }
          });
          distPlot = new Chart(document.getElementById("dist-chart"), {
            type: 'line',
            data: {
              labels: [],
              datasets: [{
                data: [],
                label: "Water Level (cm)",
                borderColor: "#3e95cd",
                fill: false
              }]
            }
          });
          relatedPlot = new Chart(document.getElementById("related-chart"), {
            type: 'line',
            data: {
              labels: [],
              datasets: [{
                data: [],
                label: "Temperature & water-level Graph",
                borderColor: "#3e95cd",
                fill: false
              }]
            }
          });
          webSocket.onmessage = function(event) {
            var json = JSON.parse(event.data);
            var today = new Date();
            var t = today.getHours() + ":" + today.getMinutes() + ":" + today.getSeconds();
            addData(t, json.temp, json.dist);
          }
        }
        function sendDataRate(){
          var dataRate = document.getElementById("dataRateSlider").value;
          webSocket.send(dataRate);
          dataRate = 1.0/dataRate;
          document.getElementById("dataRateLabel").innerHTML = "Rate: " + dataRate.toFixed(2) + "Hz";
        }
      </script>
    </body>
  </html>
)=====";

void setup() {
  
  pinMode(DHTPin, INPUT);
  dht.begin();
  ultrasonic.attach(TRIGPIN, ECHOPIN);
  
  // put your setup code here, to run once:
  WiFi.begin(ssid, password);
  Serial.begin(115200);
  while(WiFi.status()!=WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/",[](){
    server.send_P(200, "text/html", webpage);
  });
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  timer.attach(5, getData);
}

void loop() {
  webSocket.loop();
  server.handleClient();
  if(get_data){
    String json = "{\"temp\":";
    json += dht.readTemperature();
    json += ",\"dist\":";
    json += 25.8 - ultrasonic.getDistanceCM();
    json += "}";
    webSocket.broadcastTXT(json.c_str(), json.length());
    get_data = false;
  }
}

void getData() {
  get_data = true;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
  // Do something with the data from the client
  if(type == WStype_TEXT){
    float dataRate = (float) atof((const char *) &payload[0]);
    timer.detach();
    timer.attach(dataRate, getData);
  }
}
