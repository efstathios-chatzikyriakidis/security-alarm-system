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

#include <FiniteStateMachine.h>
#include <ShiftRegLCD123.h>
#include <SoftwareSerial.h>
#include <SimpleGsm.h>
#include <SimpleList.h>
#include <NewTone.h>
#include <SPI.h>
#include <SD.h>

const byte DEFAULT_SLAVE_SELECT_PIN = 10;
const byte SD_SHIELD_CHIP_SELECT_PIN = 8;
const byte SHIFT_REGISTER_CLOCK_PIN = 9;
const byte SHIFT_REGISTER_DATA_PIN = 6;
const byte GSM_RECEIVER_PIN = 2;
const byte GSM_TRANSMITTER_PIN = 3;
const byte GSM_POWER_PIN = 7;
const byte DOOR_SENSOR_PIN = 4;
const byte PIEZO_BUZZER_PIN = 5;
const byte SIREN_PIN = A0;

const unsigned long DELAY_TIME_OF_CALL_RINGING_DURATION = 10000; // 10 seconds
const unsigned long DELAY_TIME_BEFORE_ENABLING_ALARM = 120000; // 2 minutes
const unsigned long DELAY_TIME_OF_CRITICAL_SECTION = 10000; // 10 seconds
const unsigned long DELAY_TIME_BEFORE_BEEPING_SOUNDS = 9000; // 9 seconds
const unsigned long DELAY_TIME_OF_RINGING_SIREN = 600000; // 10 minutes
const unsigned long DELAY_TIME_OF_LCD_MESSAGE = 2000; // 2 seconds

const byte MAXIMUM_NOTIFICATIONS_PER_USER = 3;

String smsText, encryptionKey;

State &
getDisabledState ()
{
  static State * object = NULL;

  if (object == NULL)
  {
    object = new State (disabledEnterOperation, disabledUpdateOperation, NULL);
  }

  return *object;
}

State &
getActivationPreparationState ()
{
  static State * object = NULL;

  if (object == NULL)
  {
    object = new State (activationPreparationEnterOperation, activationPreparationUpdateOperation, NULL);
  }

  return *object;
}

State &
getEnabledState ()
{
  static State * object = NULL;

  if (object == NULL)
  {
    object = new State (enabledEnterOperation, enabledUpdateOperation, NULL);
  }

  return *object;
}

State &
getPossibleThreatState ()
{
  static State * object = NULL;

  if (object == NULL)
  {
    object = new State (possibleThreatEnterOperation, possibleThreatUpdateOperation, NULL);
  }

  return *object;
}

State &
getRealThreatPartAState ()
{
  static State * object = NULL;

  if (object == NULL)
  {
    object = new State (realThreatPartAUpdateOperation);
  }

  return *object;
}

State &
getRealThreatPartBState ()
{
  static State * object = NULL;

  if (object == NULL)
  {
    object = new State (realThreatPartBUpdateOperation);
  }

  return *object;
}

FSM &
getFsm ()
{
  static FSM * object = NULL;

  if (object == NULL)
  {
    object = new FSM (getDisabledState());
  }

  return *object;
}

ShiftRegLCD123 &
getLcd ()
{
  static ShiftRegLCD123 * object = NULL;

  if (object == NULL)
  {
    object = new ShiftRegLCD123 (SHIFT_REGISTER_DATA_PIN, SHIFT_REGISTER_CLOCK_PIN, SRLCD123);
  }

  return *object;
}

SimpleGsm &
getGsm ()
{
  static SimpleGsm * object = NULL;

  if (object == NULL)
  {
    object = new SimpleGsm (GSM_RECEIVER_PIN, GSM_TRANSMITTER_PIN, GSM_POWER_PIN);
  }

  return *object;
}

SimpleList <String> &
getRfids ()
{
  static SimpleList <String> * object = NULL;

  if (object == NULL)
  {
    object = new SimpleList <String> ();
  }

  return *object;
}

