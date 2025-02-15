//интегрируем акселерометр 
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
MPU6050 mpu;
volatile bool mpuFlag = false;  // флаг прерывания готовности
uint8_t fifoBuffer[45];         // буфер

//спереди
int trig1 = 13; //пин на отправку сигнала датчика 1
int echo1 = 12; //пин на прием сигнал датчика 1
//сзади
int trig2 = 11; //пин на отправку сигнала датчика 2
int echo2 = 10; //пин на прием сигнал датчика 2
//слева
int trig3 = 9; //пин на отправку сигнала датчика 3
int echo3 = 8; //пин на прием сигнал датчика 3
//справа
int trig4 = 7; //пин на отправку сигнала датчика 4
int echo4 = 6; //пин на прием сигнал датчика 4

//дистанция для остановки
int curdist = 30;

//левый мотор 
#define enA 9
#define in1 5
#define in2 4

//правый мотор
#define enB 6
#define in3 3
#define in4 2
void setup()
{
  //вклбчаем монитор порта
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(1000000UL);     // разгоняем шину на максимум
  // инициализация DMP и прерывания (аксерелрометр)
  mpu.initialize();
  mpu.dmpInitialize();
  mpu.setDMPEnabled(true);
  attachInterrupt(0, dmpReady, RISING);

  //задаем пины
  pinMode(trig1, OUTPUT);
  pinMode(echo1, INPUT);
  pinMode(trig2, OUTPUT);
  pinMode(echo2, INPUT);
  pinMode(trig3, OUTPUT);
  pinMode(echo3, INPUT);
  pinMode(trig4, OUTPUT);
  pinMode(echo4, INPUT);


  //делаем так, чтобы моторы не начали крутиться с самого начала
  stop();
}

void loop()
{
  
  digitalWrite(trig1, 0); //выключаем передний УДР (ультразвуковой датчик расстояния)
  digitalWrite(trig2, 0); //выключаем задний УДР 
  digitalWrite(trig3, 0); //выключаем левый УДР 
  digitalWrite(trig4, 0); //выключаем правый УДР
  
  delayMicroseconds(2);
  digitalWrite(trig1, 1); //включаем УДР передний
  digitalWrite(trig2, 1); //включаем УДР задний
  digitalWrite(trig3, 1); //включаем УДР левый
  digitalWrite(trig4, 1); //включаем УДР правый
  
  delayMicroseconds(10);
  digitalWrite(trig1, 0); //выключаем УДР передний
  digitalWrite(trig2, 0); //выключаем УДР задний
  digitalWrite(trig3, 0); //выключаем УДР левый
  digitalWrite(trig4, 0); //выключаем УДР правый
  
  //передний датчик
  int dur1 = pulseIn(echo1, 1); //задаем переменную задержки приема волны
  int dist1 = dur1/58; //задаем переменную расстояния
  //задний датчик
  int dur2 = pulseIn(echo2, 1); //задаем переменную задержки приема волны
  int dist2 = dur2/58; //задаем переменную расстояния
  //левый датчик
  int dur3 = pulseIn(echo3, 1); //задаем переменную задержки приема волны
  int dist3 = dur3/58; //задаем переменную расстояния
  //правый датчик
  int dur4 = pulseIn(echo4, 1); //задаем переменную задержки приема волны
  int dist4 = dur4/58; //задаем переменную расстояния
  
  //analogWrite(led, map(dist, 0, 400, 0, 255)); //регулируем яркость светодиода расстоянием
  //map(переменная, минимальное значение из которого преобразуем, максимальное,
  //минимальное в которо преобразуем, максимальное) - функция, которая преобразует из одного диапапзона в другой
  
  Serial.println(dist1); //выводим дистанцию в монитор порта
  Serial.println(dist2); //выводим дистанцию в монитор порта
  Serial.println(dist3); //выводим дистанцию в монитор порта
  Serial.println(dist4); //выводим дистанцию в монитор порта
  Serial.println();

  // по флагу прерывания и готовности DMP
  if (mpuFlag && mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) {
    // переменные для расчёта (ypr можно вынести в глобал)
    Quaternion q;
    VectorFloat gravity;
    float ypr[3];
    // расчёты
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
    mpuFlag = false;
    // выводим результат в градусах
    Serial.print(degrees(ypr[0])); // вокруг оси Z
    Serial.print(',');
    Serial.print(degrees(ypr[1])); // вокруг оси Y
    Serial.print(',');
    Serial.print(degrees(ypr[2])); // вокруг оси X
    Serial.println();

    //задаем переменные для градусов в разных проекциях
    int mvZ = degrees(ypr[0]);
    int mvY = degrees(ypr[1]);
    int mvX = degrees(ypr[2]);
  
  
}

//создаем функцию для движения вперед
void GoForward(){
  //включаем оба мотора на полную мощность
  analogWrite(enA, 255);
  analogWrite(enB, 255);

  //задаем направление движения мотора
  digitalWrite(in1, 1);
  digitalWrite(in2, 0);
  digitalWrite(in3, 1);
  digitalWrite(in4, 0);
  delay(100);
  
}
//создаем функцию для полной остановки моторов
void Stop(){
  //выставляем все выходы на ноль
  digitalWrite(in1, 0);
  digitalWrite(in2, 0);
  digitalWrite(in3, 0);
  digitalWrite(in4, 0);
  delay(100);
}
//создаем функцию на движение влево
void Left(){
  //выставляем левый мотор на движение назад
  digitalWrite(in1, 0);
  digitalWrite(in2, 1);
  //выставляем правый мотор на движение вперед
  digitalWrite(in3, 1);
  digitalWrite(in4, 0);
}
//создаем функцию на движение вправо
void Right(){
  //выставляем левый мотор на движение назад
  digitalWrite(in1, 1);
  digitalWrite(in2, 0);
  //выставляем правый мотор на движение вперед
  digitalWrite(in3, 0);
  digitalWrite(in4, 1);
}