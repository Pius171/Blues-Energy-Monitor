/*
Copyright (c) 2021 Jakub Mandula

Example of using one PZEM module with Software Serial interface.
================================================================

If only RX and TX pins are passed to the constructor, software 
serial interface will be used for communication with the module.

*/

#include <PZEM004Tv30.h>
#include <Notecard.h>
#include "EEPROM.h"



#define DEBUG 1

#if DEBUG == 1
#define debug(...) Serial.print(__VA_ARGS__)      // Allow multiple arguments
#define debugln(...) Serial.println(__VA_ARGS__)  // Allow multiple arguments
#define debugf(...) Serial.printf(__VA_ARGS__)    // Use variadic macros for formatted strings
#else
#define debug(...)
#define debugln(...)
#define debugf(...)
#endif


#define usbSerial Serial
#define myProductID "com.gmail.onyemandukwu:energy_monitor"

/* The cellular notecard by default uses its embedded
*  SIM which comes with 500MB and 10 years worth of 
*  cellular service, but is not available in all regions
*  If it is not available in your region you can use an 
* external SIM card.
*/
#define USING_EXTERNAL_SIM true  // chnange to false if using the embedded SIM card

#define ATTN_PIN 18
#define RELAY 2
#define SDA 19
#define SDL 21


#define EEPROM_SIZE 64

#if defined(ESP32)
PZEM004Tv30 pzem(Serial2, 16, 17);
#else
PZEM004Tv30 pzem(Serial2);
#endif;

unsigned long previousMillis = 0;     // stores the last time
const unsigned long interval = 3000;  // interval in ms

Notecard notecard;

void setup() {
  /* Debugging serial */
  Wire.begin(SDA, SDL);  // SDA and SCL pins
  Serial.begin(115200);
  usbSerial.begin(115200);

  pinMode(ATTN_PIN, INPUT_PULLDOWN);
  pinMode(RELAY, OUTPUT);

  if (!EEPROM.begin(EEPROM_SIZE)) {
    debugln("failed to initialise EEPROM");
    delay(1000000);
  }
  bool load_state = EEPROM.read(0); // retrieve load_state from memory
  digitalWrite(RELAY,load_state);

  const size_t usb_timeout_ms = 3000;
  for (const size_t start_ms = millis(); !usbSerial && (millis() - start_ms) < usb_timeout_ms;)
    ;
  notecard.setDebugOutputStream(usbSerial);

  notecard.begin();


  // "newRequest()" uses the bundled "J" json package to allocate a "req",
  // which is a JSON object for the request to which we will then add Request
  // arguments.  The function allocates a "req" request structure using
  // malloc() and initializes its "req" field with the type of request.
  J *req = notecard.newRequest("hub.set");

  // This command (required) causes the data to be delivered to the Project
  // on notehub.io that has claimed this Product ID (see above).
  if (myProductID[0]) {
    JAddStringToObject(req, "product", myProductID);
  }

  // This command determines how often the Notecard connects to the service.
  // If "continuous", the Notecard immediately establishes a session with the
  // service at notehub.io, and keeps it active continuously. Due to the power
  // requirements of a continuous connection, a battery powered device would
  // instead only sample its sensors occasionally, and would only upload to
  // the service on a "periodic" basis.
  JAddStringToObject(req, "mode", "continuous");
  JAddNumberToObject(req, "inbound", 1);  // check notehub for any inbound notes every minute
  notecard.sendRequestWithRetry(req, 5);  // 5 seconds

  req = notecard.newRequest("card.attn");
  JAddStringToObject(req, "mode", "disarm");  // make the attn pin always HIGH at setup.
  notecard.sendRequestWithRetry(req, 5);      // 5 seconds

  //now arm it,
  req = notecard.newRequest("card.attn");
  JAddStringToObject(req, "mode", "arm, files");  // attn pin will now be low because I have armed it

  // when the blynk.qi file arrives it will make the attn pin HIGH
  J *files = JAddArrayToObject(req, "files");
  // for downstream from blynk, they are written to an inbound note blynk.qi
  JAddItemToArray(files, JCreateString("blynk.qi"));


  notecard.sendRequestWithRetry(req, 5);

#if USING_EXTERNAL_SIM
  req = notecard.newRequest("card.wireless");
  JAddStringToObject(
    req,
    "apn",
    "web.gprs.mtnnigeria.net"  // change this to your SIM access point name
  );


  /* set the mode you want your SIM to use
  *  for connectivity:
  *   "-" to reset to the default mode.
  *  "auto" to perform automatic band scan mode (this is the default mode).
  *  "m" to restrict the modem to Cat-M1.
  *  "nb" to restrict the modem to Cat-NB1.
  *  "gprs" to restrict the modem to EGPRS
  */
  JAddStringToObject(req, "mode", "gprs");  // only gprs worked in my region
#else
  req = NoteNewRequest("card.wireless");
  JAddStringToObject(
    req,
    "apn",
    "-"  // reverts to embedded SIM
  );


  /* set the mode you want your SIM to use
  *  for connectivity:
  *   "-" to reset to the default mode.
  *  "auto" to perform automatic band scan mode (this is the default mode).
  *  "m" to restrict the modem to Cat-M1.
  *  "nb" to restrict the modem to Cat-NB1.
  *  "gprs" to restrict the modem to EGPRS
  */
  JAddStringToObject(req, "mode", "-");  // reverts to embedded SIM
  notecard.sendRequestWithRetry(req, 5);
#endif
}