SimpleList <String> &
getMobiles ()
{
  static SimpleList <String> * object = NULL;

  if (object == NULL)
  {
    object = new SimpleList <String> ();
  }

  return *object;
}

void
disabledEnterOperation ()
{
  sirenOff();

  printStringWithoutDelay(F("Alarm disabled"));
}

void
disabledUpdateOperation ()
{
  if (isAuthenticated ())
  {
    getFsm().transitionTo(getActivationPreparationState());
  }
}

void
activationPreparationEnterOperation ()
{
  printStringWithoutDelay(F("Preparing alarm"));
}

void
activationPreparationUpdateOperation()
{
  getFsm().transitionTo(getEnabledState());

  const unsigned long timeMark = millis();

  while(inTime(timeMark, DELAY_TIME_BEFORE_ENABLING_ALARM))
  {
    if (isAuthenticated ())
    {
      getFsm().transitionTo(getDisabledState());

      break;
    }
  }
}

void
enabledEnterOperation ()
{
  sirenOff();

  printStringWithoutDelay(F("Alarm enabled"));
}

void
enabledUpdateOperation ()
{
  if (doorIsOpen ())
  {
    getFsm().transitionTo(getPossibleThreatState());
  }

  if (isAuthenticated ())
  {
    getFsm().transitionTo(getDisabledState());
  }
}

void
possibleThreatEnterOperation ()
{
  printStringWithoutDelay(F("Possible threat"));
}

void
possibleThreatUpdateOperation ()
{
  if (authenticatedOnBeeping(DELAY_TIME_OF_CRITICAL_SECTION, DELAY_TIME_BEFORE_BEEPING_SOUNDS))
  {
    getFsm().transitionTo(getDisabledState());
  }
  else
  {
    getFsm().transitionTo(getRealThreatPartAState());
  }
}

void
realThreatPartAUpdateOperation ()
{
  sirenOn();

  printStringWithoutDelay(F("Notifying Users"));

  notifyUsers(DELAY_TIME_OF_CALL_RINGING_DURATION, MAXIMUM_NOTIFICATIONS_PER_USER);

  printStringWithoutDelay(F("Waiting police"));

  flushRfid();

  if (authenticatedOnDelay(DELAY_TIME_OF_RINGING_SIREN))
  {
    getFsm().transitionTo(getDisabledState());
  }
  else
  {
    getFsm().transitionTo(getRealThreatPartBState());
  }
}

void
realThreatPartBUpdateOperation ()
{
  if (doorIsClosed())
  {
    getFsm().transitionTo(getEnabledState());
  }

  if (isAuthenticated ())
  {
    getFsm().transitionTo(getDisabledState());
  }
}

bool
initializeSdCard ()
{
  pinMode (DEFAULT_SLAVE_SELECT_PIN, OUTPUT);

  pinMode (SD_SHIELD_CHIP_SELECT_PIN, OUTPUT);

  return SD.begin(SD_SHIELD_CHIP_SELECT_PIN);
}

void
initializeDoorSensor ()
{
  pinMode (DOOR_SENSOR_PIN, INPUT_PULLUP);
}

void
initializeLcd ()
{
  const byte numberOfColumns = 16;

  const byte numberOfRows = 2;

  getLcd().begin(numberOfColumns, numberOfRows);

  getLcd().backlightOff();

  getLcd().clear();
}

void
initializeRfid()
{
  const long baudRate = 9600;

  Serial.begin (baudRate);

  flushRfid();
}

bool
initializeGsm()
{
  const long baudRate = 9600;

  const byte numberOfRetries = 10;

  if (!getGsm().begin(baudRate, numberOfRetries))
  {
    return false;
  }

  if (!getGsm().disableEcho())
  {
    return false;
  }

  if (!getGsm().setSmsTextMode())
  {
    return false;
  }

  return true;
}

void
initializeSiren ()
{
  pinMode (SIREN_PIN, OUTPUT);

  sirenOff ();
}

