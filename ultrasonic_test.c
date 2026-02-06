/*
2026-02-06
초음파 센서 작동 테스트 (에러 체크 함수 통합버전)
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <gpiod.h>

// [변경] 값이 아니라 '에러 여부(조건)'를 받는 함수로 변경
void check_error(int is_error, int n);

int main(void)
{
  const char *chipname = "gpiochip0";
  const int trig_pin = 17;
  const int echo_pin = 27;

  struct gpiod_chip *chip;
  struct gpiod_line *trig, *echo;
  struct timespec start, end;
  int ret; // 결과를 담을 변수

  // 1. 칩 열기 (chip이 NULL이면 에러)
  chip = gpiod_chip_open_by_name(chipname);
  check_error(chip == NULL, 1);

  // 2. 핀 가져오기 (line이 NULL이면 에러)
  trig = gpiod_chip_get_line(chip, trig_pin);
  check_error(trig == NULL, 2);

  echo = gpiod_chip_get_line(chip, echo_pin);
  check_error(echo == NULL, 3);

  // 3. 트리거 출력 모드 설정 (결과가 0보다 작으면 에러)
  ret = gpiod_line_request_output(trig, "trig", 0);
  check_error(ret < 0, 4);

  // 4. 에코 입력 모드 설정 (결과가 0보다 작으면 에러)
  check_error(gpiod_line_request_input(echo, "echo") < 0, 5);

  printf("ultrasonic\n");

  while(true)
  {
    // 트리거 발사
    gpiod_line_set_value(trig, 1);
    usleep(10);
    gpiod_line_set_value(trig, 0);

    // 에코 대기
    while(gpiod_line_get_value(echo) == 0);
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // 에코 종료 대기
    while(gpiod_line_get_value(echo) == 1);
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // 거리 계산
    double time_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1000000000.0;
    double distance = (time_sec * 34300.0) / 2.0;
    
    printf("거리: %.2f cm\n", distance);
    usleep(500000);
  }
  
  gpiod_line_release(trig);
  gpiod_line_release(echo);
  gpiod_chip_close(chip);
  
  return 0;
}

void check_error(int is_error, int n)
{
  if (is_error) // "에러가 맞다면" 실행
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