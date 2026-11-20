#include <WiFi.h>
// #include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <SensirionI2cSen65.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// WIFI & SUPABASE
// =====================================================
const char *WIFINAME = "Tree-Fi";
const char *WIFIPASS = "address8ode4mil";

// const char* SUPABASE_URL = "https://elsdzrsjlftdfaffsjho.supabase.co";
// const char* SUPABASE_KEY = "sb_publishable_yTgdPq-OBtihLU6wLasQUQ_esO483xv";

const char *DB_API_URL = "http://192.168.1.211:3000";
// =====================================================
// SENSOR PINS & SETUP
// =====================================================
const int SEN65_SDA = 26;
const int SEN65_SCL = 14;
SensirionI2cSen65 sen65;

const int MHZ19_RX = 32;
const int MHZ19_TX = 33;
HardwareSerial mhz19Serial(2);

// =====================================================
// OLED DISPLAY (Wire1 on pins 27 & 25)
// =====================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, -1);

// =====================================================
// DATA VARIABLES
// =====================================================
float pm1p0 = 0, pm2p5 = 0, pm4p0 = 0, pm10 = 0;
float humidity = 0, temperature = 0, vocIndex = 0, noxIndex = 0;
int co2ppm = 0;
bool co2Valid = false;

// =====================================================
// MH-Z19C READ FUNCTION
// =====================================================
bool readMHZ19C()
{
    while (mhz19Serial.available())
    {
        mhz19Serial.read();
    }

    byte command[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
    mhz19Serial.write(command, 9);
    mhz19Serial.flush();

    delay(200); // a bit more time for response

    byte response[9];
    int bytesRead = 0;
    unsigned long startTime = millis();

    while (bytesRead < 9 && millis() - startTime < 1000)
    {
        if (mhz19Serial.available())
        {
            response[bytesRead] = mhz19Serial.read();
            bytesRead++;
        }
        yield();
    }

    if (bytesRead != 9)
    {
        co2Valid = false;
        return false;
    }

    if (response[0] != 0xFF || response[1] != 0x86)
    {
        co2Valid = false;
        return false;
    }

    byte checksum = 0xFF - (response[1] + response[2] + response[3] + response[4] + response[5] + response[6] + response[7]) + 1;
    if (checksum != response[8])
    {
        co2Valid = false;
        return false;
    }

    co2ppm = ((int)response[2] * 256) + response[3];
    co2Valid = true;
    return true;
}

// =====================================================
// SEND DATA TO SUPABASE
// =====================================================
void sendToDatabase()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("DB: WiFi not connected.");
        return;
    }

    HTTPClient http;
    String url = String(DB_API_URL) + "/air_quality_readings";

    Serial.println("\nSending data to local DB...");

    if (!http.begin(url))
    { // plain HTTP, no WiFiClientSecure
        Serial.println("DB: HTTP connection could not be started.");
        return;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Prefer", "return=minimal");
    // no apikey header needed

    String json = "{";
    json += "\"pm1_0\":" + String(pm1p0, 2) + ",";
    json += "\"pm2_5\":" + String(pm2p5, 2) + ",";
    json += "\"pm4_0\":" + String(pm4p0, 2) + ",";
    json += "\"pm10\":" + String(pm10, 2) + ",";
    json += "\"temperature\":" + String(temperature, 2) + ",";
    json += "\"humidity\":" + String(humidity, 2) + ",";
    json += "\"voc_index\":" + String(vocIndex, 2) + ",";
    json += "\"nox_index\":" + String(noxIndex, 2);
    if (co2Valid)
        json += ",\"co2_ppm\":" + String(co2ppm);
    json += "}";

    int code = http.POST(json);
    if (code > 0)
    {
        Serial.print("DB HTTP response: ");
        Serial.println(code); // 201 = success
    }
    else
    {
        Serial.print("HTTP connection error: ");
        Serial.println(http.errorToString(code));
    }
    http.end();
}
// =====================================================
// OLED: FULL METRICS SCREEN
// =====================================================
void drawFullScreen(uint16_t senError, bool mhzSuccess)
{
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    display.setCursor(0, 0);
    display.println("AIR QUALITY MONITOR");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // PM1.0 + PM2.5
    display.setCursor(0, 14);
    display.print("PM1.0: ");
    if (!senError)
    {
        display.print(pm1p0, 1);
    }
    else
    {
        display.print("ERR");
    }
    display.print("  PM2.5: ");
    if (!senError)
    {
        display.println(pm2p5, 1);
    }
    else
    {
        display.println("ERR");
    }

    // PM4.0 + PM10
    display.setCursor(0, 24);
    display.print("PM4.0: ");
    if (!senError)
    {
        display.print(pm4p0, 1);
    }
    else
    {
        display.print("ERR");
    }
    display.print("  PM10: ");
    if (!senError)
    {
        display.println(pm10, 1);
    }
    else
    {
        display.println("ERR");
    }

    // Temp + Humidity
    float temperatureF = (temperature * 1.8) + 32;
    display.setCursor(0, 34);
    display.print("Temp: ");
    if (!senError)
    {
        display.print(temperatureF, 1);
        display.print("F  RH: ");
        display.println(humidity, 1);
    }
    else
    {
        display.println("ERR");
    }

    // VOC + NOx Index
    display.setCursor(0, 44);
    display.print("VOC: ");
    if (!senError)
    {
        display.print((int)vocIndex);
    }
    else
    {
        display.print("ERR");
    }
    display.print("  NOx: ");
    if (!senError)
    {
        display.println((int)noxIndex);
    }
    else
    {
        display.println("ERR");
    }

    // CO2
    display.setCursor(0, 54);
    display.print("CO2: ");
    if (mhzSuccess && co2Valid)
    {
        display.print(co2ppm);
        display.println(" ppm");
    }
    else
    {
        display.println("ERR");
    }

    display.display();
}

