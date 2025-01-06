#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

// Flow sensor and GPS definitions
#define FLOW_SENSOR_PIN D2
SoftwareSerial gpsSerial(D5, D6); // GPS Rx/Tx

// SIM800L communication
SoftwareSerial simSerial(D8, D3);

// Wi-Fi credentials
const char* ssid = "SIH_2024_05"; 
const char* password = "1234567@";

// Server URL
const String serverURL = "http://nucleitech.online/chemicaltrack/update.php";

// Variables for flow calculations
float flowRate = 0;        
float totalFlow = 0;       
unsigned long lastTime = 0; 

// GPS variables
String gpsLatitude = "0.00";
String gpsLongitude = "0.00";
bool gpsFixed = false;

// Function prototypes
void connectToWiFi();
void uploadData(float flowRate, float totalFlow, String lat, String lon);
void sendSMS(float flowRate, float totalFlow);
void updateGPS();
String convertToDecimalDegrees(String nmeaCoord, String direction);
void updateSerial();

void setup() {
  Serial.begin(9600);
  simSerial.begin(9600);
  gpsSerial.begin(9600);
  connectToWiFi();
  Serial.println("Setup complete.");
}

void loop() {
  unsigned long currentTime = millis();

  // Read from flow sensor
  int flowValue = analogRead(FLOW_SENSOR_PIN);

  // Validate the flow sensor value
  if (flowValue < 20) { // Assuming noise produces low readings
    flowValue = 0; // Treat as no flow
  }

  flowRate = (flowValue / 1023.0) * 100; // Normalize to a percentage
  totalFlow += (flowRate / 60.0) * ((currentTime - lastTime) / 1000.0);
  lastTime = currentTime;

  Serial.print("Flow Rate: ");
  Serial.print(flowRate);
  Serial.println(" L/min");

  Serial.print("Total Flow: ");
  Serial.print(totalFlow);
  Serial.println(" L");

  // Update GPS and handle data
  updateGPS();
  if (!gpsFixed) {
    gpsLatitude = "10.9292";
    gpsLongitude = "78.7368";
  }

  static unsigned long lastUploadTime = 0;
  if (currentTime - lastUploadTime > 5000) {
    uploadData(flowRate, totalFlow, gpsLatitude, gpsLongitude);
    lastUploadTime = currentTime;
  }

  // Send SMS if flow rate is unusually high
  if (flowRate > 50) {
    sendSMS(flowRate, totalFlow);
    delay(3000);
  }

  delay(1000); // Wait 1 second before next reading
}

void uploadData(float flowRate, float totalFlow, String lat, String lon) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    String url = serverURL + "?gps_id=xyz&lat=" + lat + "&lon=" + lon + "&vstatus=open";
    Serial.println("Uploading to URL: " + url);

    http.begin(client, url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String response = http.getString();
      Serial.println("Server response: " + response);
    } else {
      Serial.println("Failed to upload data: " + String(httpCode));
    }

    http.end();
  } else {
    Serial.println("WiFi not connected.");
  }
}

void sendSMS(float flowRate, float totalFlow) {
  Serial.println("Initializing SMS...");
  delay(1000);

  simSerial.println("AT");
  updateSerial();

  simSerial.println("AT+CMGF=1");
  updateSerial();

  simSerial.println("AT+CMGS=\"+919629602658\"");
  updateSerial();

  simSerial.print("Alert! High Flow Detected.\n");
  simSerial.print("Flow Rate: ");
  simSerial.print(flowRate);
  simSerial.print(" L/min\n");
  simSerial.print("Total Flow: ");
  simSerial.print(totalFlow);
  simSerial.print(" L\n");
  simSerial.print("Location: Latitude ");
  simSerial.print(gpsLatitude);
  simSerial.print(", Longitude ");
  simSerial.print(gpsLongitude);

  simSerial.write(26);
  delay(3000);
}

void updateGPS() {
  while (gpsSerial.available()) {
    String gpsData = gpsSerial.readStringUntil('\n');
    if (gpsData.startsWith("$GPGGA")) {
      char gpsDataArray[100];
      gpsData.toCharArray(gpsDataArray, 100);

      char* token = strtok(gpsDataArray, ",");
      String tokens[15];
      int index = 0;

      while (token != NULL && index < 15) {
        tokens[index++] = String(token);
        token = strtok(NULL, ",");
      }

      if (tokens[2].length() > 0 && tokens[4].length() > 0) {
        gpsLatitude = convertToDecimalDegrees(tokens[2], tokens[3]);
        gpsLongitude = convertToDecimalDegrees(tokens[4], tokens[5]);
        gpsFixed = true;
      } else {
        gpsFixed = false;
      }
    }
  }
}

String convertToDecimalDegrees(String nmeaCoord, String direction) {
  float decimalDegrees = nmeaCoord.substring(0, 2).toFloat() +
                         (nmeaCoord.substring(2).toFloat() / 60.0);
  if (direction == "S" || direction == "W") {
    decimalDegrees *= -1;
  }
  return String(decimalDegrees, 6);
}

void updateSerial() {
  delay(500);
  while (Serial.available()) {
    simSerial.write(Serial.read());
  }
  while (simSerial.available()) {
    Serial.write(simSerial.read());
  }
}
void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected. IP Address: " + WiFi.localIP().toString());
}
