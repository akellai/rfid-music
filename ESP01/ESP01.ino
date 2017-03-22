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

// Store those in RTC as this is faster than EPPROM
struct RTC_PARAMS {
	int rtcStore = 0xABCD;	// Magic sequence
	IPAddress ip;			// ip params to speed up Wifi connection
	IPAddress gw;
	IPAddress subnet;
	IPAddress dns;
	char host[40];			// pimusic host that accepts requests http://pimusic/music.php?id=XXYYZZFF
} rtcParams;

void send_to_pimusic(String id);
bool configure_wifi(bool bForce);

//flag for saving pimusic name
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback() {
	shouldSaveConfig = true;
}

// the setup function runs once when you press reset or power the board
void setup() {
	Serial.begin(115200);	// Initialize serial communications with Pro Mini
	while (!Serial);		// Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
//	return;
	system_rtc_mem_read(RTC_BASE, &rtcParams, sizeof(rtcParams));
	if (rtcParams.rtcStore != 0xABCD)  // cold start, magic number is nor present
	{
		// check if wifi needs to be configured
		configure_wifi(false);
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
	Serial.flush();
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
	bool bRet;

	if(bForce) wifiManager.resetSettings();

	// add pimusic parameter
	EEPROM.begin(sizeof(rtcParams.host));
	for (size_t i = 0; i <= sizeof(rtcParams.host); i++)
	{
		rtcParams.host[i] = EEPROM.read(i);
		if (rtcParams.host[i] == 0) break;
		if (!isAscii(rtcParams.host[i]))
		{
			rtcParams.host[i] = 0;
			break;
		}
	}
	if(rtcParams.host[0]==0) strcpy(rtcParams.host, "pimusic.ab.lan");

	WiFiManagerParameter custom_pimusic_server("pimusic", "music server", rtcParams.host, 40);
	wifiManager.setSaveConfigCallback(saveConfigCallback);
	wifiManager.addParameter(&custom_pimusic_server);

	wifiManager.setConfigPortalTimeout(WEB_TIMEOUT);
	bRet = wifiManager.autoConnect("AutoConnectAP");
	if (!bRet)
		return(bRet);

	rtcParams.ip = WiFi.localIP();
	rtcParams.gw = WiFi.gatewayIP();
	rtcParams.subnet = WiFi.subnetMask();
	rtcParams.dns = WiFi.dnsIP();
	strcpy(rtcParams.host, custom_pimusic_server.getValue());
	rtcParams.rtcStore = 0xABCD;
	system_rtc_mem_write(RTC_BASE, &rtcParams, sizeof(rtcParams));

	if (shouldSaveConfig)
	{
		for (size_t i = 0; i <= sizeof(rtcParams.host); i++)
		{
			EEPROM.write(i, (uint8_t)(rtcParams.host[i]));
		}
		EEPROM.commit();
	}
	return(bRet);
}

void send_to_pimusic(String id)
{
	const uint8_t bssid[] = { 0x80,0x2A,0xA8,0xD1,0x5E,0x6F };
	Serial.begin(115200);	// Initialize serial communications with the PC
	while (!Serial);		// Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
	Serial.println("");
	WiFi.mode(WIFI_STA);

	Serial.print("host: ");
	Serial.println(rtcParams.host);

	WiFi.config(rtcParams.ip, rtcParams.gw, rtcParams.subnet, rtcParams.dns);
	//WiFi.begin(ssid, password, 1, bssid, true);
	WiFi.begin();

	for (int i = 0; i<30; i++) {
		if (WiFi.status() != WL_CONNECTED) {
			delay(100);
			Serial.print(".");
		}
	}
	if (WiFi.status() != WL_CONNECTED)
	{
		return;
	}
	//WiFi.disconnect();
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
	Serial.println(WiFi.BSSIDstr());

	// Use WiFiClient class to create TCP connections
	WiFiClient client;
	const int httpPort = 80;
	if (!client.connect(rtcParams.host, httpPort)) {
		Serial.println("connection failed");
		return;
	}

	// We now create a URI for the request
	String url = "/music.php?id=" + id;
	//Serial.print("Requesting URL: ");
	//Serial.println(url);

	// This will send the request to the server
	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
		"Host: " + rtcParams.host + "\r\n" +
		"Connection: close\r\n\r\n");
	//WiFi.disconnect();
}
