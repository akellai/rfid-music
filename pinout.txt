Using MFRC522 with Arduino Pro Mini and ESP-01 in a power save mode. The code is provided as a working example based on the discussion here https://github.com/miguelbalboa/rfid/issues/269

Parts (all none originals from aliexpress):
MFRC522
Arduino Pro Mini, 3.5V
ESP-01 ESP8266

Arduino <-> MFRC522

* ---------------------------------
*             MFRC522      Arduino 
*             Reader/PCD   Pro/Mini
* Signal      Pin          Pin     
* ---------------------------------
* RST/Reset   RST          9       
* SPI SS      SDA(SS)      10      
* SPI MOSI    MOSI         11
* SPI MISO    MISO         12
* SPI SCK     SCK          13
* Power       3.3V        VCC
*/

Arduino <-> ESP-01
* ---------------------------------
*             ESP-01       Arduino 
*                          Pro/Mini
* Signal      Pin          Pin     
* ---------------------------------
* RST/Reset   RESET        3       
* TX          TXD          6     
* RX          RXD          5
* Power       VCC          VCC
*/

Arduino other pins:
RST -> External button switch
2   -> Low current led through 1 K
RAW -< 3 AA batteries (no extra step up/step down regulator is used)

A couple fo comments:
1. External reset switch is used during the initial setup: If a card is present in the reader and reset button is pressed,
ESP-01 becomes WIFI AP. Admin can connect to it and configure WiFi connection parameters
2. Only card ID is used to take an external action (in my case - play particular Sonos playlist based on the card ID)
3. Security is not a concern
4. On the linuz side the following script can be used to read UDP packets:

#!/bin/bash
LOGFILE="/tmp/udp-sonos.log"

function log
{
    echo -e "\"$(date '+%Y-%m-%d %H:%M:%S'\")\t$@">>$LOGFILE
}

while read -r udp_cmd ; do
    PARAM=${udp_cmd##*:}
	# attempt to improve security a little bit by not allowing udp packets with escape caracters and alike
	# to avoid code injection
    if [[ $udp_cmd =~ ^[A-Za-z0-9:]*$ ]] ; then
        log $udp_cmd
        if [[ $udp_cmd == music:* ]] ; then
            /home/pi/auto/sonos.sh /home/pi/ids/${PARAM}
        fi
    else
        echo 'parsing error'
    fi
done< <(exec  socat - udp4-listen:1234,reuseaddr,fork)
