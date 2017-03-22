/*
Name:		rfid_arduino_nano.ino
Arduino Pro Mini w/ATmega328 3.3V 8 Mhz
Created:	3/21/2017 12:03:49 AM
Author:	ab
*/

/*
* --------------------------------------------------------------------------------------------------------------------
* Example sketch/program showing how to read data from a PICC to serial.
* --------------------------------------------------------------------------------------------------------------------
* This is a MFRC522 library example; for further details and other examples see: https://github.com/miguelbalboa/rfid
*
* Example sketch/program showing how to read data from a PICC (that is: a RFID Tag or Card) using a MFRC522 based RFID
* Reader on the Arduino SPI interface.
*
* When the Arduino and the MFRC522 module are connected (see the pin layout below), load this sketch into Arduino IDE
* then verify/compile and upload it. To see the output: use Tools, Serial Monitor of the IDE (hit Ctrl+Shft+M). When
* you present a PICC (that is: a RFID Tag or Card) at reading distance of the MFRC522 Reader/PCD, the serial output
* will show the ID/UID, type and any data blocks it can read. Note: you may see "Timeout in communication" messages
* when removing the PICC from reading distance too early.
*
* If your reader supports it, this sketch/program will read all the PICCs presented (that is: multiple tag reading).
* So if you stack two or more PICCs on top of each other and present them to the reader, it will first output all
* details of the first and then the next PICC. Note that this may take some time as all data blocks are dumped, so
* keep the PICCs at reading distance until complete.
*
* @license Released into the public domain.
*
* Typical pin layout used:
* -----------------------------------------------------------------------------------------
*             MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
*             Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
* Signal      Pin          Pin           Pin       Pin        Pin              Pin
* -----------------------------------------------------------------------------------------
* RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
* SPI SS      SDA(SS)      10            53        D10        10               10
* SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
* SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
* SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15
*/

// NOTE: pinout is Arduino Nano v3!!!

#include <LowPower.h>
#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN         9          // Configurable, see typical pin layout above
#define SS_PIN          10         // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

void setup() {
	Serial.begin(115200);		// Initialize serial communications with the PC
	while (!Serial);		// Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
	SPI.begin();			// Init SPI bus
	pinMode(SS_PIN, OUTPUT);
	digitalWrite(SS_PIN, HIGH);

	pinMode(RST_PIN, OUTPUT);
	digitalWrite(RST_PIN, HIGH);

	//mfrc522.PCD_Reset();
	mfrc522.PCD_WriteRegister(mfrc522.TModeReg, 0x80);			// TAuto=1; timer starts automatically at the end of the transmission in all communication modes at all speeds
	mfrc522.PCD_WriteRegister(mfrc522.TPrescalerReg, 0x43);		// 10μs.
	mfrc522.PCD_WriteRegister(mfrc522.TReloadRegH, 0x03);		// Reload timer with 0x3E8 = 1000, ie 25ms before timeout.
	mfrc522.PCD_WriteRegister(mfrc522.TReloadRegL, 0xE8);

	mfrc522.PCD_WriteRegister(mfrc522.TxASKReg, 0x40);		// Default 0x00. Force a 100 % ASK modulation independent of the ModGsPReg register setting
	mfrc522.PCD_WriteRegister(mfrc522.ModeReg, 0x3D);		// Default 0x3F. Set the preset value for the CRC coprocessor for the CalcCRC command to 0x6363 (ISO 14443-3 part 6.2.4)

	mfrc522.PCD_AntennaOn();						// Enable the antenna driver pins TX1 and TX2 (they were disabled by the reset)

	delay(3);
	if (!mfrc522.PICC_IsNewCardPresent()) {
		// put NFC to sleep
		mfrc522.PCD_WriteRegister(mfrc522.CommandReg, mfrc522.PCD_NoCmdChange | 0x10);
	}
	else
	{
		bool status = mfrc522.PICC_ReadCardSerial();
		mfrc522.PCD_WriteRegister(mfrc522.CommandReg, mfrc522.PCD_NoCmdChange | 0x10);
		if (status)
		{
			Serial.println("ready...");
			// Now a card is selected. The UID and SAK is in mfrc522.uid.
			// Dump UID
			Serial.print(F("Card UID:"));
			String id = "";
			for (byte i = 0; i < mfrc522.uid.size; i++) {
				id = id + String(mfrc522.uid.uidByte[i], HEX);
			}
			Serial.print(id);
		}
		//Serial.println();
	}
}

void loop() {
	// Enter power down state for 1 s with ADC and BOD module disabled
//	LowPower.powerDown(SLEEP_60MS, ADC_OFF, BOD_OFF);
	LowPower.powerDown(SLEEP_60MS, ADC_OFF, BOD_OFF);

	// Look for new cards
	if (!mfrc522.PICC_IsNewCardPresent()) {
		return;
	}

	// Select one of the cards
	if (mfrc522.PICC_ReadCardSerial()) {
		// Dump debug info about the card; PICC_HaltA() is automatically called
		mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
	}
}