#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

// I2C 장치 파일 경로 (보통 라즈베리파이는 1번 버스 사용)
#define I2C_DEV "/dev/i2c-1" 
#define I2C_ADDR 0x27 // LCD 주소 (0x27 또는 0x3F가 일반적)

// LCD 제어 상수 정의
#define LCD_CHR 1 // 문자 전송 모드 (RS=1)
#define LCD_CMD 0 // 명령어 전송 모드 (RS=0)

#define LCD_BACKLIGHT 0x08 // 백라이트 On
#define ENABLE 0x04 // Enable 비트

#define LINE1 0x80 // 첫 번째 줄
#define LINE2 0xC0 // 두 번째 줄

int file_i2c;

// 함수 원형 선언
void i2c_write_byte(unsigned char val);
void lcd_toggle_enable(int bits);
void lcd_byte(int bits, int mode);
void lcd_init();
void lcd_string(const char *s);
void lcd_loc(int line);

int main(void) 
{
    // 1. I2C 버스 열기
    if ((file_i2c = open(I2C_DEV, O_RDWR)) < 0) 
    {
        perror("Failed to open the i2c bus");
        return 1;
    }

    // 2. I2C 장치(LCD) 연결
    if (ioctl(file_i2c, I2C_SLAVE, I2C_ADDR) < 0) 
    {
        perror("Failed to acquire bus access and/or talk to slave");
        return 1;
    }

    lcd_init(); // LCD 초기화

    while (1) 
    {
        lcd_byte(0x01, LCD_CMD); // 화면 지우기 (Clear Display)
        usleep(2000); // Clear 명령어는 시간이 좀 더 필요함

        lcd_loc(LINE1);
        lcd_string("Hello World");

        lcd_loc(LINE2);
        lcd_string("Hello Linux");

        sleep(3);

        lcd_byte(0x01, LCD_CMD); // 화면 지우기
        usleep(2000);

        lcd_loc(LINE1);
        lcd_string("I2C Test");

        lcd_loc(LINE2);
        lcd_string("Success!");

        sleep(3);
    }

    close(file_i2c);
    return 0;
}

// I2C로 바이트 전송 (Low level)
void i2c_write_byte(unsigned char val) 
{
    if (write(file_i2c, &val, 1) != 1) 
    {
        perror("I2C Write Error");
    }
}

// LCD Enable 핀 토글 (신호를 LCD가 읽어가도록 펄스를 줌)
void lcd_toggle_enable(int bits) 
{
    usleep(500);
    i2c_write_byte(bits | ENABLE); // Enable High
    usleep(500);
    i2c_write_byte(bits & ~ENABLE); // Enable Low
    usleep(500);
}

// 데이터 전송 (4비트 모드 처리)
void lcd_byte(int bits, int mode) 
{
    int bits_high;
    int bits_low;

    // 상위 4비트 데이터 + 모드(RS) + 백라이트
    bits_high = mode | (bits & 0xF0) | LCD_BACKLIGHT;
    // 하위 4비트 데이터 + 모드(RS) + 백라이트
    bits_low = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT;

    // 상위 비트 전송
    i2c_write_byte(bits_high);
    lcd_toggle_enable(bits_high);

    // 하위 비트 전송
    i2c_write_byte(bits_low);
    lcd_toggle_enable(bits_low);
}

// LCD 초기화 루틴 (HD44780 표준 4비트 초기화)
void lcd_init() 
{
    lcd_byte(0x33, LCD_CMD); // 초기화 1
    lcd_byte(0x32, LCD_CMD); // 초기화 2 (4비트 모드 진입)
    lcd_byte(0x06, LCD_CMD); // 커서 이동 방향 설정
    lcd_byte(0x0C, LCD_CMD); // 화면 켜기, 커서 끄기, 깜빡임 끄기
    lcd_byte(0x28, LCD_CMD); // 2줄 모드, 5x7 도트
    lcd_byte(0x01, LCD_CMD); // 화면 지우기
    usleep(5000);            // 초기화 후 충분한 대기 시간
}

// 문자열 출력 함수
void lcd_string(const char *s) 
{
    while (*s) 
    {
        lcd_byte(*(s++), LCD_CHR);
    }
}

// 커서 위치 이동
void lcd_loc(int line) 
{
    lcd_byte(line, LCD_CMD);
}