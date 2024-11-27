// 필요한 라이브러리 포함
#include <SoftwareSerial.h>         // HC-06 블루투스 모듈용
#include <PulseSensorPlayground.h>  // 심박수 센서용
#include <Adafruit_MLX90614.h>      // 적외선 온도 센서용

// 센서 객체 생성
PulseSensorPlayground pulseSensor;
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// 블루투스 모듈은 2번(RX) 및 3번(TX) 핀에 연결
SoftwareSerial BTSerial(2, 3); // RX | TX

// 타이밍 변수
unsigned long conditionStartTime = 0; // 알림 조건 시작 시간 기록
bool conditionMet = false;            // 알림 조건 충족 여부

// 임계값 (사용자가 필요에 따라 조정해야 함)
float low_danger_temp_low = 35.0;    // A: 매우 위험한 낮은 체온
float low_danger_temp_high = 36.0;   // B: 약간 위험한 낮은 체온
float high_danger_temp_low = 37.5;   // C: 약간 위험한 높은 체온
float high_danger_temp_high = 39.0;  // D: 매우 위험한 높은 체온

int low_danger_heartrate_low = 40;   // E: 매우 위험한 낮은 심박수
int low_danger_heartrate_high = 50;  // F: 약간 위험한 낮은 심박수
int high_danger_heartrate_low = 100; // G: 약간 위험한 높은 심박수
int high_danger_heartrate_high = 120;// H: 매우 위험한 높은 심박수

// 알림 임계값
float threshold = 1.0; // 알림을 위한 임계값 (필요에 따라 조정)


// 센서 읽기 값을 저장할 변수
float currentHeartRate = 0.0;
float currentBodyTemp = 0.0;

void setup() {
  // 시리얼 통신 초기화
  Serial.begin(9600);
  BTSerial.begin(9600); // 블루투스 모듈 보드레이트

  // 센서 초기화
  initializeHeartRateSensor();
  initializeTemperatureSensor();

  // 추가 설정이 필요하면 여기에 추가
}

void loop() {
  unsigned long currentMillis = millis(); // 현재 시간 가져오기

  // 심박수 데이터 연속적으로 읽기
  readHeartRateSensorData();

  // 5초마다 체온 읽기
  if (currentMillis % 5000 < 50) {
    currentBodyTemp = getBodyTempFromSensor();
  }

  // 정규화된 값 계산
  float heartRateNormalized = normalizeHeartRate(currentHeartRate, low_danger_heartrate_low, low_danger_heartrate_high, high_danger_heartrate_low, high_danger_heartrate_high);
  float bodyTempNormalized = normalizeBodyTemperature(currentBodyTemp, low_danger_temp_low, low_danger_temp_high, high_danger_temp_low, high_danger_temp_high);

  // 가중치를 사용하여 알림 값 계산
  float alertval = 0.7 * heartRateNormalized + 0.3 * bodyTempNormalized;

  // 알림 조건 확인
  if (alertval >= threshold) {
    if (!conditionMet) {
      conditionMet = true;
      conditionStartTime = currentMillis; // 조건 충족 시작 시간 기록
    } else {
      unsigned long elapsedTime = currentMillis - conditionStartTime;
      if (elapsedTime >= 60000) { // 조건이 1분 이상 지속되면
        alert119();
      }
    }
  } else if (alertval > 0 && alertval < threshold) {
    alertUser();
    // 조건 초기화
    conditionMet = false;
    conditionStartTime = 0;
  } else {
    // alertval이 0이면 조건 초기화
    conditionMet = false;
    conditionStartTime = 0;
  }

  delay(50); // 센서 읽기 안정성을 위한 짧은 지연
}

void initializeHeartRateSensor() {
  // 심박수 센서 초기화
  pulseSensor.analogInput(A0); // 센서가 A0에 연결되었다고 가정
  pulseSensor.setThreshold(550);
  if (pulseSensor.begin()) {
    Serial.println("심박수 센서 초기화 성공.");
  } else {
    Serial.println("심박수 센서 초기화 실패.");
  }
}

void initializeTemperatureSensor() {
  // 체온 센서 초기화
  if (mlx.begin()) {
    Serial.println("체온 센서 초기화 성공.");
  } else {
    Serial.println("체온 센서 초기화 실패.");
  }
}

void readHeartRateSensorData() {
  // 센서로부터 심박수 읽기
  if (pulseSensor.sawNewSample()) {
    currentHeartRate = pulseSensor.getBeatsPerMinute();
    if (pulseSensor.sawStartOfBeat()) {
      Serial.print("심박수: ");
      Serial.println(currentHeartRate);
    }
  }
}

float getBodyTempFromSensor() {
  // 센서로부터 체온 읽기
  float tempC = mlx.readObjectTempC();
  Serial.print("체온: ");
  Serial.println(tempC);
  return tempC;
}

float normalizeHeartRate(float heartRate, int low_low, int low_high, int high_low, int high_high) {
  float normalizedValue = 0.0;

  if (heartRate < low_low) {
    // 매우 위험한 낮은 심박수
    normalizedValue = 1.0;
  } else if (heartRate >= low_low && heartRate < low_high) {
    // 약간 위험한 낮은 심박수
    normalizedValue = (low_high - heartRate) / (low_high - low_low);
  } else if (heartRate >= low_high && heartRate <= high_low) {
    // 정상 심박수
    normalizedValue = 0.0;
  } else if (heartRate > high_low && heartRate <= high_high) {
    // 약간 위험한 높은 심박수
    normalizedValue = (heartRate - high_low) / (high_high - high_low);
  } else if (heartRate > high_high) {
    // 매우 위험한 높은 심박수
    normalizedValue = 1.0;
  }

  return normalizedValue; // 0.0에서 1.0 사이의 값 반환
}

float normalizeBodyTemperature(float bodyTemp, float low_low, float low_high, float high_low, float high_high) {
  float normalizedValue = 0.0;

  if (bodyTemp < low_low) {
    // 매우 위험한 낮은 체온
    normalizedValue = 1.0;
  } else if (bodyTemp >= low_low && bodyTemp < low_high) {
    // 약간 위험한 낮은 체온
    normalizedValue = (low_high - bodyTemp) / (low_high - low_low);
  } else if (bodyTemp >= low_high && bodyTemp <= high_low) {
    // 정상 체온
    normalizedValue = 0.0;
  } else if (bodyTemp > high_low && bodyTemp <= high_high) {
    // 약간 위험한 높은 체온
    normalizedValue = (bodyTemp - high_low) / (high_high - high_low);
  } else if (bodyTemp > high_high) {
    // 매우 위험한 높은 체온
    normalizedValue = 1.0;
  }

  return normalizedValue; // 0.0에서 1.0 사이의 값 반환
}

void alertUser() {
  // 블루투스를 통해 사용자에게 알림 전송
  BTSerial.println("경고: 생체 신호가 정상 범위를 벗어났습니다.");
  Serial.println("사용자에게 알림을 보냈습니다.");
}

void alert119() {
  // 블루투스를 통해 긴급 알림 전송
  BTSerial.println("긴급: 즉각적인 도움이 필요합니다!");
  Serial.println("긴급 서비스에 알림을 보냈습니다.");
}
