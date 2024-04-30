#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include "debug.h"
#if __has_include(<config-private.h>)
    #include "config-private.h"
#else
    #include "config.h"
#endif
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>

// Specify number of Units (characters) in the display (4 - 12)
#define UNITCOUNT 12

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define WORDNIKURL "https://api.wordnik.com/v4/words.json/randomWord?hasDictionaryDef=true&excludePartOfSpeech=family-name%2Cgiven-name%2Cproper-noun%2Cproper-noun-plural&minCorpusCount=100&maxCorpusCount=-1&minDictionaryCount=1&maxDictionaryCount=-1&minLength=" STR(MINWORDLEN) "&maxLength=" STR(UNITCOUNT) "&api_key=" WORDNIKAPIKEY
#define NTP_MIN_VALID_EPOCH 1577836800  //2020-1-1
#define RTC_MAGIC 0x76b78ec4

void disableCertificates();
boolean synchroniseWith_NTP_Time(time_t &now, tm &timeinfo);
boolean getNTP(time_t &now, tm &timeinfo);
String wordOfTheDay();
void setup_routing();
void sendwebpage();
void receiveAPI();
void receiveInput();
void randomWord ();
void handle_NotFound();
String padToFullWidth (const char* word);

extern void displayString(String display);