void loop() {

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;  // Update the timestamp for the last run

    debug("Custom Address:");
    debugln(pzem.readAddress(), HEX);

    // Read the data from the sensor
    float voltage = pzem.voltage();
    float current = pzem.current();
    float power = pzem.power();
    float energy = pzem.energy();
    float frequency = pzem.frequency();
    float pf = pzem.pf();
    //pzem.setPowerAlarm(4);


    // Check if the data is valid
    if (isnan(voltage)) {
      debugln("Error reading voltage");
    } else if (isnan(current)) {
      debugln("Error reading current");
    } else if (isnan(power)) {
      debugln("Error reading power");
    } else if (isnan(energy)) {
      debugln("Error reading energy");
    } else if (isnan(frequency)) {
      debugln("Error reading frequency");
    } else if (isnan(pf)) {
      debugln("Error reading power factor");
    } else {

      // Print the values to the Serial console
      debug("Voltage: ");
      debug(voltage);
      debugln("V");
      debug("Current: ");
      debug(current);
      debugln("A");
      debug("Power: ");
      debug(power);
      debugln("W");
      debug("Energy: ");
      debug(energy, 3);
      debugln("kWh");
      debug("Frequency: ");
      debug(frequency, 1);
      debugln("Hz");
      debug("PF: ");
      debugln(pf);
    }

    debugln();

    //post data to Notehub
    J *req = NoteNewRequest("note.add");
    JAddStringToObject(req, "file", "energy_readings.qo");
    JAddBoolToObject(req, "sync", true);

    J *body = JCreateObject();
    JAddNumberToObject(body, "Voltage", voltage);
    JAddNumberToObject(body, "Current", current);
    JAddNumberToObject(body, "Power", power);
    JAddNumberToObject(body, "Energy", energy);
    JAddItemToObject(req, "body", body);

    NoteRequest(req);
  }

  if (digitalRead(ATTN_PIN)) {
    debugln("blynk.qi has arrived. ATTN pin fired");

    J *req = NoteNewRequest("note.get");
    JAddStringToObject(req, "file", "blynk.qi");
    JAddBoolToObject(req, "delete", true);
    JAddBoolToObject(req, "sync", true);
    J *rsp = notecard.requestAndResponse(req);
    debugln(JConvertToJSONString(rsp));
    J *body = JGetObject(rsp, "body");
    bool load_state = JGetNumber(body, "Load");
    digitalWrite(RELAY, load_state);
    debug("load_state: ");
    debugln(load_state);

    EEPROM.write(0, load_state);  // aadr, value
    EEPROM.commit();
    if (load_state == 0) {
      //reset energy
      pzem.resetEnergy();
      debugln("energy has been reset");
    }


    //rearm
    debugln("rearming ATTN PIN.");
    req = NoteNewRequest("card.attn");
    JAddStringToObject(req, "mode", "arm");  // make the attn pin always HIGH at setup.
    notecard.sendRequestWithRetry(req, 5);   // 5 secondsd
    debugln("ATTN PIN is armed(LOW)");
  }
}
