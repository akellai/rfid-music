/*
 Name:		ESP01.ino
 Created:	3/21/2017 4:54:55 PM
 Author:	ab
*/
#include <ESP8266WiFi.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <EEPROM.h>

#define RTC_BASE 64	// RTC memory: 64-512 range is userspace
#define WEB_TIMEOUT 120	// web config timeout in seconds

// Store those EPPROM
struct EPPROM_PARAMS {
	int magic = 0xABCD;		// Magic sequence
	IPAddress ip;			// ip params to speed up Wifi connection
	IPAddress gw;
	IPAddress subnet;
	IPAddress dns;
	char host[40];			// pimusic host that accepts requests http://pimusic/music.php?id=XXYYZZFF
} eppromParams;

bool send_to_pimusic(String id);
bool configure_wifi(bool bForce);

//flag for saving pimusic name
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback() {
	shouldSaveConfig = true;
}

// the setup function runs once when you press reset or power the board
void setup() 
{
	// wdt_disable();

	Serial.begin(115200);	// Initialize serial communications with Pro Mini
	while (!Serial) yield();		// Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
	Serial.println("ABCDEF");

	EEPROM.begin(sizeof(eppromParams));
	uint8_t *mem = (uint8_t *) &eppromParams;
	for (size_t i = 0; i < sizeof(eppromParams); i++)
	{
		*mem++ = EEPROM.read(i);
	}

	if (eppromParams.magic != 0xABCD)  // first run - let's config
	{
		// check if wifi needs to be configured
		strcpy(eppromParams.host, "pimusic.ab.lan");
		configure_wifi(true);
	}
	else
	{
		send_to_pimusic("00000000");
		Serial.println("Wifi is ready!");
	}

	if( !WiFi.isConnected() )
	{
		// report fatal error to ProMini
		Serial.println("error: Wifi is disconnected!");
	}
	else {
		Serial.println("Wifi is connected!");
	}

	Serial.flush();
	// delay(100);
	// go to the deep sleep untill MiniPro sends the reset
	ESP.deepSleep(0, RF_DEFAULT);
}

void loop() {
	// should never come here
	Serial.println("ping");
	delay(1000);
}

bool configure_wifi(bool bForce) {
	// test wifi connection
	WiFiManager wifiManager;
	wifiManager.setDebugOutput(false);
	bool bRet;

	if(bForce) wifiManager.resetSettings();

	WiFiManagerParameter custom_pimusic_server("pimusic", "music server", eppromParams.host, 40);
	wifiManager.setSaveConfigCallback(saveConfigCallback);
	wifiManager.addParameter(&custom_pimusic_server);

	wifiManager.setConfigPortalTimeout(WEB_TIMEOUT);
	bRet = wifiManager.autoConnect("AutoConnectAP");
	if (!bRet)
		return(bRet);

	eppromParams.ip = WiFi.localIP();
	eppromParams.gw = WiFi.gatewayIP();
	eppromParams.subnet = WiFi.subnetMask();
	eppromParams.dns = WiFi.dnsIP();
	strcpy(eppromParams.host, custom_pimusic_server.getValue());
	eppromParams.magic = 0xABCD;

	// check connection to pimusic
	WiFiClient client;
	const int httpPort = 80;
	if (!client.connect(eppromParams.host, httpPort)) {
		return false;
	}

	if (shouldSaveConfig)
	{
		uint8_t *buf = (uint8_t *)&eppromParams;
		for (size_t i = 0; i < sizeof(eppromParams); i++)
		{
			EEPROM.write(i, *buf++);
		}
		EEPROM.commit();
	}
	return(bRet);
}

bool send_to_pimusic(String id)
{
	const uint8_t bssid[6] = { 0x80,0x2A,0xA8,0xD1,0x5E,0x6F };

	WiFi.config(eppromParams.ip, eppromParams.gw, eppromParams.subnet, eppromParams.dns);
	WiFi.mode(WIFI_STA);
	//WiFi.begin("ABLUCERNE", "veeam123", 1, bssid, true);
	//WiFi.begin();
	WiFi.reconnect();

	for (int i = 0; i<30; i++) {
		if (WiFi.status() != WL_CONNECTED) {
			delay(100);
			Serial.print(".");
		}
	}
	if (WiFi.status() != WL_CONNECTED)
	{
		return false;
	}

	//Serial.println("WiFi connected");
	//Serial.println("IP address: ");
	//Serial.println(WiFi.localIP());
	//Serial.println(WiFi.BSSIDstr());

	// Use WiFiClient class to create TCP connections
	WiFiClient client;
	const int httpPort = 80;
	if (!client.connect(eppromParams.host, httpPort)) {
		return false;
	}

	// We now create a URI for the request
	String url = "/music.php?id=" + id;

	// This will send the request to the server
	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
		"Host: " + eppromParams.host + "\r\n" +
		"Connection: close\r\n\r\n");
	return true;
}
