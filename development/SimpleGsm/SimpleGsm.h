#ifndef _SimpleGsm_H
#define _SimpleGsm_H

#include "Arduino.h"

#include <SoftwareSerial.h>

class SimpleGsm : public SoftwareSerial
{
  public:
    SimpleGsm (const byte rxPin, const byte txPin, const byte powerPin);

    bool begin (const long baudRate, const byte numberOfRetries);

    bool disableEcho ();

    bool setSmsTextMode ();

    bool sendSms (const String phoneNumber, const String message);

    bool missedCall (const String phoneNumber, const unsigned long ringingDuration);

    bool startCall (const String phoneNumber);

    bool callIsDialing ();

    bool callIsRinging ();

    void waitOnCallDialing ();

    void waitOnCallRinging (const unsigned long duration);

    bool hangCall ();

  private:
    void restart ();

    bool setEcho (const bool state);

    bool setSmsMode (const byte mode);

    void queryForCallStatus ();

    bool responseIsReceived (char * const pattern, const long timeOut);

    static char * OK_RESPONSE_FORMAT;

    byte _powerPin;
};

#endif /* _SimpleGsm_H */
