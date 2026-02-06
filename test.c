#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define DHT_PIN 4 // GPIO 4 (물리적 7번 핀)

int main(void) {
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    int data[5] = {0, 0, 0, 0, 0};

    chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip) { perror("chip open failed"); return 1; }

    line = gpiod_chip_get_line(chip, DHT_PIN);
    if (!line) { perror("line get failed"); return 1; }

    while (1) {
        // 1. 센서에게 시작 신호 보내기
        gpiod_line_request_output(line, "dht11", 1);
        gpiod_line_set_value(line, 0);
        usleep(18000); // 18ms 대기
        gpiod_line_set_value(line, 1);
        usleep(40);
        gpiod_line_release(line);

        // 2. 센서 응답 읽기 준비
        gpiod_line_request_input(line, "dht11");

        int last_state = 1;
        int counter = 0;
        int j = 0;
        data[0] = data[1] = data[2] = data[3] = data[4] = 0;

        // 3. 85번의 신호 변화를 감지 (타이밍 핵심)
        for (int i = 0; i < 85; i++) {
            counter = 0;
            while (gpiod_line_get_value(line) == last_state) {
                counter++;
                usleep(1);
                if (counter == 255) break;
            }
            last_state = gpiod_line_get_value(line);
            if (counter == 255) break;

            // 데이터 비트 추출 (앞의 응답 신호 제외)
            if ((i >= 4) && (i % 2 == 0)) {
                data[j / 8] <<= 1;
                if (counter > 16) data[j / 8] |= 1; // counter 기준값 보정
                j++;
            }
        }

        // 4. 체크섬 확인 및 출력
        if ((j >= 40) && (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF))) {
            printf("성공! 습도: %d%% 온도: %dC\n", data[0], data[2]);
        } else {
            printf("읽기 시도 중...\n");
        }

        gpiod_line_release(line);
        sleep(2); // DHT11은 2초 간격 필수
    }

    gpiod_chip_close(chip);
    return 0;
}