/*
파일명: ir_ultrasonic_lcd.c
작성일: 2026-02-08
설명: IR 센서로 물체 감지 시 초음파 센서로 거리 측정 후 LED 제어
      거리 데이터는 SQLite DB에 저장하고 I2C LCD에 표시
 */

#include <stdio.h>      // printf, fprintf 등 표준 입출력 함수
#include <stdlib.h>     // exit, malloc 등 표준 라이브러리 함수
#include <unistd.h>     // usleep (마이크로초 단위 대기) 함수
#include <time.h>       // clock_gettime (시간 측정) 함수
#include <stdbool.h>    // bool, true, false 타입 사용
#include <string.h>     // strlen, memset 등 문자열 함수
#include <fcntl.h>      // open() 함수
#include <sys/ioctl.h>  // ioctl() 함수 (I2C 제어)
#include <linux/i2c-dev.h>  // I2C 디바이스 제어용
#include <gpiod.h>      // GPIO 제어 라이브러리 (libgpiod)
#include <sqlite3.h>    // SQLite 데이터베이스 라이브러리
#include <signal.h>     // 시그널 처리 (Ctrl+C 감지)

// ========== LCD 관련 상수 정의 ==========
#define LCD_ADDR 0x27       // I2C LCD 주소 (일반적으로 0x27 또는 0x3F)
#define LCD_BACKLIGHT 0x08  // 백라이트 비트
#define LCD_ENABLE 0x04     // Enable 비트
#define LCD_RW 0x02         // Read/Write 비트 (0=쓰기)
#define LCD_RS 0x01         // Register Select 비트 (0=명령, 1=데이터)

// LCD 명령어
#define LCD_CLEAR 0x01      // 화면 지우기
#define LCD_HOME 0x02       // 커서 홈으로
#define LCD_ENTRY_MODE 0x06 // Entry mode: 커서 오른쪽 이동
#define LCD_DISPLAY_ON 0x0C // 디스플레이 켜기, 커서 끄기
#define LCD_FUNCTION_SET 0x28 // 4비트 모드, 2줄, 5x8 폰트
#define LCD_SET_DDRAM 0x80  // DDRAM 주소 설정

// ========== 전역 변수 ==========
volatile bool running = true;        // 프로그램 실행 상태
volatile bool ir_detected = false;   // IR 센서 감지 플래그
int i2c_fd = -1;                     // I2C 파일 디스크립터

// ========== 함수 선언 ==========
void check_error(int is_error, int error_code);
void signal_handler(int sig);

// LCD 관련 함수
int lcd_init(const char *i2c_device, int lcd_address);
void lcd_close(void);
void lcd_write_nibble(unsigned char data, unsigned char mode);
void lcd_write_byte(unsigned char data, unsigned char mode);
void lcd_command(unsigned char cmd);
void lcd_data(unsigned char data);
void lcd_clear(void);
void lcd_set_cursor(int row, int col);
void lcd_print(const char *str);
void lcd_printf(int row, int col, const char *format, ...);