bool
inTime (const unsigned long timeMark, const unsigned long timeInterval)
{
  const unsigned long currentTime = millis();

  const unsigned long elapsedTime = currentTime - timeMark;

  if (elapsedTime >= timeInterval)
  {
    return false;
  }

  return true;
}

void
sirenOn ()
{
  digitalWrite(SIREN_PIN, HIGH);
}

void
sirenOff ()
{
  digitalWrite(SIREN_PIN, LOW);
}

bool
doorIsOpen ()
{
  return doorHasValue (HIGH);
}

bool
doorIsClosed ()
{
  return ! doorIsOpen ();
}

bool
doorHasValue (const int expectedValue)
{
  if (digitalRead(DOOR_SENSOR_PIN) == expectedValue)
  {
    const byte numberOfSamples = 50;

    const unsigned long delayTimeBetweenSamples = 10;

    const byte thresholdPercentage = 85;

    byte actualPercentage = 0;

    for (byte i = 0; i < numberOfSamples; i++)
    {
      if (digitalRead(DOOR_SENSOR_PIN) == expectedValue)
      {
        actualPercentage++;
      }

      delay(delayTimeBetweenSamples);
    }

    actualPercentage *= (100.0 / numberOfSamples);

    if (actualPercentage >= thresholdPercentage)
    {
      return true;
    }
  }

  return false;
}

void
systemError (const __FlashStringHelper * string)
{
  printStringWithoutDelay(string);

  while (true) ;
}

void
printStringWithoutDelay (const __FlashStringHelper * string)
{
  getLcd().clear();

  getLcd().print(string);
}

void
printString (const __FlashStringHelper * string)
{
  printStringWithoutDelay(string);

  delay(DELAY_TIME_OF_LCD_MESSAGE);
}

void
flushRfid()
{
  while (Serial.available ())
  {
    Serial.read ();
  }
}

bool
authenticatedOnDelay (const unsigned long milliseconds)
{
  const unsigned long timeMark = millis();

  while (inTime(timeMark, milliseconds))
  {
    if (isAuthenticated ())
    {
      return true;
    }
  }

  return false;
}

bool
authenticatedOnBeeping (const unsigned long delayTimeOfCriticalSection, const unsigned long delayTimeBeforeBeepingSounds)
{
  const unsigned long timeMark = millis();

  while (inTime(timeMark, delayTimeOfCriticalSection))
  {
    if (isAuthenticated ())
    {
      return true;
    }

    if (!inTime(timeMark, delayTimeBeforeBeepingSounds))
    {
      generateBeepSound();
    }
  }

  return false;
}

bool
isAuthenticated ()
{
  String rfidCode;

  return (rfidTagHandled(rfidCode) && rfidCodeExists(rfidCode));
}

void
generateBeepSound ()
{
  const unsigned long toneFrequency = 500;

  const unsigned long durationTime = 50;

  NewTone(PIEZO_BUZZER_PIN, toneFrequency);

  delay(durationTime);

  noNewTone();

  delay(durationTime);
}

bool
rfidTagHandled (String & rfidCode)
{
  const byte rfidCodeSize = 10;

  byte code[6], value, checkSum, bytesRead, tempByte;

  value = checkSum = bytesRead = tempByte = 0;

  char codeRead[rfidCodeSize + 1] = { '\0' };

  if (Serial.available () > 0)
  {
    if (0x02 == (value = Serial.read ()))
    {
      while (bytesRead < (rfidCodeSize + 2))
      {
        if (Serial.available () > 0)
        {
          value = Serial.read ();

          if ((0x0D == value) || (0x0A == value) || (0x03 == value) || (0x02 == value))
          {
            break;
          }

          if (bytesRead < rfidCodeSize)
          {
            codeRead[bytesRead] = value;
          }

          if ((value >= '0') && (value <= '9'))
          {
            value = value - '0';
          }
          else if ((value >= 'A') && (value <= 'F'))
          {
            value = 10 + value - 'A';
          }

          if (bytesRead & 1 == 1)
          {
            code[bytesRead >> 1] = (value | (tempByte << 4));

            if (bytesRead >> 1 != 5)
            {
              checkSum ^= code[bytesRead >> 1];
            }
          }
          else
          {
            tempByte = value;
          }

          bytesRead++;
        }
      }

      if (bytesRead == (rfidCodeSize + 2))
      {
        if (code[5] == checkSum)
        {
          rfidCode = String(codeRead);

          return true;
        }
      }
    }
  }

  return false;
}

