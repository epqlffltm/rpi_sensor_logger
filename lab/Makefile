#makefile
CC = gcc
CFLAGS = -W -Wall
LDLIBS = -lgpiod

# 1. 현재 디렉터리의 모든 .c 파일을 찾음 (예: add.c input.c)
SRCS = $(wildcard *.c)

# 2. .c 확장자를 떼어내서 실행 파일 이름 목록 생성 (예: add input)
TARGETS = $(SRCS:.c=)

# 3. 'make'만 쳤을 때 모든 실행 파일을 만들도록 설정
all: $(TARGETS)

# 4. 일반화된 규칙: 이름이 %인 타겟은 %.c를 의존성으로 가짐
# $@ : 타겟 이름 (예: add)
# $< : 첫 번째 의존성 파일 (예: add.c)
%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

# 5. 생성된 실행 파일들 삭제
clean:
	rm -f $(TARGETS)