int main(void)
{
  // ========== GPIO 핀 번호 및 상수 정의 ==========
  const char *chipname = "gpiochip0";
  const int trig_pin = 27;
  const int echo_pin = 17;
  const int ir_pin = 22;
  const int led_pin = 23;
  const double THRESHOLD = 20.0;

  // ========== GPIO 관련 구조체 변수 ==========
  struct gpiod_chip *chip;
  struct gpiod_line *trig, *echo;
  struct gpiod_line *ir, *led;
  struct timespec start, end;
  struct timespec timeout;
  struct gpiod_line_event event;

  // ========== 일반 변수 ==========
  int error_code = 0;
  int ret = 0;
  int num = 0;
  int timeout_count = 0;

  // ========== SQLite 관련 변수 ==========
  sqlite3 *db;
  char *err_msg = NULL;
  char sql[256];
  
  // ========== 시그널 핸들러 등록 ==========
  signal(SIGINT, signal_handler);

  // ========== I2C LCD 초기화 ==========
  printf("I2C LCD 초기화 중...\n");
  if (lcd_init("/dev/i2c-1", LCD_ADDR) < 0) 
  {
    fprintf(stderr, "LCD 초기화 실패!\n");
    fprintf(stderr, "다음을 확인하세요:\n");
    fprintf(stderr, "1. I2C가 활성화되었나요? (sudo raspi-config)\n");
    fprintf(stderr, "2. LCD 주소가 0x27인가요? (i2cdetect -y 1로 확인)\n");
    fprintf(stderr, "3. 배선이 올바른가요? (SDA->GPIO2, SCL->GPIO3)\n");
    exit(1);
  }

  // LCD 시작 메시지
  lcd_clear();
  lcd_print("IR+Ultrasonic");
  lcd_set_cursor(1, 0);
  lcd_print("System Ready");
  sleep(2);

  // ========== SQLite 데이터베이스 초기화 ==========
  int rc = sqlite3_open("ultrasonic.db", &db);
  check_error(rc != SQLITE_OK, error_code);

  const char *create_table_sql = 
      "CREATE TABLE IF NOT EXISTS ultrasonic("
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "measurement_num INT, "
      "distance REAL, "
      "ir_triggered BOOL, "
      "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";
    
  rc = sqlite3_exec(db, create_table_sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) 
  {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    lcd_close();
    exit(1);
  }

  // ========== GPIO 칩 열기 ==========
  chip = gpiod_chip_open_by_name(chipname);
  error_code = 1;
  check_error(chip == NULL, error_code);

  // ========== 트리거 핀 설정 ==========
  trig = gpiod_chip_get_line(chip, trig_pin);
  error_code = 2;
  check_error(trig == NULL, error_code);

  // ========== 에코 핀 설정 ==========
  echo = gpiod_chip_get_line(chip, echo_pin);
  error_code = 3;
  check_error(echo == NULL, error_code);

  // ========== IR 센서 핀 설정 ==========
  ir = gpiod_chip_get_line(chip, ir_pin);
  error_code = 4;
  check_error(ir == NULL, error_code);

  // ========== LED 핀 설정 ==========
  led = gpiod_chip_get_line(chip, led_pin);
  error_code = 5;
  check_error(led == NULL, error_code);

  // ========== 핀 모드 설정 ==========
  ret = gpiod_line_request_output(trig, "trig", 0);
  error_code = 6;
  check_error(ret < 0, error_code);

  ret = gpiod_line_request_input(echo, "echo");
  error_code = 7;
  check_error(ret < 0, error_code);
  
  ret = gpiod_line_request_falling_edge_events(ir, "ir_sensor");
  error_code = 8;
  check_error(ret < 0, error_code);

  ret = gpiod_line_request_output(led, "led", 0);
  error_code = 9;
  check_error(ret < 0, error_code);

  // ========== 시작 메시지 출력 ==========
  printf("IR 센서 + 초음파 센서 + LCD 통합 시스템 시작\n");
  printf("IR 센서가 물체를 감지하면 초음파로 거리 측정\n");
  printf("거리 %.1f cm 이내면 LED ON\n\n", THRESHOLD);

  // LCD 대기 화면
  lcd_clear();
  lcd_print("Waiting for");
  lcd_set_cursor(1, 0);
  lcd_print("IR Detection...");

  // ========== 인터럽트 대기 타임아웃 설정 ==========
  timeout.tv_sec = 1;
  timeout.tv_nsec = 0;

  // ========== 메인 루프 ==========
  while(running)
  {
    // ========== IR 센서 인터럽트 대기 ==========
    ret = gpiod_line_event_wait(ir, &timeout);

    if (ret < 0) 
    {
      perror("Error waiting for IR event");
      break;
    }
    else if (ret == 0) 
    {
      continue;
    }

    // ========== 이벤트 읽기 ==========
    ret = gpiod_line_event_read(ir, &event);
    if (ret < 0) 
    {
      perror("Error reading IR event");
      continue;
    }

    // ========== IR 센서가 물체 감지 ==========
    printf("\nIR 센서 감지! 초음파 측정 시작...\n");
    num++;
    ir_detected = true;

    // LCD에 감지 표시
    lcd_clear();
    lcd_print("IR Detected!");
    lcd_set_cursor(1, 0);
    lcd_print("Measuring...");

    // ========== 센서 안정화 대기 ==========
    usleep(10000);

    // ========== 초음파 센서 트리거 신호 발생 ==========
    gpiod_line_set_value(trig, 0);
    usleep(2);

    gpiod_line_set_value(trig, 1);
    usleep(10);
    
    gpiod_line_set_value(trig, 0);

    // ========== 에코 신호 HIGH 대기 ==========
    timeout_count = 0;
    
    while(gpiod_line_get_value(echo) == 0)
    {
      usleep(1);
      if(++timeout_count > 30000) 
      {
        printf("Echo timeout (waiting for HIGH)\n");
        lcd_clear();
        lcd_print("Timeout Error!");
        lcd_set_cursor(1, 0);
        lcd_print("Please retry");
        sleep(2);
        break;
      }
    }

    if(timeout_count > 30000) 
    {
      ir_detected = false;
      lcd_clear();
      lcd_print("Waiting for");
      lcd_set_cursor(1, 0);
      lcd_print("IR Detection...");
      continue;
    }

    // ========== 에코 신호 시작 시간 기록 ==========
    clock_gettime(CLOCK_MONOTONIC, &start);

    // ========== 에코 신호 LOW 대기 ==========
    timeout_count = 0;

    while(gpiod_line_get_value(echo) == 1)
    {
      usleep(1);
      if(++timeout_count > 30000) 
      {
        printf("Echo timeout (waiting for LOW)\n");
        lcd_clear();
        lcd_print("Timeout Error!");
        lcd_set_cursor(1, 0);
        lcd_print("Please retry");
        sleep(2);
        break;
      }
    }

    if(timeout_count > 30000)
    {
      ir_detected = false;
      lcd_clear();
      lcd_print("Waiting for");
      lcd_set_cursor(1, 0);
      lcd_print("IR Detection...");
      continue;
    }

    // ========== 에코 신호 종료 시간 기록 ==========
    clock_gettime(CLOCK_MONOTONIC, &end);

    // ========== 거리 계산 ==========
    double time_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1000000000.0;
    double distance = (time_sec * 34300.0) / 2.0;

    // ========== 유효 범위 체크 ==========
    if(distance >= 2.0 && distance <= 400.0) {
    // ========== 측정 결과 출력 ==========
    printf("측정 거리: %.2f cm\n", distance);

    // ========== LCD에 거리 표시 ==========
    lcd_clear();
    lcd_printf(0, 0, "Dist: %.1fcm #%d", distance, num);

    // ========== LED 제어 ==========
    if(distance < THRESHOLD) 
    {
      gpiod_line_set_value(led, 1);
      printf("LED ON - 물체가 %.2f cm 이내에 있습니다!\n", THRESHOLD);
      // LCD에 경고 표시
      lcd_set_cursor(1, 0);
      lcd_print("LED ON! CLOSE!");
    }
    else
    {
      gpiod_line_set_value(led, 0);
      printf("LED OFF - 안전 거리 (%.2f cm)\n", distance);
      
      // LCD에 안전 표시
      lcd_set_cursor(1, 0);
      lcd_print("LED OFF - Safe");
    }
            
    // ========== 데이터베이스에 저장 ==========
    snprintf(sql, sizeof(sql), 
    "INSERT INTO ultrasonic(measurement_num, distance, ir_triggered) "
    "VALUES(%d, %.2f, %d);",
    num, distance, ir_detected ? 1 : 0);
            
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK)
    {
      fprintf(stderr, "SQL error: %s\n", err_msg);
      sqlite3_free(err_msg);
    }
            
    // 측정 결과 2초간 표시
    sleep(2);
    }
    else
    {
      printf("측정 범위 초과: %.2f cm\n", distance);
            
      // LCD에 에러 표시
      lcd_clear();
      lcd_print("Out of Range!");
      lcd_set_cursor(1, 0);
      lcd_printf(1, 0, "%.1f cm", distance);
      sleep(2);
    }

    // ========== 다음 측정 준비 ==========
    ir_detected = false;
        
    // LCD 대기 화면으로 복귀
    lcd_clear();
    lcd_print("Waiting for");
    lcd_set_cursor(1, 0);
    lcd_print("IR Detection...");
        
    usleep(500000);
  }

  // ========== 프로그램 종료 처리 ==========
  printf("\n프로그램 종료 중...\n");
    
  // LCD 종료 메시지
  lcd_clear();
  lcd_print("System");
  lcd_set_cursor(1, 0);
  lcd_print("Shutting Down");
  sleep(1);
    
  gpiod_line_set_value(led, 0);
    
  gpiod_line_release(trig);
  gpiod_line_release(echo);
  gpiod_line_release(ir);
  gpiod_line_release(led);
    
  gpiod_chip_close(chip);
  sqlite3_close(db);
  lcd_close();
    
  printf("총 %d개의 IR 트리거 이벤트가 처리되었습니다.\n", num);

  return 0;
}

