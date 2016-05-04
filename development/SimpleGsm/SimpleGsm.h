/*
 *  Copyright (C) 2015  Efstathios Chatzikyriakidis (contact@efxa.org)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

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

    static char * const OK_RESPONSE_FORMAT;

    byte _powerPin;
};

#endif /* _SimpleGsm_H */