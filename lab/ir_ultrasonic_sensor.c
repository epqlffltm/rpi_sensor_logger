/*
 * 파일명: ir_ultrasonic_sensor.c
 * 작성일: 2026-02-07
 * 설명: IR 센서로 물체 감지 시 초음파 센서로 거리 측정 후 LED 제어
 *       거리 데이터는 SQLite DB에 저장
 */

#include <stdio.h>      // printf, fprintf 등 표준 입출력 함수
#include <stdlib.h>     // exit, malloc 등 표준 라이브러리 함수
#include <unistd.h>     // usleep (마이크로초 단위 대기) 함수
#include <time.h>       // clock_gettime (시간 측정) 함수
#include <stdbool.h>    // bool, true, false 타입 사용
#include <gpiod.h>      // GPIO 제어 라이브러리 (libgpiod)
#include <sqlite3.h>    // SQLite 데이터베이스 라이브러리
#include <signal.h>     // 시그널 처리 (Ctrl+C 감지)

// ========== 전역 변수 ==========
// volatile: 컴파일러 최적화 방지 (시그널 핸들러에서 변경되므로)
volatile bool running = true;        // 프로그램 실행 상태 (false되면 종료)
volatile bool ir_detected = false;   // IR 센서 감지 플래그

// ========== 함수 선언 ==========
void check_error(int is_error, int error_code);  // 에러 체크 및 종료 함수
void signal_handler(int sig);                     // Ctrl+C 시그널 핸들러