// ========== LCD 초기화 함수 ==========
int lcd_init(const char *i2c_device, int lcd_address)
{
  // I2C 디바이스 열기
  i2c_fd = open(i2c_device, O_RDWR);
  if (i2c_fd < 0) 
  {
    perror("Failed to open I2C device");
    return -1;
  }

  // I2C 슬레이브 주소 설정
  if (ioctl(i2c_fd, I2C_SLAVE, lcd_address) < 0) 
  {
    perror("Failed to set I2C slave address");
    close(i2c_fd);
    i2c_fd = -1;
    return -1;
  }

  // LCD 초기화 시퀀스 (4비트 모드)
  usleep(50000);  // 50ms 대기 (전원 안정화)
    
  // 8비트 모드로 3번 시도 (리셋)
  lcd_write_nibble(0x03 << 4, 0);
  usleep(4500);
  lcd_write_nibble(0x03 << 4, 0);
  usleep(4500);
  lcd_write_nibble(0x03 << 4, 0);
  usleep(150);
    
  // 4비트 모드로 전환
  lcd_write_nibble(0x02 << 4, 0);
  usleep(150);
    
  // Function Set: 4비트, 2줄, 5x8 폰트
  lcd_command(LCD_FUNCTION_SET);
  usleep(50);
    
  // Display ON/OFF Control: 디스플레이 켜기, 커서 끄기
  lcd_command(LCD_DISPLAY_ON);
  usleep(50);
    
  // Clear Display
  lcd_command(LCD_CLEAR);
  usleep(2000);  // 클리어 명령은 시간이 오래 걸림
    
  // Entry Mode Set: 커서 오른쪽 이동, 화면 이동 없음
  lcd_command(LCD_ENTRY_MODE);
  usleep(50);

  return 0;
}

