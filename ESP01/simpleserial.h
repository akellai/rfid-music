// simpleserial.h
/*
/*
MAGIC:CMD:X[:param]
u - status is up. NOTE: this is the only command that can have garbage in front of it. Any other command should in the spec or considered a failure
p - ping (expects 'u' in return)
i - Init WiFi using web (force) autoconfig
c - connect WiFi
r - reconnect WiFi (fast)
q - quit
t:id - transmit ID to pimusic (t:XXYYZZFF)
b:xx - transmit battery readings (b:XXXXXX), XXXXXX is ascii reading

example:
< MAGIC:u		// up
> MAGIC:t:00000000	// transmit id 00000000
< MAGIC:d		// u - success; d - failure
*/

#ifndef _SIMPLESERIAL_h
#define _SIMPLESERIAL_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

struct COMMAND {
	String cmd = "u";
	String param ="";
};

class SimpleSerial
{
private:
	const String m_delimiter = ":";
	const String m_magic = "MAGIC" + m_delimiter;

protected:
	 Stream &serial_;

 public:
	 SimpleSerial() : serial_(Serial) { };
	 SimpleSerial(Stream & serial) : serial_(serial) { }

	 void setTimeout(unsigned long timeout);
	 bool recieveCmd(COMMAND &cmd);
	 bool sendCmd(String cmd);
	 bool sendCmd(String cmd,String param);
};

#endif
