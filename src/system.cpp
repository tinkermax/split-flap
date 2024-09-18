#include "system.h"

const char* word_server = "api.wordnik.com";  // word server
WiFiClientSecure client;
WebServer server(80);

// HTML web page to handle input of text to display
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Split-Flap Display</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <form action="receiveInput" method="POST">
    <p style="font-weight: bold; margin-bottom:12px;">Split-Flap Display</p>
    <input type="text" id="displaytext" name="displaytext">
    <input type="submit" value="Update" id="submitbtn" onclick="this.hidden=true; document.getElementById('submitrnd').hidden = true"><br/><br/>
  </form>
  <form action="randomWord" method="POST">
    <input type="submit" value="Random Word" id="submitrnd" onclick="document.getElementById('displaytext').value = '***RANDOM***'; this.hidden=true; document.getElementById('submitbtn').hidden = true">
  </form>
  </body></html>)rawliteral";

void disableCertificates() {
    client.setInsecure();
}

boolean synchroniseWith_NTP_Time(time_t &now, tm &timeinfo){
  uint8_t timeout = 0;
  
  // debug("Setting time using SNTP");
  while (now < NTP_MIN_VALID_EPOCH && ++timeout < 50) {
    delay(100);
    // debug(".");
    now = time(nullptr);
  }

  if (timeout >= 50) {
    return false;
  }

  localtime_r(&now, &timeinfo); // update the structure tm (timeinfo) with the current time
  
  return true;
}

boolean getNTP(time_t &now, tm &timeinfo) {
  // If cannot get NTP time, return error
  if (!synchroniseWith_NTP_Time(now, timeinfo)) {
    debugln("Error getting time");
    return false;
  }
  else {
    time(&now); // read the current time
    localtime_r(&now, &timeinfo); // update the structure tm with the local current time
  }
  return true;
}

String wordOfTheDay() {
    JsonDocument jsonBufferData;
    String json_word;

    if (WORDNIKAPIKEY != "") {
      // debugln("\nStarting connection to word server...");
      if (!client.connect(word_server, 443)) {
          debugln("Connection failed!");
          return "";
      }
      else {
          // debugln("Connected to word_server!");
          // Make a HTTP request:
          client.println("GET " WORDNIKURL " HTTP/1.0");
          client.println("Host: api.wordnik.com");
          client.println("Connection: close");
          client.println();

          while (client.connected()) {
          String line = client.readStringUntil('\n');
          if (line == "\r") {
              // debugln("headers received");
              break;
          }
          }
          // if there are incoming bytes available
          // from the word_server, read them and print them:
          while (client.available()) {
          char c = client.read();
          json_word += c;
          }
          
          client.stop();
      }

      //Parse JSON to get word
      DeserializationError jsonError = deserializeJson(jsonBufferData, json_word);

      // Test if parsing succeeds
      if (jsonError) {
          debug(F("deserializeJson() failed: "));
          debugln(jsonError.f_str());
          return "";
      }

      const char* word = jsonBufferData["word"];
      return padToFullWidth(word);
    }
    else {
      debugln("WORDNIKAPIKEY not specified in config.h");
      return("");
    }

}

void setup_routing() {     
  // Send web page with input fields to client
  server.on("/", HTTP_GET, sendwebpage);  
  server.on("/display", HTTP_POST, receiveAPI);    
  server.on("/receiveInput", HTTP_POST, receiveInput);    
  server.on("/randomWord", HTTP_POST, randomWord);    
  
  server.onNotFound(handle_NotFound);

  server.begin();    

  // Setup mDNS to allow connection via hostname
  if (!MDNS.begin(NETWORKNAME)) {   // Set the hostname NETWORKNAME defined in system.h: by default "splitflap.local"
    debugln("Error setting up MDNS responder!");
  }

  MDNS.addService("http", "tcp", 80);
  MDNS.addServiceTxt("http", "tcp", NETWORKNAME, "1");
}

void sendwebpage() {
  server.send(200, "text/html", index_html);
}

void receiveAPI() {
  JsonDocument jsonBufferData;
  const char* displaytext;
  String body = server.arg("plain");
  
  DeserializationError jsonError = deserializeJson(jsonBufferData, body);

  // Test if parsing succeeds
  if (jsonError) {
      debug(F("deserializeJson() failed: "));
      debugln(jsonError.f_str());
      return;
  }

  displaytext = jsonBufferData["displaytext"];

  debugf("Text to display from API: %s\n", padToFullWidth (displaytext));
  displayString(padToFullWidth (displaytext));

  server.send(200, "application/json", "{}");
}

void receiveInput() {
  JsonDocument jsonBufferData;
  const char* displaytext;
  String inputText = server.arg("displaytext");

  displaytext = inputText.c_str();

  // server.send(200, "text/plain", "{}");
  server.sendHeader("Location", "/",true);  
  server.send(302, "text/plain", "");  

  delay(500);
  displayString(padToFullWidth (displaytext));
}


void randomWord () {
  String word = wordOfTheDay();
  debugf("Word, [%s]\n", word);

  server.sendHeader("Location", "/",true);  
  server.send(302, "text/plain", "");

  delay(500);
  displayString(word);
}

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

String padToFullWidth (const char* word) {
  String word_fullwidth = word;

  if (word_fullwidth.length() <= UNITCOUNT - 2) {
      word_fullwidth = " " + word_fullwidth;
  }

  while (word_fullwidth.length() < UNITCOUNT) {
      word_fullwidth += " ";
  }

  return word_fullwidth;
}