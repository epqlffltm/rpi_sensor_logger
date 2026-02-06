/*
2026-02-06
dht11온습도 센서
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <gpiod.h>
#include <time.h>

#define MAX_TIMINGS 85
#define DHT_PIN 4

int data[5] = { 0, 0, 0, 0, 0 };

void read_dht11(struct gpiod_line *line);

int main(void) 
{
  struct gpiod_chip *chip;
  struct gpiod_line *line;

  chip = gpiod_chip_open_by_name("gpiochip0");
  line = gpiod_chip_get_line(chip, DHT_PIN);

  printf("DHT11 온습도 측정 시작 (2초 간격)\n");

  while (1) 
  {
    read_dht11(line);
    sleep(2);//안정화
  }

  return 0;
}

void read_dht11(struct gpiod_line *line) 
{
  uint8_t last_state = 1;
  uint8_t counter = 0;
  uint8_t j = 0, i;

  data[0] = data[1] = data[2] = data[3] = data[4] = 0;

  // 1. 시작 신호 보내기 (Output 모드)
  gpiod_line_release(line); // 기존 설정 해제 후 다시 요청
  gpiod_line_request_output(line, "dht11", 1);
  gpiod_line_set_value(line, 0);
  usleep(18000); // 최소 18ms 유지
  gpiod_line_set_value(line, 1);
  usleep(40);
    
  // 2. 응답 받기 (Input 모드 전환)
  gpiod_line_release(line);
  gpiod_line_request_input(line, "dht11");

  // 3. 데이터 읽기 (타이밍 체크)
  for (i = 0; i < MAX_TIMINGS; i++) 
  {
    counter = 0;
    while (gpiod_line_get_value(line) == last_state) 
    {
      counter++;
      usleep(1);
      if (counter == 255)
      break;
    }
    last_state = gpiod_line_get_value(line);

    if (counter == 255) 
    break;

    // 앞의 3개 신호는 무시하고 데이터 비트만 추출
    if ((i >= 4) && (i % 2 == 0)) 
    {
      data[j / 8] <<= 1;
      if (counter > 16) // 신호가 길면 1, 짧으면 0 (시스템 성능에 따라 보정 필요)
      data[j / 8] |= 1;
      j++;
    }
  }

  // 40비트 다 읽고 체크섬 확인
  if ((j >= 40) && (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF))) 
  {
    printf("습도: %d.%d %%  온도: %d.%d C\n", data[0], data[1], data[2], data[3]);
  }
  else 
  {
    printf("데이터 읽기 실패 (재시도 중...)\n");
  }
}