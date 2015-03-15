#include <FiniteStateMachine.h>
#include <ShiftRegLCD123.h>
#include <SoftwareSerial.h>
#include <SimpleGsm.h>
#include <LinkedList.h>
#include <SPI.h>
#include <SD.h>

const byte DEFAULT_SLAVE_SELECT_PIN = 10;
const byte SD_SHIELD_CHIP_SELECT_PIN = 8;
const byte SHIFT_REGISTER_CLOCK_PIN = 9;
const byte SHIFT_REGISTER_DATA_PIN = 6;
const byte GSM_RECEIVER_LINE_PIN = 2;
const byte GSM_TRANSMITTER_LINE_PIN = 3;
const byte GSM_POWER_PIN = 7;
const byte DOOR_SENSOR_PIN = A3;
const byte PIR_SENSOR_PIN = A1;
const byte SIREN_PIN = A0;

const unsigned long DELAY_TIME_OF_PIR_SENSOR_CALIBRATION = 40000; // 40 seconds
const unsigned long DELAY_TIME_OF_CALL_RINGING_DURATION = 10000; // 10 seconds
const unsigned long DELAY_TIME_WHILE_ENABLING_ALARM = 120000; // 2 minutes
const unsigned long DELAY_TIME_OF_CRITICAL_SECTION = 15000; // 15 seconds
const unsigned long DELAY_TIME_OF_RINGING_SIREN = 900000; // 15 minutes
const unsigned long DELAY_TIME_OF_LCD_MESSAGE = 2000; // 2 seconds

String smsText, encryptionKey;

