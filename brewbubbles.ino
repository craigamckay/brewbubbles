// Write DHT22 sensor data to an MS‑SQL/MariaDB database
#include <WiFi.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include "Arduino_LED_Matrix.h"

//byte mac_addr[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// MySQL server information
IPAddress server_addr(192,168,86,250);
char user[] = "beerbrew";     // MySQL user login username
char password[] = "beerbrew"; // MySQL user login password
char db[] = "beerbrew";

// Wi‑Fi connection info
char ssid[] = "SOUTHESK-MAIN";   // your SSID
char pass[] = "hellowelcome";  // your SSID Password

WiFiClient client;        // Use this for Wi‑Fi instead of EthernetClient
MySQL_Connection conn((Client *)&client);
// Create an instance of the cursor passing in the connection
MySQL_Cursor cur = MySQL_Cursor(&conn);

// Connect the opto signal pin to the first INT pin (digital pin 2 on an Arduino Uno)
#define resolution 800000 // only capture pulse that are greater than this to avoid noise
//#define resolution 50000 // only capture pulse that are greater than this to avoid noise
#define sample 4 // sample size of pulses for average starts at 0

volatile unsigned long pulseThen;
volatile unsigned long pulseNow;
volatile unsigned long numPulses;
volatile unsigned long keeppulse[sample];
volatile double averagepulse;
volatile bool datatosave = false;

volatile int count=0;
const byte interruptPin = 2;

// LED Matrix
ArduinoLEDMatrix matrix;
const uint32_t happy[] = {
    0x19819,
    0x80000001,
    0x81f8000
};
const uint32_t heart[] = {
    0x3184a444,
    0x44042081,
    0x100a0040
};

// Buzzer
int buzzerPin = 4;

void isr()
{
  unsigned long now = micros();
  float pulselength;
  pulseNow = now;
  double totalsample = 0;
 
  if ((pulseNow - pulseThen) > resolution)
  {
    pulselength = (float)(pulseNow - pulseThen) / 1000000;
    if( count < sample )
    {
      keeppulse[count] = pulselength;
      count++;
    }
    else
    {
      count = 0;
      for(int x=0; (x<sample); x++) {
        totalsample += (int)keeppulse[x];
      }
      averagepulse = totalsample/sample;
      datatosave = true;
    }

    pulseThen = pulseNow;
    ++numPulses;
  }
}

void buzz(int buzzDuration, int count, int pauseDuration) {
  for (int i = 0; i<count; i++) {
    digitalWrite(buzzerPin, LOW);
    delay(buzzDuration);
    digitalWrite(buzzerPin, HIGH);
    delay(pauseDuration);
  }
  delay(100);
}

// Setup Wi‑Fi and SQL interfacess
void setup() {
  Serial.begin(9600);
  matrix.begin();

  // Buzzer config
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, HIGH);  

  // Begin Wi‑Fi section
  Serial.println("Connecting to WiFi  ");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  matrix.loadFrame(LEDMATRIX_CLOUD_WIFI);
  buzz(400, 4, 100);
  // print out info about the connection:
  Serial.println("\nConnected to network");
  Serial.print("My IP address is: ");
  Serial.println(WiFi.localIP());

  //  WiFiClient client;
  MySQL_Connection conn(&client);
  Serial.print("Connecting to SQL...  ");
  if (conn.connect(server_addr, 3306, user, password)) {
    matrix.loadFrame(LEDMATRIX_EMOJI_HAPPY);
    buzz(100, 3, 200);
    Serial.println("OK.");
  } else {
    matrix.loadFrame(LEDMATRIX_EMOJI_SAD);
    buzz(5000, 1, 0);
    Serial.println("FAILED.");
  }

  // create MySQL cursor object
  MySQL_Cursor* cursor;
  cursor = new MySQL_Cursor(&conn);

  // Finally, create the ISR for the opto coupler
  attachInterrupt(digitalPinToInterrupt(interruptPin), isr, RISING);
  numPulses = 0;                      // prime the system to start a new reading
}

void loop() {
  if (datatosave) {
    if (!conn.connected()) {
      conn.connect(server_addr, 3306, user, password);
      matrix.loadFrame(LEDMATRIX_EMOJI_HAPPY);
      buzz(100, 2, 200);
    }

    if (conn.connected())
    {
      // Initiate the query class instance
      MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);

      // Prepare INSERT query string
      char INSERT_SQL[] = "INSERT INTO beerbrew.bubble (sample, avgpulse) VALUES ('%i','%.4f')";
      char query[128];
      sprintf(query, INSERT_SQL, sample, averagepulse);

      // Execute the INSERT query in the database
      Serial.println(query);
      if (conn.connected()) {
        bool result = cur_mem->execute(query);
        matrix.loadFrame(LEDMATRIX_HEART_BIG);
        delay(500);      
        if (result) {
          matrix.loadFrame(LEDMATRIX_EMOJI_HAPPY);  
          buzz(50, 3, 50);
        } else {
          matrix.loadFrame(LEDMATRIX_EMOJI_SAD); 
        }
      }

      datatosave = false;
    }
  }
}