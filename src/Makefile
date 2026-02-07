# 1. 컴파일러 및 플래그 설정
CC = gcc
# -Wall: 모든 경고 출력, -O2: 최적화, -g: 디버깅 정보 포함
CFLAGS = -Wall -O2 -g
LDLIBS = -lgpiod -lsqlite3

# 2. 파일 및 타겟 설정
# 현재 디렉터리의 모든 .c 파일을 타겟으로 설정
SRCS = $(wildcard *.c)
TARGETS = $(SRCS:.c=)
# 메인 실행 파일 이름 (run 명령에서 사용)
MAIN_TARGET = ir_ultrasonic_sensor

# 3. 가상 타겟(Phony Targets) 설정
# 파일 이름과 명령어 중복 방지
.PHONY: all clean run view_db stats help

# 4. 기본 빌드 규칙
all: $(TARGETS)

# 각 .c 파일을 개별 실행 파일로 컴파일
%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

# 5. 실행 편의 기능 (메인 프로그램 실행)
run: $(MAIN_TARGET)
	@echo "프로그램을 실행합니다 (sudo 권한 필요)..."
	@echo "종료하려면 Ctrl+C를 누르세요"
	sudo ./$(MAIN_TARGET)

# 6. 데이터베이스 조회 기능 (개선됨)
view_db:
	@echo "========================================="
	@echo "SQLite 데이터베이스 내용 조회"
	@echo "========================================="
	@if [ -f ultrasonic.db ]; then \
		sqlite3 -column -header ultrasonic.db \
		"SELECT id, measurement_num AS '측정번호', \
		 printf('%.2f', distance) AS '거리(cm)', \
		 CASE ir_triggered WHEN 1 THEN 'O' ELSE 'X' END AS 'IR감지', \
		 timestamp AS '시간' \
		 FROM ultrasonic ORDER BY id DESC LIMIT 20;"; \
		echo ""; \
		echo "(최근 20개 데이터만 표시)"; \
	else \
		echo "❌ 데이터베이스 파일(ultrasonic.db)이 없습니다."; \
		echo "먼저 'make run'으로 프로그램을 실행하세요."; \
	fi

# 7. 데이터베이스 통계 보기
stats:
	@echo "========================================="
	@echo "측정 데이터 통계"
	@echo "========================================="
	@if [ -f ultrasonic.db ]; then \
		sqlite3 ultrasonic.db \
		"SELECT \
		COUNT(*) AS '총 측정 횟수', \
		printf('%.2f cm', AVG(distance)) AS '평균 거리', \
		printf('%.2f cm', MIN(distance)) AS '최소 거리', \
		printf('%.2f cm', MAX(distance)) AS '최대 거리', \
		SUM(ir_triggered) AS 'IR 트리거 횟수' \
		FROM ultrasonic;"; \
	else \
		echo "❌ 데이터베이스 파일이 없습니다."; \
	fi

# 8. 실행 중인 프로그램 종료
stop:
	@echo "실행 중인 $(MAIN_TARGET) 프로세스를 종료합니다..."
	@pkill -f $(MAIN_TARGET) || echo "실행 중인 프로세스가 없습니다."

# 9. 정리 규칙
clean:
	@echo "빌드 파일 및 데이터베이스를 삭제합니다..."
	rm -f $(TARGETS)
	rm -f *.db
	@echo "✅ 정리 완료"

# 10. 도움말
help:
	@echo "========================================="
	@echo "사용 가능한 명령어:"
	@echo "========================================="
	@echo "make         - 모든 .c 파일 컴파일"
	@echo "make run     - 메인 프로그램 실행 (sudo)"
	@echo "make view_db - 데이터베이스 내용 조회 (최근 20개)"
	@echo "make stats   - 측정 데이터 통계 보기"
	@echo "make stop    - 실행 중인 프로그램 종료"
	@echo "make clean   - 빌드 파일 및 DB 삭제"
	@echo "make help    - 이 도움말 표시"
	@echo "========================================="