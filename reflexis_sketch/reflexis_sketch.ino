// 2, 4, 18, 19, 21, 22, 23, 13, 12, 27, 26, 25, 33, 32 – нормальные обычные пины
#include <Arduino.h>
#include <EEPROM.h>
#include <MedianFilter.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID        "6fff889d-c509-408f-9284-5aeefada3f4d"
#define CHARACTERISTIC_UUID "09aa0822-08df-42af-913c-428d0355e9b2"

#define INA 21
#define INB 22
#define MOTORPWM 23
#define ANGLEPIN 25
#define EMGPIN 26
#define eepromSize 2
#define DEBUG_SERIAL_ENABLE

int highLimit;                //2420
int lowLimit;                 //1100

MedianFilter test(50, lowLimit);


struct trainingProgram {
  int participation;
  int spd;
  int angleStart;
  int angleEnd;
  int amount;
  int tm;
  bool isActive;
};


trainingProgram progs[10] = {(trainingProgram){ -1, 0, 0, 0, 0, 0, false}, (trainingProgram){ -1, 0, 0, 0, 0, 0, false}, (trainingProgram){ -1, 0, 0, 0, 0, 0, false}, (trainingProgram){ -1, 0, 0, 0, 0, 0, false}, (trainingProgram){ -1, 0, 0, 0, 0, 0, false}, (trainingProgram){ -1, 0, 0, 0, 0, 0, false}, (trainingProgram){ -1, 0, 0, 0, 0, 0, false}, (trainingProgram){ -1, 0, 0, 0, 0, 0, false}, (trainingProgram){ -1, 0, 0, 0, 0, 0, false}, (trainingProgram) {
  -1, 0, 0, 0, 0, 0, false}
};
int currentBlock = -1;


//---------------------------------------------------ВЫПОЛНЕНИЕ ПАССИВНОЙ ПРОГРАММЫ
bool isStopped = false;
bool isPremade = false;

void passiveProgramm(int spd, int loAngle, int hiAngle, int minsTime, int number, void *ptr) {
  int spdNorm = 95 + spd * 16;
  int curTime = millis();
  lowLimit = analogRead(ANGLEPIN);
  highLimit = lowLimit + 1345;
  MedianFilter test(50, lowLimit);
  int curAngle = calibrateAngle(analogRead(ANGLEPIN));
  ledcWrite(0, spdNorm);
  //Serial.println(curTime);
  //Serial.println(millis() - curTime);
  //Serial.println(minsTime * 60 * 1000);
  //Serial.println(spdNorm);
  delay(2000);
  while (millis() - curTime < minsTime * 60 * 1000) {
    digitalWrite(INA, LOW);            //сгибаем руку
    digitalWrite(INB, HIGH);
    while (abs(curAngle - hiAngle) > 3 && millis() - curTime < minsTime * 60 * 1000) {
      curAngle = calibrateAngle(analogRead(ANGLEPIN));
      Serial.println(curAngle);
      if (isStopped) {
        return;
      }
      delay(50);
    }

    digitalWrite(INB, LOW);         // разогибаем руку
    digitalWrite(INA, HIGH);
    while (abs(curAngle - loAngle) > 3 && millis() - curTime < minsTime * 60 * 1000) {
      curAngle = calibrateAngle(analogRead(ANGLEPIN));
      if (isStopped) {
        return;
      }
      delay(50);
    }
  }

  ledcWrite(0, 111);
  while (abs(curAngle) > 2) {
    curAngle = calibrateAngle(analogRead(ANGLEPIN));
    delay(50);
  }
}



//---------------------------------------------------ВЫПОЛНЕНИЕ АКТИВНОЙ ПРОГРАММЫ

void activeProgramm(int involvment, int spd, int loAngle, int hiAngle, int flex, int number, void *ptr) {
  
  String participation;
  int spdNorm = 100 + spd * 11;
  int flexCounter = 0;
  String flexAmount;
  lowLimit = analogRead(ANGLEPIN);
  highLimit = lowLimit + 1345;
  MedianFilter test(50, lowLimit);
  int curAngle = calibrateAngle(analogRead(ANGLEPIN));
  int curEmg = 0;
  int curEmgNorm;
  ledcWrite(0, 111);

  digitalWrite(INA, LOW);
  digitalWrite(INB, HIGH);

//  // для дебага
//  while (abs(curAngle - hiAngle) > 3) {
//    if (isStopped) {
//      return;
//    }
//    curAngle = calibrateAngle(analogRead(ANGLEPIN));
//    delay(50);
//  }

  while (flexCounter < flex) {

    digitalWrite(INB, LOW);         // разгибаем руку
    digitalWrite(INA, HIGH);

    ledcWrite(0, spdNorm);
    while (abs(curAngle - loAngle) > 3) {
      curAngle = calibrateAngle(analogRead(ANGLEPIN));
      curEmg = analogRead(EMGPIN);
      curEmgNorm = map(curEmg, 1800, 4086, 0, 100);
      if (isStopped) {
        return;
      }
      if (curEmg >= 585 + involvment * 350) {
        ledcWrite(0, spdNorm + 45);  
        delay(500);
      }else{
        ledcWrite(0, spdNorm);
      }
      delay(50);
    }

    digitalWrite(INA, LOW);            //сгибаем руку
    digitalWrite(INB, HIGH);

    ledcWrite(0, spdNorm);
    while (abs(curAngle - hiAngle) > 3) {
      curAngle = calibrateAngle(analogRead(ANGLEPIN));
      curEmg = analogRead(EMGPIN);
      curEmgNorm = map(curEmg, 1800, 4086, 0, 100);
      if (isStopped) {
        return;
      }
      if (curEmg >= 585 + involvment * 350) {
        ledcWrite(0, spdNorm + 45);
        delay(500);
      }else{
        ledcWrite(0, spdNorm);
      }
      delay(50);
    }
    flexCounter++;
  }

  ledcWrite(0, 111);
  digitalWrite(INB, LOW);
  digitalWrite(INA, HIGH);
  while (abs(curAngle) > 3) {
    curAngle = calibrateAngle(analogRead(ANGLEPIN));
    delay(50);
  }
}


