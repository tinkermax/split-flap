#pragma once

// Set debug to 1 to allow debugging info on serial monitor
#define DEBUG 1

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#define debugf(...) Serial.printf(__VA_ARGS__)
#define TXT_RST "\e[0m"
#define TXT_BLUE "\e[0;34m"
#define TXT_YELLOW "\e[0;33m"
#define TXT_RED "\e[1;31m"
#define TXT_GREEN "\e[0;32m"
#else
#define debug(x)
#define debugln(x)
#define debugf(...)
#endif