int main(void)
{
    // ========== GPIO 핀 번호 및 상수 정의 ==========
    const char *chipname = "gpiochip0";   // Raspberry Pi의 GPIO 칩 이름
    const int trig_pin = 27;              // 초음파 센서 트리거 핀 (GPIO 27)
    const int echo_pin = 17;              // 초음파 센서 에코 핀 (GPIO 17)
    const int ir_pin = 22;                // IR 센서 입력 핀 (GPIO 22)
    const int led_pin = 23;               // LED 출력 핀 (GPIO 23)
    const double THRESHOLD = 20.0;        // LED 켜질 거리 임계값 (20cm)

    // ========== GPIO 관련 구조체 변수 ==========
    struct gpiod_chip *chip;              // GPIO 칩 핸들
    struct gpiod_line *trig, *echo;       // 초음파 센서용 GPIO 라인
    struct gpiod_line *ir, *led;          // IR 센서, LED용 GPIO 라인
    struct timespec start, end;           // 시간 측정용 (에코 신호 시간 계산)
    struct timespec timeout;              // 인터럽트 대기 타임아웃 설정용
    struct gpiod_line_event event;        // GPIO 이벤트 정보 저장용

    // ========== 일반 변수 ==========
    int error_code = 0;                   // 에러 코드 (어떤 단계에서 에러인지 구분)
    int ret = 0;                          // 함수 리턴값 임시 저장
    int num = 0;                          // 측정 횟수 카운터
    int timeout_count = 0;                // 타임아웃 체크용 카운터

    // ========== SQLite 관련 변수 ==========
    sqlite3 *db;                          // 데이터베이스 핸들
    char *err_msg = NULL;                 // SQLite 에러 메시지 저장용
    char sql[256];                        // SQL 쿼리 문자열 버퍼
    
    // ========== 시그널 핸들러 등록 ==========
    // Ctrl+C (SIGINT) 입력 시 signal_handler 함수 호출
    signal(SIGINT, signal_handler);

    // ========== SQLite 데이터베이스 초기화 ==========
    // ultrasonic.db 파일을 열거나 생성 (없으면 자동 생성)
    int rc = sqlite3_open("ultrasonic.db", &db);
    check_error(rc != SQLITE_OK, error_code);  // 실패 시 프로그램 종료

    // 테이블 생성 SQL 문 (이미 존재하면 무시)
    const char *create_table_sql = 
        "CREATE TABLE IF NOT EXISTS ultrasonic("  // IF NOT EXISTS: 이미 있으면 건너뜀
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "  // 자동 증가하는 고유 ID
        "measurement_num INT, "                   // 측정 번호
        "distance REAL, "                         // 측정된 거리 (실수)
        "ir_triggered BOOL, "                     // IR 센서 트리거 여부 (1 or 0)
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";  // 자동 타임스탬프
    
    // SQL 실행 (콜백 없음, 결과 없음, 에러 메시지만 받음)
    rc = sqlite3_exec(db, create_table_sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);  // 에러 출력
        sqlite3_free(err_msg);                        // 에러 메시지 메모리 해제
        sqlite3_close(db);                            // DB 닫기
        exit(1);                                      // 프로그램 종료
    }

    // ========== GPIO 칩 열기 ==========
    // gpiochip0 장치를 열어서 chip 핸들에 저장
    chip = gpiod_chip_open_by_name(chipname);
    error_code = 1;                       // 에러 발생 시 "Chip Open Failed" 출력용
    check_error(chip == NULL, error_code); // NULL이면 열기 실패

    // ========== 트리거 핀 설정 (초음파 센서) ==========
    // GPIO 칩에서 trig_pin 번호의 라인 가져오기
    trig = gpiod_chip_get_line(chip, trig_pin);
    error_code = 2;                       // 에러 코드: "Trig Pin Failed"
    check_error(trig == NULL, error_code);

    // ========== 에코 핀 설정 (초음파 센서) ==========
    // GPIO 칩에서 echo_pin 번호의 라인 가져오기
    echo = gpiod_chip_get_line(chip, echo_pin);
    error_code = 3;                       // 에러 코드: "Echo Pin Failed"
    check_error(echo == NULL, error_code);

    // ========== IR 센서 핀 설정 ==========
    // GPIO 칩에서 ir_pin 번호의 라인 가져오기
    ir = gpiod_chip_get_line(chip, ir_pin);
    error_code = 4;                       // 에러 코드: "IR Pin Failed"
    check_error(ir == NULL, error_code);

    // ========== LED 핀 설정 ==========
    // GPIO 칩에서 led_pin 번호의 라인 가져오기
    led = gpiod_chip_get_line(chip, led_pin);
    error_code = 5;                       // 에러 코드: "LED Pin Failed"
    check_error(led == NULL, error_code);

    // ========== 트리거 핀을 출력 모드로 설정 ==========
    // "trig"는 이름표, 0은 초기값 LOW
    ret = gpiod_line_request_output(trig, "trig", 0);
    error_code = 6;                       // 에러 코드: "Trig Output Mode Failed"
    check_error(ret < 0, error_code);     // 음수 반환 시 실패

    // ========== 에코 핀을 입력 모드로 설정 ==========
    // 초음파 센서에서 돌아오는 신호를 읽기 위해 입력으로 설정
    ret = gpiod_line_request_input(echo, "echo");
    error_code = 7;                       // 에러 코드: "Echo Input Mode Failed"
    check_error(ret < 0, error_code);

    // ========== IR 센서를 인터럽트 모드로 설정 ==========
    // falling edge: HIGH → LOW로 변할 때 이벤트 발생 (물체 감지 시)
    ret = gpiod_line_request_falling_edge_events(ir, "ir_sensor");
    error_code = 8;                       // 에러 코드: "IR Interrupt Mode Failed"
    check_error(ret < 0, error_code);

    // ========== LED 핀을 출력 모드로 설정 ==========
    // 초기값 0 (꺼진 상태)
    ret = gpiod_line_request_output(led, "led", 0);
    error_code = 9;                       // 에러 코드: "LED Output Mode Failed"
    check_error(ret < 0, error_code);

    // ========== 시작 메시지 출력 ==========
    printf("IR 센서 + 초음파 센서 통합 시스템 시작\n");
    printf("IR 센서가 물체를 감지하면 초음파로 거리 측정\n");
    printf("거리 %.1f cm 이내면 LED ON\n\n", THRESHOLD);

    // ========== 인터럽트 대기 타임아웃 설정 ==========
    // 1초마다 체크 (이벤트 없으면 1초 대기)
    timeout.tv_sec = 1;                   // 1초
    timeout.tv_nsec = 0;                  // 0 나노초

    // ========== 메인 루프 (Ctrl+C 전까지 무한 반복) ==========
    while(running)
    {
        // ========== IR 센서 인터럽트 대기 ==========
        // timeout 시간 동안 이벤트를 기다림
        // 반환값: >0 이벤트 발생, 0 타임아웃, <0 에러
        ret = gpiod_line_event_wait(ir, &timeout);
        
        if (ret < 0) {
            // 에러 발생
            perror("Error waiting for IR event");
            break;  // 루프 종료
        } else if (ret == 0) {
            // 타임아웃 - 1초 동안 이벤트 없음
            // 다시 대기하러 계속 진행
            continue;
        }

        // ========== 이벤트 발생 - 이벤트 읽기 ==========
        // event 구조체에 이벤트 정보 저장
        ret = gpiod_line_event_read(ir, &event);
        if (ret < 0) {
            perror("Error reading IR event");
            continue;  // 다음 이벤트 대기
        }

        // ========== IR 센서가 물체 감지 ==========
        printf("\nIR 센서 감지! 초음파 측정 시작...\n");
        num++;                            // 측정 횟수 증가
        ir_detected = true;               // IR 트리거 플래그 설정

        // ========== 센서 안정화 대기 ==========
        // 10ms (10,000 마이크로초) 대기
        usleep(10000);

        // ========== 초음파 센서 트리거 신호 발생 ==========
        // 10us 펄스를 보내서 초음파 발생 요청
        
        gpiod_line_set_value(trig, 0);    // LOW로 설정 (안정화)
        usleep(2);                        // 2us 대기
        
        gpiod_line_set_value(trig, 1);    // HIGH로 설정 (펄스 시작)
        usleep(10);                       // 10us 대기 (펄스 유지)
        
        gpiod_line_set_value(trig, 0);    // LOW로 설정 (펄스 종료)
        // 이제 센서가 초음파를 발생시킴

        // ========== 에코 신호 HIGH 대기 ==========
        // 초음파가 발사되면 에코 핀이 HIGH가 됨
        timeout_count = 0;                // 타임아웃 카운터 초기화
        
        while(gpiod_line_get_value(echo) == 0)  // LOW인 동안 계속 대기
        {
            usleep(1);                    // 1us 대기
            if(++timeout_count > 30000) { // 30ms 이상 대기하면
                printf("Echo timeout (waiting for HIGH)\n");
                break;                    // 타임아웃으로 중단
            }
        }

        // 타임아웃 발생 시 이번 측정 건너뛰기
        if(timeout_count > 30000) {
            ir_detected = false;
            continue;
        }

        // ========== 에코 신호 시작 시간 기록 ==========
        // 에코 핀이 HIGH가 된 시점 = 초음파 발사 시점
        clock_gettime(CLOCK_MONOTONIC, &start);

        // ========== 에코 신호 LOW 대기 ==========
        // 초음파가 물체에 반사되어 돌아오면 에코 핀이 LOW가 됨
        timeout_count = 0;                // 타임아웃 카운터 초기화
        
        while(gpiod_line_get_value(echo) == 1)  // HIGH인 동안 계속 대기
        {
            usleep(1);                    // 1us 대기
            if(++timeout_count > 30000) { // 30ms 이상 대기하면
                printf("Echo timeout (waiting for LOW)\n");
                break;                    // 타임아웃으로 중단
            }
        }

        // 타임아웃 발생 시 이번 측정 건너뛰기
        if(timeout_count > 30000) {
            ir_detected = false;
            continue;
        }

        // ========== 에코 신호 종료 시간 기록 ==========
        // 에코 핀이 LOW가 된 시점 = 초음파 수신 시점
        clock_gettime(CLOCK_MONOTONIC, &end);

        // ========== 거리 계산 ==========
        // 시간 차이 계산 (초 단위)
        // tv_sec: 초, tv_nsec: 나노초 (1초 = 10억 나노초)
        double time_sec = (end.tv_sec - start.tv_sec) +      // 초 차이
                         (end.tv_nsec - start.tv_nsec) / 1000000000.0;  // 나노초 → 초

        // 거리 = (시간 × 음속) / 2
        // 음속 = 343 m/s = 34300 cm/s
        // 2로 나누는 이유: 왕복 거리이므로 (보낸 거리 + 돌아온 거리)
        double distance = (time_sec * 34300.0) / 2.0;

        // ========== 유효 범위 체크 ==========
        // HC-SR04 센서의 측정 범위: 2cm ~ 400cm
        if(distance >= 2.0 && distance <= 400.0) {
            // ========== 측정 결과 출력 ==========
            printf("측정 거리: %.2f cm\n", distance);
            
            // ========== LED 제어 ==========
            if(distance < THRESHOLD) {
                // 임계값(20cm)보다 가까우면 LED 켜기
                gpiod_line_set_value(led, 1);  // LED ON (HIGH)
                printf("LED ON - 물체가 %.2f cm 이내에 있습니다!\n", THRESHOLD);
            } else {
                // 임계값보다 멀면 LED 끄기
                gpiod_line_set_value(led, 0);  // LED OFF (LOW)
                printf("LED OFF - 안전 거리 (%.2f cm)\n", distance);
            }
            
            // ========== 데이터베이스에 저장 ==========
            // SQL INSERT 문 생성 (snprintf: 버퍼 오버플로우 방지)
            snprintf(sql, sizeof(sql), 
                    "INSERT INTO ultrasonic(measurement_num, distance, ir_triggered) "
                    "VALUES(%d, %.2f, %d);",
                    num,                      // 측정 번호
                    distance,                 // 거리
                    ir_detected ? 1 : 0);     // IR 트리거 여부 (true→1, false→0)
            
            // SQL 실행
            rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
            if (rc != SQLITE_OK) {
                // 에러 발생 시 메시지 출력
                fprintf(stderr, "SQL error: %s\n", err_msg);
                sqlite3_free(err_msg);    // 에러 메시지 메모리 해제
            }
        } else {
            // 측정 범위 밖 (센서 오류 또는 거리 초과)
            printf("측정 범위 초과: %.2f cm\n", distance);
        }

        // ========== 다음 측정 준비 ==========
        ir_detected = false;              // IR 플래그 리셋
        usleep(500000);                   // 0.5초 대기 (센서 안정화)
        // 다시 IR 센서 인터럽트 대기로 돌아감
    }

    // ========== 프로그램 종료 처리 ==========
    // Ctrl+C 입력 시 또는 에러로 루프 탈출 시 실행
    printf("\n프로그램 종료 중...\n");
    
    // LED 끄기 (안전을 위해)
    gpiod_line_set_value(led, 0);
    
    // GPIO 라인 해제 (리소스 반환)
    gpiod_line_release(trig);
    gpiod_line_release(echo);
    gpiod_line_release(ir);
    gpiod_line_release(led);
    
    // GPIO 칩 닫기
    gpiod_chip_close(chip);
    
    // 데이터베이스 닫기
    sqlite3_close(db);
    
    // 종료 메시지
    printf("총 %d개의 IR 트리거 이벤트가 처리되었습니다.\n", num);

    return 0;  // 정상 종료
}