//Выполнение составленной программы

void executePrograms(void *ptr) {
  int i = 0;
  while (progs[i].participation != -1) {
    if (progs[i].isActive) {
      activeProgramm(progs[i].participation, progs[i].spd,  progs[i].angleStart, progs[i].angleEnd, progs[i].amount, i + 1, ptr);
    } else {
      passiveProgramm(progs[i].spd, progs[i].angleStart, progs[i].angleEnd, progs[i].tm, i + 1, ptr);
    }
    i++;
    if (i == 10) {
      break;
    }
    if (isStopped) {
      isStopped = false;
      break;
    }
  }
  isPremade = false;
}

//Остановка программы

void stopProgram(void *ptr) {
  Serial.println("СТОООООООП");

  isStopped = true;
  ledcWrite(0, 127);
  digitalWrite(INB, LOW);         // разгибаем руку
  digitalWrite(INA, HIGH);
  /*  int curAngle = calibrateAngle(analogRead(ANGLEPIN));
    while (abs(curAngle) > 2) {
      curAngle = calibrateAngle(analogRead(ANGLEPIN));
      //Serial.println(analogRead(ANGLEPIN));
      //Serial.println(curAngle);
      delay(50);
    }*/
  /*НЕ УВЕРЕН НАДО ЛИ ТУТ ЕЩЕ ЧТО-НИБУДЬ ОСТАНАВЛИВАТЬ*/
}


//int angleArray[7] = {0, 0, 0, 0, 0, 0, 0};

int calibrateAngle(int angle) {
  /*int sum = 0;
    int newAngle = map(angle, lowLimit, highLimit, -1, 136);
    if (abs(newAngle - angleArray[6]) > 50) {
    lowLimit += angle - map(angleArray[6], -1, 136, lowLimit, highLimit);
    highLimit = lowLimit + 1585;
    newAngle = map(angle, lowLimit, highLimit, -1, 136);
    }
    if (newAngle < -1) {
    newAngle = -1;
    } else if (newAngle > 136) {
    newAngle = 136;
    }
    for (int i = 0; i < 6; i++) {
    angleArray[i] = angleArray[i + 1];
    }
    angleArray[6] = newAngle;
    for (int i = 0; i < 7; i++) {
    sum += angleArray[i];
    }
    return sum / 7;*/
  if (angle < lowLimit) {
    angle = lowLimit;
  } else if (angle > highLimit) {
    angle =  highLimit;
  }
  test.in(angle);
  int result = map(test.out(), lowLimit, highLimit, -1, 136);
  if (result <= -1) {
    result = 0;
  } else if (result >= 136) {
    result = 135;
  }
  return result;
}


class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic, void *ptr) {
      String value = pCharacteristic->getValue().c_str();

      if (value.length() > 0) {
        Serial.println("*********");
        Serial.print("New value: ");
        for (int i = 0; i < value.length(); i++){
          
        }
        Serial.print(value[0]);

        Serial.println();
        Serial.println("*********");
        delay(2000);
        if(value[0] == 1)
          passiveProgramm(5, 0, 135, 5, 1, ptr);
      }
    }
};


void setup() {
  Serial.begin(115200);
  EEPROM.begin(eepromSize);
  delay(10);

  pinMode(INA, OUTPUT);
  pinMode(INB, OUTPUT);
  pinMode(MOTORPWM, OUTPUT);
  ledcSetup(0, 5000, 8); //канал=0, частота=5000, разрешение=8бит
  ledcAttachPin(MOTORPWM, 0);

  //Serial.println("setup done");
  //Serial.println("lol");
  
  delay(10);
  lowLimit = analogRead(ANGLEPIN);
  highLimit = lowLimit + 1585;
  ledcWrite(0, 127);  //устанавливаем среднюю скорость
  digitalWrite(INB, LOW);         // разгибаем руку
  digitalWrite(INA, HIGH);


  BLEDevice::init("ReFlexis");
  BLEServer *pServer = BLEDevice::createServer();

  BLEService *pService = pServer->createService(SERVICE_UUID);

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());

  pCharacteristic->setValue("");
  pService->start();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
}

void loop() {
//  if (deviceConnected) {
//        pCharacteristic->setValue((uint8_t*)&value, 4);
//        pCharacteristic->notify();
//        value++;
//        delay(3); 
//        
//    
//    if (!deviceConnected && oldDeviceConnected) {
//        delay(500); 
//        pServer->startAdvertising(); 
//        Serial.println("start advertising");
//        oldDeviceConnected = deviceConnected;
//    }
//
//    
//    if (deviceConnected && !oldDeviceConnected) {
//        oldDeviceConnected = deviceConnected;
//    }
}
