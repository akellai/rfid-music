/*
 Name:		ESP01.ino
 Created:	3/21/2017 4:54:55 PM
 Author:	ab
*/
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include "simpleserial.h"

#define WEB_TIMEOUT 120	// web config timeout in seconds
#define MAGIC 0xA5A5
WiFiUDP udp;

// Store those EEPROM
struct EEPROM_PARAMS {
	int magic = 0;			// Magic sequence AA55
	uint8 bssid[6];			// BSSID for faster connection
	bool bretry;
	// bool bfake;
} eepromParams;

bool send_to_pimusic(String id);
bool configure_wifi(bool bForce);
bool connect(bool binit);

// serial port
SimpleSerial *serial;

// the setup function runs once when you press reset or power the board
void setup() 
{
	COMMAND cmd;
	uint8_t *mem;

	//Serial.begin(115200);	// Initialize serial communications with Pro Mini
	Serial.begin(19200);	// Initialize serial communications with Pro Mini

	// read from EEPROM only if RTC fails
	mem = (uint8_t *)&eepromParams;
	EEPROM.begin(sizeof(eepromParams));
	for (size_t i = 0; i < sizeof(eepromParams); i++)
		*mem++ = EEPROM.read(i);
	
	while (!Serial);		// Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
	serial = new SimpleSerial(Serial);

	if (eepromParams.magic != MAGIC)  // first run - let's config
	{
		configure_wifi(true);
	}
	else
	{		
		serial->setTimeout(500);
		Serial.println("--INIT--");
		serial->sendCmd("u");	//report ready
		if (serial->recieveCmd(cmd)) {
			delay(1);
			if (cmd.cmd == "p") {
				serial->sendCmd("u");	// pong
			}
			else if (cmd.cmd == "i") { // i - init WiFi using web(force) autoconfig
				configure_wifi(true);
				if (WiFi.isConnected())
					serial->sendCmd("u");
				else
					serial->sendCmd("d");
			}
			else if (cmd.cmd == "c") { //  c - connect WiFi
				if (connect(true))	// this should renew dhcp and BSSID?
					serial->sendCmd("u");
				else
					serial->sendCmd("d");
			}
			else if (cmd.cmd == "r") {  // r - reconnect WiFi (fast)
				if (connect(false))
					serial->sendCmd("u");
				else
					serial->sendCmd("d");
			}
			else if (cmd.cmd == "t") {  // t:id - transmit ID to pimusic (t:XXYYZZFF)
				if (send_to_pimusic("music:" + cmd.param))
					serial->sendCmd("u");
				else
					serial->sendCmd("d");
			}
			else if (cmd.cmd == "b") {  // b:xx - transmit battery readings (b:XXXXXX), XXXXXX is ascii reading
				if (send_to_pimusic("battery:" + cmd.param))
					serial->sendCmd("u");
				else
					serial->sendCmd("d");
			}
		}
	}
	delay(1);
	serial->sendCmd("q");
	Serial.flush();
	ESP.deepSleep(0, RF_DEFAULT);
}

void loop() {
	// should never come here
}

void SaveEeprom()
{
	uint8_t *buf = (uint8_t *)&eepromParams;
	for (size_t i = 0; i < sizeof(eepromParams); i++)
		EEPROM.write(i, *buf++);
	EEPROM.commit();
}

bool configure_wifi(bool bForce) {
	WiFiManager wifiManager;

	wifiManager.setDebugOutput(false);

	if(bForce) wifiManager.resetSettings();

	wifiManager.setConfigPortalTimeout(WEB_TIMEOUT);
	if( !wifiManager.autoConnect("AutoConnectAP") )
		return(false);

	eepromParams.magic = MAGIC;
	eepromParams.bretry = true;
	SaveEeprom();
	return true;
}

bool connect(bool binit)
{
	bool bret = false;
	struct station_config conf;

	if (WiFi.isConnected())
		return true;

	WiFi.mode(WIFI_STA);
	WiFi.persistent(false);
	wifi_station_get_config(&conf);

	if (binit | eepromParams.bretry)
		eepromParams.bretry = true;

	WiFi.config(IPAddress(1, 2, 3, 4), IPAddress(1, 2, 3, 5), IPAddress(255, 255, 255, 0), IPAddress(1, 2, 3, 5));
	if (eepromParams.bretry)
	{
		WiFi.begin((char*)conf.ssid, (char*)conf.password, 0, NULL, true);
	}
	else
	{
		WiFi.begin((char*)conf.ssid, (char*)conf.password, 0, eepromParams.bssid, true);
	}

	for (int i = 0; i<50; i++) {
		if (WiFi.status() != WL_CONNECTED) {
			delay(100);
			Serial.print(".");
		}
	}

	bret = WiFi.isConnected();
	if (bret && eepromParams.bretry)
	{
		// store BSSID for successful connection
		memcpy((void *)&eepromParams.bssid[0], (void *)WiFi.BSSID(), 6);
		eepromParams.bretry = false;
		SaveEeprom();
	}
	else if ((!bret) && (!eepromParams.bretry))
	{
		eepromParams.bretry = true;
		SaveEeprom();
	}
	return(bret);
}

bool send_to_pimusic(String cmd)
{
	if (!connect(false))
		return false;

	udp.beginPacket(IPAddress(255, 255, 255, 255), 1234);
	udp.write((cmd + "\n").c_str());
	udp.endPacket();
	return true;
}
