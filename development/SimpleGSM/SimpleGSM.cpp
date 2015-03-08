#include "SimpleGSM.h"

SimpleGSM::SimpleGSM (const byte rxPin, const byte txPin, const byte powerPin) : SoftwareSerial (rxPin, txPin)
{
  _powerPin = powerPin;

  pinMode(_powerPin, OUTPUT);
}

void
SimpleGSM::restart ()
{
  digitalWrite(_powerPin, HIGH);

  delay(12000);

  digitalWrite(_powerPin, LOW);

  delay(1000);
}

bool
SimpleGSM::responseIsReceived (char * const pattern, const long timeOut)
{
  this->setTimeout(timeOut);

  return this->find(pattern);
}

bool
SimpleGSM::disableEcho ()
{
  return this->setEcho (false);
}

bool
SimpleGSM::setSMSTextMode ()
{
  return this->setSMSMode (1);
}

bool
SimpleGSM::setEcho (const bool state)
{
  this->print(F("ATE"));

  this->print(state);

  this->print(F("\r"));

  return this->responseIsReceived(OK_RESPONSE_FORMAT, 2000);
}

bool
SimpleGSM::setSMSMode (const byte mode)
{
  this->print(F("AT+CMGF="));

  this->print(mode);

  this->print(F("\r"));

  return this->responseIsReceived(OK_RESPONSE_FORMAT, 2000);
}

bool
SimpleGSM::begin (const long baudRate, const byte numberOfRetries)
{
  SoftwareSerial::begin (baudRate);

  for (byte i = 0; i < numberOfRetries; i++)
  {
    this->restart ();

    delay(1000);

    this->print(F("AT\r"));

    if (this->responseIsReceived(OK_RESPONSE_FORMAT, 2000))
    {
      return true;
    }
  }

  return false;
}

bool
SimpleGSM::sendSMS (const String phoneNumber, const String message)
{
  this->print(F("AT+CMGS=\""));

  this->print(phoneNumber);

  this->print(F("\"\r"));

  if (!this->responseIsReceived("\r\n> ", 4000))
  {
    return false;
  }

  this->print(message);

  this->write(0x1A);

  return this->responseIsReceived(OK_RESPONSE_FORMAT, 8000);
}

bool
SimpleGSM::startCall (const String phoneNumber)
{
  this->print(F("ATD"));

  this->print(phoneNumber);

  this->print(F(";\r"));

  return this->responseIsReceived(OK_RESPONSE_FORMAT, 2000);
}

bool
SimpleGSM::callRings(const unsigned long timeOut)
{
  const unsigned long previousMillis = millis();

  while ((unsigned long) (millis() - previousMillis) < timeOut)
  {
    this->print(F("AT+CLCC\r"));

    if (this->responseIsReceived("+CLCC: 1,0,3,0,0", 300))
    {
      return true;
    }
  }

  return false;
}

bool
SimpleGSM::hangCall ()
{
  this->print(F("ATH\r"));

  return this->responseIsReceived(OK_RESPONSE_FORMAT, 2000);
}

char * SimpleGSM::OK_RESPONSE_FORMAT = "\r\nOK\r\n";
