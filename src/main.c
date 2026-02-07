/*
2026-02-07

*/

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<time.h>
#include<stdlib.h>
#include<gpiod.h>

void check_error(int is_error, int *error_code);

int main(void)
{
  const chat *chipname = "gpiochip0";//pi 4
  const int trig_pin = 27;//트리거
  const int echo_pin = 17;//에코

  struct gpiod_chip *chip;
  struct gpiod_line *trig, *echo;
  struct timespce start, end;

  int error_code = 0;//에러코드
  int ret = 0;

  chip = gpiod_chip_open_by_name(chipname)//칩 활성화
  error_code = 1;
  check_error(chip == NULL, &error_code);

  trig = gpiod_chip_get_line(chip,trig_pin);
  error_code = 2;
  check_error(trig == NULL, &error_code);

  echo = gpiod_chip_get_line(chip,echo_pin);
  error_code = 3;
  check_error(echo == NULL, &error_code);

  ret = gpiod_line_request_output(trig, "trig", 0);
  error_code = 4;
  check_error(ret < 0, &error_code);

  error_code = 5;
  check_error(gpiod_line_request_input(echo,"echo")<0, &error_code);

  printf("초음파 센서 작동 시작");
}