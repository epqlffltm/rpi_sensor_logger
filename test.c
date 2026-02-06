/*
MPU6050 상태 확인
*/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

uint8_t read_register(int file, uint8_t reg) {
    uint8_t value;
    write(file, &reg, 1);
    usleep(1000);
    read(file, &value, 1);
    return value;
}

void write_register(int file, uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    write(file, data, 2);
    usleep(10000);
}

int main(void)
{
    int file = open("/dev/i2c-1", O_RDWR);
    if (file < 0) {
        perror("Failed to open I2C");
        exit(1);
    }

    if (ioctl(file, I2C_SLAVE, 0x68) < 0) {
        perror("Failed to set slave address");
        exit(1);
    }

    printf("=== MPU6050 레지스터 확인 ===\n\n");

    // 1. WHO_AM_I
    uint8_t who = read_register(file, 0x75);
    printf("WHO_AM_I (0x75): 0x%02X ", who);
    if (who == 0x68) printf("✓ 정상\n");
    else printf("✗ 비정상 (예상: 0x68)\n");

    // 2. Power Management
    uint8_t pwr = read_register(file, 0x6B);
    printf("PWR_MGMT_1 (0x6B): 0x%02X ", pwr);
    if (pwr & 0x40) printf("✗ SLEEP 모드! (깨우기 필요)\n");
    else if (pwr == 0x00) printf("✓ 정상 작동\n");
    else printf("? 알 수 없는 상태\n");

    // 3. 강제로 깨우기
    printf("\n센서 깨우는 중...\n");
    write_register(file, 0x6B, 0x80);  // Reset
    usleep(100000);
    write_register(file, 0x6B, 0x00);  // Wake up
    usleep(100000);

    pwr = read_register(file, 0x6B);
    printf("PWR_MGMT_1 (0x6B): 0x%02X ", pwr);
    if (pwr == 0x00) printf("✓ 깨우기 성공!\n");
    else printf("✗ 여전히 문제 있음\n");

    // 4. 가속도 데이터 읽기
    printf("\n=== 가속도 데이터 읽기 ===\n");
    uint8_t reg = 0x3B;
    uint8_t data[6];
    
    write(file, &reg, 1);
    usleep(1000);
    read(file, data, 6);

    int16_t ax = (int16_t)((data[0] << 8) | data[1]);
    int16_t ay = (int16_t)((data[2] << 8) | data[3]);
    int16_t az = (int16_t)((data[4] << 8) | data[5]);

    printf("Raw 데이터:\n");
    printf("  Byte[0-1]: 0x%02X%02X → Acc X = %d\n", data[0], data[1], ax);
    printf("  Byte[2-3]: 0x%02X%02X → Acc Y = %d\n", data[2], data[3], ay);
    printf("  Byte[4-5]: 0x%02X%02X → Acc Z = %d\n", data[4], data[5], az);

    if (ax == 0 && ay == 0 && az == 0) {
        printf("\n✗ 모든 값이 0 → 센서 미작동\n");
        printf("가능한 원인:\n");
        printf("  1. VCC 전원 문제 (3.3V 확인)\n");
        printf("  2. 센서 불량\n");
        printf("  3. AD0 핀 상태 확인\n");
    } else if (az > 10000 || az < -10000) {
        printf("\n✓ 정상 데이터! (Z축 중력 감지)\n");
    } else {
        printf("\n? 값이 너무 작음 (센서 범위 설정 확인)\n");
    }

    close(file);
    return 0;
}