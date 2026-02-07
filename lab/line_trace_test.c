/*
2026-02-06
라인 트레이더 테스트
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <gpiod.h>

void check_error(int is_error, int *pn);

int main(void)
{
  const char *chipname = "gpiochip0";
  const int line_pin = 26;
  int error_code = 0;

  struct gpiod_chip *chip;
  struct gpiod_line *ir;

  chip = gpiod_chip_open_by_name(chipname);
  error_code = 1;
  check_error(chip == NULL, &error_code);

  ir = gpiod_chip_get_line(chip,line_pin);
  error_code = 2;
  check_error(ir == NULL, &error_code);

  error_code = 3;
  check_error(gpiod_line_request_input(ir, "line_trace_test") < 0 , &error_code);

  printf("ir_line_trace_test\n");

  while(true)
  {
    gpiod_line_get_value(ir) == true ? printf("true\n") : printf("false\n");
    /*
    if(gpiod_line_get_value(ir) == true)
    printf("true\n");

    else if(gpiod_line_get_value(ir) == false)
    printf("false\n");
    */

    usleep(100000);
  }

  gpiod_line_release(ir);
  gpiod_chip_close(chip);

  return 0;
}

void check_error(int is_error, int *pn)
{
  if (is_error)
  {
    switch(*pn)
    {
      case 1: 
      perror("error: chip open failed");
      break;

      case 2:
      perror("error: IR pin failed");
      break;

      case 3:
      perror("Error: Input Request Failed");
      break;
    }


    exit(1);
  }
}