// =====================================================
// SETUP
// =====================================================
void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nBooting Full Air Quality System...");

    // I2C
    Wire.begin(SEN65_SDA, SEN65_SCL);
    Wire1.begin(27, 25);

    // OLED
    if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
    {
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println("Air Quality Monitor");
        display.println("Connecting WiFi...");
        display.display();
    }
    else
    {
        Serial.println("OLED init failed");
    }

    // SEN65
    sen65.begin(Wire, 0x6B);
    uint16_t err = sen65.startContinuousMeasurement();
    if (err)
    {
        Serial.print("SEN65 start error: ");
        Serial.println(err);
    }

    // MH-Z19C
    mhz19Serial.begin(9600, SERIAL_8N1, MHZ19_RX, MHZ19_TX);

    // WiFi
    WiFi.begin(WIFINAME, WIFIPASS);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        yield();
    }
    Serial.println("\nWiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("System Ready!");
    display.println("WiFi Connected");
    display.display();
    delay(2000);
}

// =====================================================
// LOOP
// =====================================================
void loop()
{
    static unsigned long lastSensorRead = 0;
    if (millis() - lastSensorRead >= 4000)
    {
        lastSensorRead = millis();

        uint16_t senError = sen65.readMeasuredValues(
            pm1p0, pm2p5, pm4p0, pm10,
            humidity, temperature, vocIndex, noxIndex);
        bool mhzSuccess = readMHZ19C();

        drawFullScreen(senError, mhzSuccess);

        // Debug to Serial
        if (!senError)
        {
            Serial.print("PM1.0: ");
            Serial.print(pm1p0);
            Serial.print(" PM2.5: ");
            Serial.print(pm2p5);
            Serial.print(" PM4.0: ");
            Serial.print(pm4p0);
            Serial.print(" PM10: ");
            Serial.print(pm10);
            Serial.print(" T(C): ");
            Serial.print(temperature);
            Serial.print(" RH(%): ");
            Serial.print(humidity);
            Serial.print(" VOC: ");
            Serial.print(vocIndex);
            Serial.print(" NOx: ");
            Serial.print(noxIndex);
            Serial.print(" CO2: ");
            if (mhzSuccess && co2Valid)
                Serial.println(co2ppm);
            else
                Serial.println("ERR");
        }
        else
        {
            Serial.print("SEN65 read error: ");
            Serial.println(senError);
        }
    }

    static unsigned long lastSupabaseUpload = 0;
    if (millis() - lastSupabaseUpload >= 30000)
    {
        lastSupabaseUpload = millis();
        sendToDatabase();
    }

    yield();
}
