#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>

#define DHT_PIN 7

int main(void) 
{
    if (wiringPiSetup() == -1) 
    exit(1);

    printf("WiringPi 준비 완료! 이제 센서 값을 읽어올 수 있습니다.\n");
    return 0;
}