// ========== LCD 닫기 함수 ==========
void lcd_close(void)
{
  if (i2c_fd >= 0)
  {
    lcd_clear();
    close(i2c_fd);
    i2c_fd = -1;
  }
}

// ========== 4비트 쓰기 함수 (Low Level) ==========
void lcd_write_nibble(unsigned char data, unsigned char mode)
{
  unsigned char byte = data | mode | LCD_BACKLIGHT;
    
  // Enable 신호로 데이터 전송
  (void)write(i2c_fd, &byte, 1);
  usleep(1);
  
  byte |= LCD_ENABLE;
  (void)write(i2c_fd, &byte, 1);
  usleep(1);
    
  byte &= ~LCD_ENABLE;
  (void)write(i2c_fd, &byte, 1);
  usleep(50);
}

// ========== 8비트 쓰기 함수 ==========
void lcd_write_byte(unsigned char data, unsigned char mode)
{
  // 상위 4비트 전송
  lcd_write_nibble(data & 0xF0, mode);
  // 하위 4비트 전송
  lcd_write_nibble((data << 4) & 0xF0, mode);
}

// ========== 명령 전송 함수 ==========
void lcd_command(unsigned char cmd)
{
  lcd_write_byte(cmd, 0);  // RS=0 (명령 모드)
}

// ========== 데이터 전송 함수 ==========
void lcd_data(unsigned char data)
{
  lcd_write_byte(data, LCD_RS);  // RS=1 (데이터 모드)
}

// ========== 화면 지우기 함수 ==========
void lcd_clear(void)
{
  lcd_command(LCD_CLEAR);
  usleep(2000);  // 클리어 명령은 시간이 오래 걸림
}

// ========== 커서 위치 설정 함수 ==========
void lcd_set_cursor(int row, int col)
{
  // 16x2 LCD의 DDRAM 주소
  // 첫 번째 줄: 0x00-0x0F
  // 두 번째 줄: 0x40-0x4F
  unsigned char address = (row == 0) ? 0x00 : 0x40;
  address += col;
  lcd_command(LCD_SET_DDRAM | address);
}

// ========== 문자열 출력 함수 ==========
void lcd_print(const char *str)
{
  while (*str) 
  {
    lcd_data(*str++);
  }
}

// ========== 포맷 문자열 출력 함수 (printf 스타일) ==========
void lcd_printf(int row, int col, const char *format, ...)
{
  char buffer[17];  // 16x2 LCD이므로 최대 16자 + NULL
  va_list args;
    
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
    
  lcd_set_cursor(row, col);
  lcd_print(buffer);
}

// ========== 에러 체크 함수 ==========
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
      case 4: perror("Error: IR Pin Failed"); break;
      case 5: perror("Error: LED Pin Failed"); break;
      case 6: perror("Error: Trig Output Mode Failed"); break;
      case 7: perror("Error: Echo Input Mode Failed"); break;
      case 8: perror("Error: IR Interrupt Mode Failed"); break;
      case 9: perror("Error: LED Output Mode Failed"); break;
      default: perror("Error: Unknown Error"); break;
    }
    
    if (i2c_fd >= 0)
    {
      lcd_close();
    }
    exit(1);
  }
}

// ========== 시그널 핸들러 함수 ==========
void signal_handler(int sig)
{
  if (sig == SIGINT)
  {
    printf("\n종료 신호를 받았습니다...\n");
    running = false;
  }
}