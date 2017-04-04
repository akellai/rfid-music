/*
 Name:		ESP01.ino
 Created:	3/21/2017 4:54:55 PM
 Author:	ab
*/
#include "simpleserial.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <EEPROM.h>

#include "simpleserial.h"

#define RTC_BASE 64	// RTC memory: 64-512 range is userspace
#define WEB_TIMEOUT 120	// web config timeout in seconds

// Store those EEPROM
struct EEPROM_PARAMS {
	int magic = 0xABCD;		// Magic sequence AA55
	IPAddress ip;			// ip params to speed up Wifi connection
	IPAddress gw;
	IPAddress subnet;
	IPAddress dns;
	char host[40];			// pimusic host that accepts requests http://pimusic/music.php?id=XXYYZZFF
	uint8 bssid[6];			// BSSID for faster connection
	bool bretry = false;
} eepromParams;

bool send_to_pimusic(String id);
bool configure_wifi(bool bForce);
bool connect(bool binit);

// serial port
SimpleSerial *serial;

//flag for saving pimusic name
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback() {
	shouldSaveConfig = true;
}

// the setup function runs once when you press reset or power the board
void setup() 
{
	COMMAND cmd;
	uint8_t *mem = (uint8_t *)&eepromParams;

	Serial.begin(19200);	// Initialize serial communications with Pro Mini
	EEPROM.begin(sizeof(eepromParams));
	for (size_t i = 0; i < sizeof(eepromParams); i++)
	{
		*mem++ = EEPROM.read(i);
	}

	if (eepromParams.magic != 0xABCD)  // first run - let's config
	{
		// check if wifi needs to be configured
		strcpy(eepromParams.host, "pimusic.ab.lan");
		configure_wifi(true);
	}
	else
	{
		serial = new SimpleSerial(Serial);
		while (!Serial) delay(1);		// Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
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
				if (send_to_pimusic("/music.php?id=" + cmd.param))
					serial->sendCmd("u");
				else
					serial->sendCmd("d");
			}
			else if (cmd.cmd == "b") {  // b:xx - transmit battery readings (b:XXXXXX), XXXXXX is ascii reading
				if (send_to_pimusic("/battery.php?id=" + cmd.param))
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
	Serial.println("ping");
	delay(1000);
}

void SaveEeprom()
{
	uint8_t *buf = (uint8_t *)&eepromParams;
	for (size_t i = 0; i < sizeof(eepromParams); i++)
	{
		EEPROM.write(i, *buf++);
	}
	EEPROM.commit();
}

bool configure_wifi(bool bForce) {
	WiFiManager wifiManager;
	bool bRet;
	WiFiClient client;

	wifiManager.setDebugOutput(false);

	if(bForce) wifiManager.resetSettings();

	WiFiManagerParameter custom_pimusic_server("pimusic", "music server", eepromParams.host, 40);
	//wifiManager.setSaveConfigCallback(saveConfigCallback);
	wifiManager.addParameter(&custom_pimusic_server);

	wifiManager.setConfigPortalTimeout(WEB_TIMEOUT);
	bRet = wifiManager.autoConnect("AutoConnectAP");
	if (!bRet)
		return(bRet);

	eepromParams.ip = WiFi.localIP();
	eepromParams.gw = WiFi.gatewayIP();
	eepromParams.subnet = WiFi.subnetMask();
	eepromParams.dns = WiFi.dnsIP();
	strcpy(eepromParams.host, custom_pimusic_server.getValue());
	eepromParams.magic = 0xABCD;
	eepromParams.bretry = true;

	// check connection to pimusic
	if (!client.connect(eepromParams.host, 80)) {
		return false;
	}

	SaveEeprom();
	return bRet;
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

	if (eepromParams.bretry)
	{
		WiFi.begin((char*)conf.ssid, (char*)conf.password, 0, NULL, true);
	}
	else
	{
		WiFi.config(eepromParams.ip, eepromParams.gw, eepromParams.subnet, eepromParams.dns);
		WiFi.begin((char*)conf.ssid, (char*)conf.password, 0, eepromParams.bssid, true);
	}

	for (int i = 0; i<40; i++) {
		if (WiFi.status() != WL_CONNECTED) {
			delay(100);
			Serial.print(".");
		}
	}

	bret = WiFi.isConnected();
	if (bret)
	{
		if (eepromParams.bretry)
		{
			// store BSSID for successful connection
			memcpy((void *)&eepromParams.bssid[0], (void *)WiFi.BSSID(), 6);
			eepromParams.ip = WiFi.localIP();
			eepromParams.gw = WiFi.gatewayIP();
			eepromParams.subnet = WiFi.subnetMask();
			eepromParams.dns = WiFi.dnsIP();
			eepromParams.bretry = false;
			SaveEeprom();
		}
	}
	else
	{
		if (!eepromParams.bretry)
		{
			eepromParams.bretry = true;
			SaveEeprom();
		}
	}

	return(bret);
}

bool send_to_pimusic(String url)
{
	// Use WiFiClient class to create TCP connections
	WiFiClient client;

	if (!connect(false))
		return false;

	if (!client.connect(eepromParams.host, 80)) {
		return false;
	}

	// This will send the request to the server
	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
		"Host: " + eepromParams.host + "\r\n" +
		"Connection: close\r\n\r\n");
	return true;
}
