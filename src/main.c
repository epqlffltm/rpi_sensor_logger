/*
2026-02-07
초음파 센서(HC-SR04) 거리 측정 및 SQLite 저장
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <gpiod.h>
#include <sqlite3.h>
#include <signal.h>

// 전역 변수 (시그널 핸들러용)
volatile bool running = true;

void check_error(int is_error, int error_code);
void signal_handler(int sig);

int main(void)
{
    const char *chipname = "gpiochip0"; // Raspberry Pi 4
    const int trig_pin = 27; // 트리거 핀
    const int echo_pin = 17; // 에코 핀

    struct gpiod_chip *chip;
    struct gpiod_line *trig, *echo;
    struct timespec start, end;

    int error_code = 0;
    int ret = 0;
    int num = 0;
    int timeout_count = 0;

    sqlite3 *db;
    char *err_msg = NULL;
    char sql[256];
    
    // Ctrl+C 시그널 핸들러 등록
    signal(SIGINT, signal_handler);

    // SQLite 데이터베이스 열기
    int rc = sqlite3_open("ultrasonic.db", &db);
    check_error(rc != SQLITE_OK, &error_code);

    // 테이블 생성 (이미 존재하면 무시)
    const char *create_table_sql = 
        "CREATE TABLE IF NOT EXISTS ultrasonic("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "measurement_num INT, "
        "distance REAL, "
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";
    
    rc = sqlite3_exec(db, create_table_sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        exit(1);
    }

    // GPIO 칩 열기
    chip = gpiod_chip_open_by_name(chipname);
    error_code = 1;
    check_error(chip == NULL, &error_code);

    // 트리거 핀 설정
    trig = gpiod_chip_get_line(chip, trig_pin);
    error_code = 2;
    check_error(trig == NULL, &error_code);

    // 에코 핀 설정
    echo = gpiod_chip_get_line(chip, echo_pin);
    error_code = 3;
    check_error(echo == NULL, &error_code);

    // 트리거를 출력으로 설정
    ret = gpiod_line_request_output(trig, "trig", 0);
    error_code = 4;
    check_error(ret < 0, &error_code);

    // 에코를 입력으로 설정
    ret = gpiod_line_request_input(echo, "echo");
    error_code = 5;
    check_error(ret < 0, &error_code);

    printf("초음파 센서 작동 시작 (Ctrl+C로 종료)\n");
    printf("데이터베이스: ultrasonic.db\n\n");

    while(running)
    {
        // 센서 안정화를 위한 대기 (60ms)
        usleep(60000);
        num++;

        // 트리거 신호 발생 (10us 펄스)
        gpiod_line_set_value(trig, 0);
        usleep(2);
        gpiod_line_set_value(trig, 1);
        usleep(10);
        gpiod_line_set_value(trig, 0);

        // 에코 신호 HIGH 대기
        timeout_count = 0;
        while(gpiod_line_get_value(echo) == 0)
        {
            usleep(1);
            if(++timeout_count > 30000)
            {
                printf("Error: Echo timeout (waiting for HIGH)\n");
                break;
            }
        }

        if(timeout_count > 30000) {
            continue; // 타임아웃 시 다음 측정으로
        }

        // 에코 신호 시작 시간 기록
        clock_gettime(CLOCK_MONOTONIC, &start);

        // 에코 신호 LOW 대기
        timeout_count = 0;
        while(gpiod_line_get_value(echo) == 1)
        {
            usleep(1);
            if(++timeout_count > 30000)
            {
                printf("Error: Echo timeout (waiting for LOW)\n");
                break;
            }
        }

        if(timeout_count > 30000) {
            continue; // 타임아웃 시 다음 측정으로
        }

        // 에코 신호 종료 시간 기록
        clock_gettime(CLOCK_MONOTONIC, &end);

        // 거리 계산
        double time_sec = (end.tv_sec - start.tv_sec) + 
                          (end.tv_nsec - start.tv_nsec) / 1000000000.0;
        double distance = (time_sec * 34300.0) / 2.0;

        // 유효 범위 체크 (HC-SR04 기준: 2cm ~ 400cm)
        if(distance >= 2.0 && distance <= 400.0) {
            printf("[%d] 거리: %.2f cm\n", num, distance);
            
            // SQLite에 데이터 저장
            snprintf(sql, sizeof(sql), 
                    "INSERT INTO ultrasonic(measurement_num, distance) VALUES(%d, %.2f);",
                    num, distance);
            
            rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "SQL error: %s\n", err_msg);
                sqlite3_free(err_msg);
            }
        } else {
            printf("[%d] 측정 범위 초과: %.2f cm\n", num, distance);
        }

        // 1초 대기
        usleep(1000000);
    }

    // 리소스 정리
    printf("\n프로그램 종료 중...\n");
    gpiod_line_release(trig);
    gpiod_line_release(echo);
    gpiod_chip_close(chip);
    sqlite3_close(db);
    
    printf("총 %d개의 측정값이 저장되었습니다.\n", num);

    return 0;
}

void check_error(int is_error, int error_code)
{
    if (is_error)
    {
        switch(error_code)
        {
            case 0: perror("Error: DB Failed"); break;
            case 1: perror("Error: Chip Open Failed"); break;
            case 2: perror("Error: Trig Pin Failed"); break;
            case 3: perror("Error: Echo Pin Failed"); break;
            case 4: perror("Error: Trig Output Mode Failed"); break;
            case 5: perror("Error: Echo Input Mode Failed"); break;
            default: perror("Error: Unknown Error"); break;
        }
        exit(1);
    }
}

void signal_handler(int sig)
{
    if (sig == SIGINT) {
        printf("\n종료 신호를 받았습니다...\n");
        running = false;
    }
}