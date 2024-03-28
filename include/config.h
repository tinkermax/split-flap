#pragma once

#define WIFI_SSID ""
#define WIFI_PWD ""

#define MY_NTP_SERVER "au.pool.ntp.org" // Set the best fitting NTP server (pool) for your location
#define MY_TZ "AEST-10AEDT,M10.1.0,M4.1.0/3" // Set your time zone from https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

#define WORDNIKAPIKEY "" // Insert your private key from https://developer.wordnik.com/
#define MINWORDLEN 9 // Specify minimum word length to fetch from Wordnik
#define WORDUPDATESPERHOUR 1 // Set number of word updates from https://wordnik.com per hour. 0 will disable, else use an integer that results in an exact number of minutes btw updates eg. 1,2,3,4,5,6,10,12...
