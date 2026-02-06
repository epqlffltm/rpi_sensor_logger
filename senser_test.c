/*
2026-02-06
GPIO 센서 테스트
led깜박이기
*/

#include <stdio.h>
#include<stdlib.h>
#include <unistd.h>
#include <gpiod.h>
#include <stdbool.h>

int main(void)
{
  const char *chipname ="gpiochip0";//pi4 칩

  unsigned int offset = 17;

  struct gpiod_chip *chip;
  struct gpiod_line *line;
  //int value = -1;

  chip = gpiod_chip_open_by_name(chipname);//칩 열기
  if(!chip)
  {
    perror("gpiochip0 error");
    exit(1);
  }

  line = gpiod_chip_get_line(chip, offset);//핀 엵기
  if(!line)
  {
    perror("pin error");
    exit(1);
  }

  if(gpiod_line_request_output( line,"sensor-tesr") < 0)//출력 모드
  {
    perror("input error");
    exit(1);
  }

  printf("senser 감지");

  while(true)
  {
    //value = gpiod_line_get_value(line);
    /*
    if(value < 0)
    {
      perror("value error");
      exit(1);
    }
      */
    printf("on\n");
    gpiod_line_set_value(line,1);
    sleep(1);
    
    printf("off\n");
    gpiod_line_set_value(line,0);
    sleep(1);
  }

  //연결 종료
  gpiod_line_release(line);
  gpiod_chip_close(chip);

  return 0;
}