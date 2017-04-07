/*
 Name:		ESP01_test.ino
 Created:	4/6/2017 7:33:26 PM
 Author:	ab
*/

#include <ESP8266WiFi.h>
#include <udp.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <EEPROM.h>

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

WiFiUDP udp;

// the setup function runs once when you press reset or power the board
void setup()
{
	uint8_t *mem = (uint8_t *)&eepromParams;

	Serial.begin(115200);	// Initialize serial communications with Pro Mini
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
}

void broadcast_cmd(String cmd)
{
	udp.beginPacket(IPAddress(255,255,255,255), 1234);
	udp.write(cmd.c_str());
	udp.endPacket();
}

void loop() {
	Serial.println("start");
	//send_to_pimusic("/music.php?id=none");
	connect(false);
	broadcast_cmd("test");
	Serial.println("!");
	delay(10000);
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

	if (bForce) wifiManager.resetSettings();

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

	WiFi.config(IPAddress(1, 2, 3, 4), IPAddress(1, 2, 3, 5), IPAddress(255, 255, 255, 0), IPAddress(1, 2, 3, 5));
	if (eepromParams.bretry)
	{
		WiFi.begin((char*)conf.ssid, (char*)conf.password, 0, NULL, true);
	}
	else
	{
		WiFi.begin();
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
