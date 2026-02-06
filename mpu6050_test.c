/*
2026-02-06
가솟 센서 test
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>


int main(void)
{
  //MPU6050 기본 주소 및 레지스터
  const int addr = 0x68;
  const int pwr = 0x6b;
  const int accel = 0x3b;
  //컴파일 검사를 위해 const int에 담음.

  int file = 0;
  char *bus = "/dev/i2c-1";
  int16_t ax, ay, az, temp;

  
  if((file = open(bus, O_RDWR))<0)//I2C 버스 열기
  {
    perror("error: failed to opeen the i2c bus");
    exit(1);
  }

  ioctl(file, I2C_SLAVE, addr);//I2C 슬레이브 주소 설정

  //MPU6050 깨우기
  uint8_t wakeup[] = {pwr,0x00};
  write(file, wakeup, 2);
  
  printf("GY-521 센서 로그 기록 시작 (표 형태)...\n");
  printf("----------------------------------------------------------\n");
  printf("|  Time (s)  |   Acc X   |   Acc Y   |   Acc Z   |  Temp C  |\n");
  printf("----------------------------------------------------------\n");

  int count = 0;
  while (count < 20) // 테스트용으로 20번만 출력
  {
    uint8_t reg = accel;
    uint8_t data[14]; // 가속도(6) + 온도(2) + 자이로(6) = 14바이트
    
    // 읽을 레지스터 위치 지정
    write(file, &reg, 1);
    // 데이터 읽기
    read(file, data, 14);

    // 2바이트씩 합쳐서 16비트 정수로 변환 (상위비트 << 8 | 하위비트)
    ax = (data[0] << 8) | data[1];
    ay = (data[2] << 8) | data[3];
    az = (data[4] << 8) | data[5];
    temp = (data[6] << 8) | data[7];

    // 온도 공식: (데이터 / 340) + 36.53
    float celsius = (float)temp / 340.0 + 36.53;
    printf("| %10d | %9d | %9d | %9d | %8.2f |\n", count, ax, ay, az, celsius);

    count++;
    usleep(500000); // 0.5초 간격
  }
  close(file);

  return 0;
}