// ========== 에러 체크 함수 ==========
// is_error가 true면 error_code에 해당하는 메시지 출력 후 종료
void check_error(int is_error, int error_code)
{
    if (is_error)  // 에러 조건이 참이면
    {
        // error_code에 따라 적절한 메시지 출력
        switch(error_code)
        {
            case 0: perror("Error: DB Failed"); break;
            case 1: perror("Error: Chip Open Failed"); break;
            case 2: perror("Error: Trig Pin Failed"); break;
            case 3: perror("Error: Echo Pin Failed"); break;
            case 4: perror("Error: IR Pin Failed"); break;
            case 5: perror("Error: LED Pin Failed"); break;
            case 6: perror("Error: Trig Output Mode Failed"); break;
            case 7: perror("Error: Echo Input Mode Failed"); break;
            case 8: perror("Error: IR Interrupt Mode Failed"); break;
            case 9: perror("Error: LED Output Mode Failed"); break;
            default: perror("Error: Unknown Error"); break;
        }
        exit(1);  // 프로그램 종료 (에러 코드 1)
    }
}

// ========== 시그널 핸들러 함수 ==========
// Ctrl+C (SIGINT) 입력 시 호출됨
void signal_handler(int sig)
{
    if (sig == SIGINT) {  // SIGINT 시그널이면
        printf("\n종료 신호를 받았습니다...\n");
        running = false;  // 메인 루프 종료 플래그 설정
        // 이제 while(running) 루프가 종료됨
    }
}