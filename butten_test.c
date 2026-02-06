#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gpiod.h>
#include <stdbool.h>

int main(void)
{
    const char *chipname = "gpiochip0"; // Pi 4
    unsigned int led_pin = 17;    // 출력 (LED)
    unsigned int button_pin = 27; // 입력 (버튼)

    struct gpiod_chip *chip;
    struct gpiod_line *led_line;    // LED용 변수 따로
    struct gpiod_line *button_line; // 버튼용 변수 따로
    int val;

    // 1. 칩 열기
    chip = gpiod_chip_open_by_name(chipname);
    if (!chip) {
        perror("chip open error");
        exit(1);
    }

    // 2. LED 핀 설정 (출력)
    led_line = gpiod_chip_get_line(chip, led_pin);
    gpiod_line_request_output(led_line, "led-out", 0);

    // 3. 버튼 핀 설정 (입력)
    button_line = gpiod_chip_get_line(chip, button_pin);
    gpiod_line_request_input(button_line, "button-in");

    printf("버튼 테스트 시작... (Ctrl+C 종료)\n");

    // 4. 무한 루프
    while (true) {
        // 버튼 상태 읽기 (눌리면 1, 아니면 0)
        // *연결 방식에 따라 반대(눌리면 0)일 수 있음
        val = gpiod_line_get_value(button_line);
        
        // 읽은 값을 LED에 그대로 전달
        gpiod_line_set_value(led_line, val);

        usleep(10000); // 0.01초 대기 (CPU 부하 감소)
    }

    // 5. 정리
    gpiod_line_release(led_line);
    gpiod_line_release(button_line);
    gpiod_chip_close(chip);

    return 0;
}