State &
getDisabledState ()
{
  static State * object = NULL;

  if (object == NULL)
  {
    object = new State (disabledEnterOperation, disabledUpdateOperation, disabledExitOperation);
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
getRealThreatState ()
{
  static State * object = NULL;

  if (object == NULL)
  {
    object = new State (realThreatUpdateOperation);
  }

  return *object;
}

FSM &
getSystemFsm ()
{
  static FSM * object = NULL;

  if (object == NULL)
  {
    object = new FSM (getDisabledState());
  }

  return *object;
}

ShiftRegLCD123 &
getSystemLcd ()
{
  static ShiftRegLCD123 * object = NULL;

  if (object == NULL)
  {
    object = new ShiftRegLCD123 (SHIFT_REGISTER_DATA_PIN, SHIFT_REGISTER_CLOCK_PIN, SRLCD123);
  }

  return *object;
}

SimpleGsm &
getSystemGsm ()
{
  static SimpleGsm * object = NULL;

  if (object == NULL)
  {
    object = new SimpleGsm (GSM_RECEIVER_LINE_PIN, GSM_TRANSMITTER_LINE_PIN, GSM_POWER_PIN);
  }

  return *object;
}

LinkedList <String> &
getSystemRfids ()
{
  static LinkedList <String> * object = NULL;

  if (object == NULL)
  {
    object = new LinkedList <String> ();
  }

  return *object;
}

LinkedList <String> &
getSystemMobiles ()
{
  static LinkedList <String> * object = NULL;

  if (object == NULL)
  {
    object = new LinkedList <String> ();
  }

  return *object;
}

void
disabledEnterOperation ()
{
  printStringWithoutDelay(F("Alarm disabled"));

  flushSystemRfid();
}

void
disabledUpdateOperation ()
{
  if (isAuthenticated ())
  {
    getSystemFsm().transitionTo(getEnabledState());
  }
}

void
disabledExitOperation ()
{
  printStringWithoutDelay(F("Enabling alarm"));

  delay (DELAY_TIME_WHILE_ENABLING_ALARM);
}

void
enabledEnterOperation ()
{
  printStringWithoutDelay(F("Alarm enabled"));

  flushSystemRfid();
}

void
enabledUpdateOperation ()
{
  if (pirSensorSensesMotion() || isDoorOpen ())
  {
    getSystemFsm().transitionTo(getPossibleThreatState());
  }

  if (isAuthenticated ())
  {
    getSystemFsm().transitionTo(getDisabledState());
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
  if (authenticatedOnDelay(DELAY_TIME_OF_CRITICAL_SECTION))
  {
    getSystemFsm().transitionTo(getDisabledState());
  }
  else
  {
    getSystemFsm().transitionTo(getRealThreatState());
  }
}

void
realThreatUpdateOperation ()
{
  sirenOn();

  printStringWithoutDelay(F("Notifying Users"));

  notifyUsers();

  printStringWithoutDelay(F("Waiting police"));

  authenticatedOnDelay(DELAY_TIME_OF_RINGING_SIREN);

  sirenOff();

  getSystemFsm().transitionTo(getDisabledState());
}

bool
initializeSdCard ()
{
  pinMode (DEFAULT_SLAVE_SELECT_PIN, OUTPUT);

  pinMode (SD_SHIELD_CHIP_SELECT_PIN, OUTPUT);

  return SD.begin(SD_SHIELD_CHIP_SELECT_PIN);
}

void
initializePirSensor ()
{
  pinMode (PIR_SENSOR_PIN, INPUT);

  delay (DELAY_TIME_OF_PIR_SENSOR_CALIBRATION);
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

  getSystemLcd().begin(numberOfColumns, numberOfRows);

  getSystemLcd().backlightOff();

  getSystemLcd().clear();
}

void
initializeRfid()
{
  const long baudRate = 9600;

  Serial.begin (baudRate);

  flushSystemRfid();
}

bool
initializeGsm()
{
  const long baudRate = 9600;

  const byte numberOfRetries = 10;

  if (!getSystemGsm().begin(baudRate, numberOfRetries))
  {
    return false;
  }

  if (!getSystemGsm().disableEcho())
  {
    return false;
  }

  if (!getSystemGsm().setSmsTextMode())
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
isDoorOpen ()
{
  return digitalRead(DOOR_SENSOR_PIN) == HIGH;
}

bool
pirSensorSensesMotion ()
{
  return digitalRead(PIR_SENSOR_PIN) == HIGH;
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
  getSystemLcd().clear();

  getSystemLcd().print(string);
}

void
printString (const __FlashStringHelper * string)
{
  printStringWithoutDelay(string);

  delay(DELAY_TIME_OF_LCD_MESSAGE);
}

void
flushSystemRfid()
{
  while (Serial.available ())
  {
    Serial.read ();
  }
}

bool
authenticatedOnDelay (const unsigned long milliseconds)
{
  flushSystemRfid();

  const unsigned long previousMillis = millis();

  while ((unsigned long) (millis() - previousMillis) < milliseconds)
  {
    if (isAuthenticated ())
    {
      return true;
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

bool
rfidTagHandled (String & rfidCode)
{
  // temporary data received from RFID reader.
  byte value = 0;

  // code + checksum data of RFID tag received.
  byte code[6];

  // checksum data of RFID tag received.
  byte checksum = 0;

  // number of received data from RFID reader.
  byte bytesRead = 0;

  // temporary value used for checksum calculation.
  byte tempByte = 0;

  // code characters length.
  const byte rfidCodeSize = 10;

  // code read as null-terminated string.
  char codeRead[rfidCodeSize + 1] = { '\0' };

  // if there are any data coming from the RFID reader.
  if (Serial.available () > 0)
  {
    // check for the STX header (0x02 ASCII value).
    if (0x02 == (value = Serial.read ()))
    {
      // read the RFID 10-digit code & the 2 digit checksum.
      while (bytesRead < (rfidCodeSize + 2))
      {
        // if there are any data coming from the RFID reader.
        if (Serial.available () > 0)
        {
          // get a byte from the RFID reader.
          value = Serial.read ();

          // check for ETX | STX | CR | LF.
          if ((0x0D == value) || (0x0A == value) || (0x03 == value) || (0x02 == value))
          {
            // stop reading - there is an error.
            break;
          }

          // store the RFID code digits to an array.
          if (bytesRead < rfidCodeSize)
          {
            codeRead[bytesRead] = value;
          }

          // convert hex tag ID.
          if ((value >= '0') && (value <= '9'))
          {
            value = value - '0';
          }
          else if ((value >= 'A') && (value <= 'F'))
          {
            value = 10 + value - 'A';
          }

          // every two hex-digits, add byte to code.
          if (bytesRead & 1 == 1)
          {
            // make some space for this hex-digit by shifting
            // the previous hex-digit with 4 bits to the left.
            code[bytesRead >> 1] = (value | (tempByte << 4));

            if (bytesRead >> 1 != 5)
            {
              // if we're at the checksum byte, calculate the checksum (XOR).
              checksum ^= code[bytesRead >> 1];
            }
          }
          else
          {
            tempByte = value;
          }

          // ready to read next digit.
          bytesRead++;
        }
      }

      // handle the RFID 10-digit code & the 2 digit checksum data.
      if (bytesRead == (rfidCodeSize + 2))
      {
        // check if the RFID code is correct.
        if (code[5] == checksum)
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
readSystemRfids()
{
  getSystemRfids().clear();

  File file = SD.open("rfids.txt");

  if (file)
  {
    while (file.available())
    {
      getSystemRfids().add(file.readStringUntil('\n'));
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
readSystemMobiles()
{
  getSystemMobiles().clear();

  File file = SD.open("mobiles.txt");

  if (file)
  {
    while (file.available())
    {
      getSystemMobiles().add(file.readStringUntil('\n'));
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

  const byte numberOfRfids = getSystemRfids().size();

  for (byte i = 0; i < numberOfRfids; i++)
  {
    const String rfid = getSystemRfids().get(i);

    if (cipherText == rfid)
    {
      return true;
    }
  }

  return false;
}

void
notifyUsers ()
{
  const byte notificationsPerUser = 3;

  const byte numberOfMobiles = getSystemMobiles().size();

  for (byte i = 0; i < notificationsPerUser; i++)
  {
    for (byte j = 0; j < numberOfMobiles; j++)
    {
      const String mobile = getSystemMobiles().get(j);

      getSystemGsm().missedCall(mobile, DELAY_TIME_OF_CALL_RINGING_DURATION);

      getSystemGsm().sendSms(mobile, smsText);
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

  printString(F("Init PIR sensor"));

  initializePirSensor();

  printString(F("Init GSM"));

  if (!initializeGsm())
  {
    systemError (F("GSM failed"));
  }

  printString(F("GSM OK"));

  printString(F("Init SD card"));

  if (!initializeSdCard())
  {
    systemError (F("SD card failed"));
  }

  printString(F("SD card OK"));

  printString(F("Read RFIDs"));

  if (!readSystemRfids())
  {
    systemError(F("RFIDs failed"));
  }

  printString(F("RFIDs OK"));

  printString(F("Read mobiles"));

  if (!readSystemMobiles())
  {
    systemError(F("Mobiles failed"));
  }

  printString(F("Mobiles OK"));

  printString(F("Read SMS text"));

  if (!readSmsText())
  {
    systemError(F("SMS text failed"));
  }

  printString(F("SMS text OK"));

  printString(F("Init RFID"));

  initializeRfid();

  printString(F("Read key"));

  readEncryptionKey();
}

void
loop ()
{
  getSystemFsm().update();
}