bool
readRfids()
{
  getRfids().clear();

  File file = SD.open("rfids.txt");

  if (file)
  {
    while (file.available())
    {
      getRfids().push_back(file.readStringUntil('\n'));
    }

    file.close();

    return true;
  }

  return false;
}

bool
readSmsText()
{
  File file = SD.open("message.txt");

  if (file)
  {
    if (file.available())
    {
      smsText = file.readStringUntil('\n');
    }

    file.close();

    return true;
  }

  return false;
}

bool
readMobiles()
{
  getMobiles().clear();

  File file = SD.open("mobiles.txt");

  if (file)
  {
    while (file.available())
    {
      getMobiles().push_back(file.readStringUntil('\n'));
    }

    file.close();

    return true;
  }

  return false;
}

bool
rfidCodeExists (const String & rfidCode)
{
  const String cipherText = xorEncryption (encryptionKey, rfidCode);

  for (SimpleList<String>::iterator element = getRfids().begin(); element != getRfids().end(); ++element)
  {
    if (cipherText == *element)
    {
      return true;
    }
  }

  return false;
}

void
notifyUsers (const unsigned long delayTimeOfCallRingingDuration, const byte maximumNotificationsPerUser)
{
  for (byte i = 0; i < maximumNotificationsPerUser; i++)
  {
    for (SimpleList<String>::iterator element = getMobiles().begin(); element != getMobiles().end(); ++element)
    {
      const String mobile = *element;

      getGsm().sendSms(mobile, smsText);
    }
  }

  for (byte i = 0; i < maximumNotificationsPerUser; i++)
  {
    for (SimpleList<String>::iterator element = getMobiles().begin(); element != getMobiles().end(); ++element)
    {
      const String mobile = *element;

      getGsm().missedCall(mobile, delayTimeOfCallRingingDuration);
    }
  }
}

void
readEncryptionKey ()
{
  while (!rfidTagHandled(encryptionKey)) ;
}

String
convertStringToHexadecimal (const char string[], const int length)
{
  const char symbols[] = "0123456789ABCDEF";

  String result;

  for (int i = 0; i < length; i++)
  {
    result += symbols[string[i] >> 4];

    result += symbols[string[i] & 0x0F];
  }

  return result;
}

String
xorEncryption (const String key, const String data)
{
  const int dataLength = data.length();

  const int bufferLength = dataLength + 1;

  char cipherText[bufferLength];

  data.toCharArray(cipherText, bufferLength);

  for (int i = 0; i < dataLength; i++)
  {
    cipherText[i] = cipherText[i] ^ key[i];
  }

  return convertStringToHexadecimal (cipherText, dataLength);
}

void
setup ()
{
  initializeSiren();

  initializeLcd();

  printString(F("Init door sensor"));

  initializeDoorSensor();

  printString(F("Init GSM"));

  if (!initializeGsm())
  {
    systemError (F("GSM failed"));
  }

  printString(F("Init SD card"));

  if (!initializeSdCard())
  {
    systemError (F("SD card failed"));
  }

  printString(F("Read RFIDs"));

  if (!readRfids())
  {
    systemError(F("RFIDs failed"));
  }

  printString(F("Read mobiles"));

  if (!readMobiles())
  {
    systemError(F("Mobiles failed"));
  }

  printString(F("Read SMS text"));

  if (!readSmsText())
  {
    systemError(F("SMS text failed"));
  }

  printString(F("Init RFID"));

  initializeRfid();

  printString(F("Read key"));

  readEncryptionKey();
}

void
loop ()
{
  getFsm().update();
}
