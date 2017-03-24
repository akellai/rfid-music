#include "simpleserial.h"

void SimpleSerial::setTimeout(unsigned long timeout)
{
	serial_.setTimeout(timeout);
}

bool SimpleSerial::recieveCmd(COMMAND & cmd)
{
	String s = serial_.readStringUntil('\r');
	if (s == NULL) return false;

	int start_pos = s.lastIndexOf(m_magic);
	if (start_pos < 0) return false;

	s = s.substring(start_pos + m_magic.length());

	start_pos = s.indexOf(m_delimiter);
	if (start_pos == 0)
		return false;	// cmd cannot be empty

	if (start_pos < 0)
	{
		cmd.cmd = s;
		return true;
	}
	cmd.cmd = s.substring(0, start_pos);
	cmd.param = s.substring(start_pos);
	cmd.param.replace(m_delimiter, "");
	return true;
}

bool SimpleSerial::sendCmd(String cmd)
{
	return(serial_.println(m_magic + cmd) > 0);
}

bool SimpleSerial::sendCmd(String cmd, String param)
{
	return(serial_.println(m_magic + cmd + m_delimiter + param) > 0);
}
