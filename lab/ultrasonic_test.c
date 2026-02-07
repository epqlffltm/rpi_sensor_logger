/*
2026-02-06
초음파 센서 작동 테스트 (에러 체크 함수 통합버전)
*/

/*
2026-02-06
초음파 센서 작동 테스트 (안정화 버전)
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <gpiod.h>

void check_error(int is_error, int n);

int main(void)
{
  const char *chipname = "gpiochip0";
  const int trig_pin = 17;
  const int echo_pin = 27;

  struct gpiod_chip *chip;
  struct gpiod_line *trig, *echo;
  struct timespec start, end;
  int ret;

  chip = gpiod_chip_open_by_name(chipname);
  check_error(chip == NULL, 1);

  trig = gpiod_chip_get_line(chip, trig_pin);
  check_error(trig == NULL, 2);

  echo = gpiod_chip_get_line(chip, echo_pin);
  check_error(echo == NULL, 3);

  ret = gpiod_line_request_output(trig, "trig", 0);
  check_error(ret < 0, 4);

  check_error(gpiod_line_request_input(echo, "echo") < 0, 5);

  printf("ultrasonic sensor started\n");

  while(true)
  {
    // 1. 센서 안정화를 위한 대기
    usleep(60000); // 60ms 대기 (최소 측정 주기)
    
    // 2. 트리거 신호 (10us 펄스)
    gpiod_line_set_value(trig, 0);
    usleep(2); // 확실하게 LOW 상태로
    gpiod_line_set_value(trig, 1);
    usleep(10);
    gpiod_line_set_value(trig, 0);

    // 3. 에코 신호 대기 (타임아웃 추가)
    int timeout_count = 0;
    while(gpiod_line_get_value(echo) == 0) 
    {
      usleep(1);
      if(++timeout_count > 10000) 
      { // 10ms 타임아웃
        printf("Error: Echo timeout (waiting for HIGH)\n");
        continue; // 다음 측정으로
      }
    }
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // 4. 에코 종료 대기 (타임아웃 추가)
    timeout_count = 0;
    while(gpiod_line_get_value(echo) == 1) 
    {
      usleep(1);
      if(++timeout_count > 30000) 
      { // 30ms 타임아웃 (최대 측정 거리 약 5m)
        printf("Error: Echo timeout (waiting for LOW)\n");
        break;
      }
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // 5. 거리 계산 (유효성 검증 추가)
    double time_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1000000000.0;
    double distance = (time_sec * 34300.0) / 2.0;
    
    // 6. 유효 범위 체크 (HC-SR04 기준: 2cm ~ 400cm)
    if(distance >= 2.0 && distance <= 400.0) {
      printf("거리: %.2f cm\n", distance);
    } else {
      printf("측정 범위 초과: %.2f cm\n", distance);
    }
    usleep(1000000);
  }
  
  gpiod_line_release(trig);
  gpiod_line_release(echo);
  gpiod_chip_close(chip);
  
  return 0;
}

void check_error(int is_error, int n)
{
  if (is_error)
  {
    switch(n)
    {
      case 1: perror("Error: Chip Open Failed"); break;
      case 2: perror("Error: Trig Pin Failed"); break;
      case 3: perror("Error: Echo Pin Failed"); break;
      case 4: perror("Error: Trig Output Mode Failed"); break;
      case 5: perror("Error: Echo Input Mode Failed"); break;
    }
    exit(1);
  }
}