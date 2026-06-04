# veda_linux_project2 사용 문서

## 1. 프로젝트 개요

이 프로젝트는 TCP 소켓 기반 장치 제어 시스템이다.

서버는 데몬 프로세스로 동작하며 LED, BUZZER, LIGHT SENSOR, FND 장치를 제어한다. 각 장치 제어 코드는 동적 라이브러리(`.so`)로 분리되어 있고, 서버는 실행 중 `dlopen()`과 `dlsym()`을 사용하여 장치 제어 함수를 로드한다.

클라이언트는 메뉴 기반 CLI 프로그램이며 TCP 5100 포트로 서버에 명령을 전송한다. 추가 기능으로 C 기반 웹 상태 서버를 제공하여 브라우저에서 현재 장치 상태를 확인할 수 있다.

## 2. 빌드 방법

### 전체 빌드

```bash
cd ~/veda_linux_project2
make
```

`make`는 서버와 클라이언트를 빌드한다.

### 서버 빌드

```bash
cd ~/veda_linux_project2
make server
```

생성 파일:

```text
exec/server
exec/lib/libled.so
exec/lib/libbuzzer.so
exec/lib/liblight.so
exec/lib/libfnd.so
```

`make server` 실행 시 `docs/running.txt`는 새 파일처럼 초기화된다.

### 클라이언트 빌드

```bash
cd ~/veda_linux_project2
make client
```

생성 파일:

```text
exec/client
```

다른 기기에서 wiringPi가 없고 클라이언트만 필요한 경우에는 `make client`만 실행하면 된다.

### 웹 상태 서버 빌드

```bash
cd ~/veda_linux_project2
make web
```

생성 파일:

```text
exec/web_status
```

### 정리

```bash
cd ~/veda_linux_project2
make clean
```

## 3. 실행 방법

### 서버 실행

서버는 동적 라이브러리를 `./lib/lib*.so` 경로에서 로드하므로 반드시 `exec` 디렉터리에서 실행한다.

```bash
cd ~/veda_linux_project2/exec
./server -d
```

서버 실행 확인:

```bash
ss -ltnp | grep 5100
```

정상 실행 예:

```text
LISTEN 0 8 0.0.0.0:5100 0.0.0.0:* users:(("server",pid=...,fd=...))
```

서버 종료:

```bash
ss -ltnp | grep 5100
kill <server_pid>
```

### 클라이언트 실행

서버와 같은 기기에서 실행:

```bash
cd ~/veda_linux_project2/exec
./client
```

다른 기기에서 서버에 접속:

```bash
cd ~/veda_linux_project2/exec
./client <server_ip>
```

예:

```bash
./client 192.168.0.25
```

클라이언트 메뉴:

```text
1. LED ON
2. LED OFF
3. Set Brightness
4. BUZZER ON (play melody)
5. BUZZER OFF (stop)
6. LIGHT SENSOR ON
7. LIGHT SENSOR OFF
8. SEGMENT DISPLAY
9. SEGMENT STOP
10. Exit
```

`LIGHT SENSOR ON`을 선택하면 클라이언트에는 현재 조도센서 값이 한 번 출력된다. 이후 서버는 내부 thread에서 센서 값을 주기적으로 읽으며, 빛이 감지되지 않으면 LED를 켜고 빛이 감지되면 LED를 끈다. 이 자동 제어는 `LIGHT SENSOR OFF`를 선택하면 중지된다.

### 웹 상태 서버 실행

웹 상태 서버는 장치 제어 서버와 별도로 실행한다. 기본 추천 포트는 8080이다.

```bash
cd ~/veda_linux_project2/exec
./web_status 8080
```

브라우저에서 접속:

```text
http://<server_ip>:8080
```

예:

```text
http://192.168.0.25:8080
```

웹 화면은 `exec/status.txt`를 읽어서 현재 장치 상태를 표시한다. 화면은 0.3초마다 자동 새로고침된다.

## 4. 로그 및 상태 파일

### 실행 로그

서버와 클라이언트는 모두 같은 로그 파일을 사용한다.

```text
docs/running.txt
```

로그 예:

```text
[SERVER] Server started
[SERVER] Client connected
[SERVER] Command received: LED ON
[SERVER] [LED] Device result: success
[CLIENT] Client started
[CLIENT] Sent command: led on
[CLIENT] Received response: Result: LED ON
```

### 장치 상태 파일

서버는 장치 상태가 변경될 때마다 다음 파일을 갱신한다.

```text
exec/status.txt
```

상태 파일 예:

```text
CLIENT     = CONNECTED
LED        = ON
BRIGHTNESS = MAX
BUZZER     = OFF
SENSOR     = ON
LIGHT      = NOT_DETECTED
FND        = 5
UPDATED    = 2026-06-04 18:17:30
```

## 5. 평가 기준 항목별 구현 여부

### 사용자 편의성

- 메뉴 기반 클라이언트를 제공하여 사용자가 번호 선택으로 장치를 제어할 수 있다.
- 서버와 클라이언트 로그를 `docs/running.txt`에 통합 저장한다.
- 로그에 `[SERVER]`, `[CLIENT]` prefix를 붙여 어느 프로그램에서 출력된 로그인지 구분할 수 있다.
- 웹 브라우저에서 장치 상태를 확인할 수 있는 웹 상태 대시보드를 제공한다.
- 웹 상태 화면에서 client 접속 여부를 우측 상단에 표시한다.
- client 접속 상태는 `CONNECTED`일 때 초록색, `DISCONNECTED`일 때 빨간색으로 표시한다.

### 문서

- 빌드 방법, 실행 방법, 서버 종료 방법을 문서화했다.
- 서버/클라이언트/웹 상태 서버의 역할을 문서화했다.
- 로그 파일과 상태 파일의 위치 및 예시를 문서화했다.
- 평가 기준별 구현 내용을 문서화했다.

### 추가기능

- C 기반 웹 상태 서버(`web_status`)를 추가했다.
- Node.js, Flask 같은 외부 웹 프레임워크 없이 C socket API만 사용하여 HTTP 응답을 생성한다.
- 서버가 `exec/status.txt`에 장치 상태를 저장하고, 웹 상태 서버가 이 파일을 읽어 HTML로 시각화한다.
- 웹 화면은 0.3초마다 자동 새로고침된다.
- LED 밝기는 숫자가 아니라 `MAX`, `MEDIUM`, `LOW/OFF`로 표시한다.
- FND는 마지막으로 기록된 숫자 상태를 표시한다.

## 6. 사용한 주요 기능 설명

### TCP Socket

서버는 TCP 5100 포트에서 클라이언트 연결을 기다린다. 클라이언트는 문자열 명령을 서버로 전송하고, 서버는 명령에 따라 장치 제어 함수를 호출한 뒤 결과를 응답한다.

### Daemon Process

서버는 `-d` 옵션으로 실행하면 `fork()`와 `setsid()`를 사용하여 데몬 프로세스로 동작한다.

### Dynamic Library

LED, BUZZER, LIGHT, FND 제어 코드는 각각 `.so` 파일로 빌드된다.

서버는 다음 함수를 사용하여 실행 중 동적으로 라이브러리를 로드한다.

```c
dlopen()
dlsym()
dlclose()
```

### Thread

서버는 `LIGHT SENSOR ON` 상태에서 센서 값을 주기적으로 읽고 LED를 자동 제어하기 위해 `pthread_create()`를 사용한다. 클라이언트 화면에는 센서 값을 한 번만 출력하고, 이후 상태 변화는 `exec/status.txt`와 웹 상태 대시보드에 반영된다. BUZZER 라이브러리도 멜로디 재생을 별도 thread에서 처리한다.

### Multi Process

클라이언트는 FND countdown 기능에서 `fork()`를 사용하여 countdown 처리를 별도 프로세스로 실행한다.

### File Based Status Sharing

장치 서버는 현재 상태를 `exec/status.txt`에 기록한다. 웹 상태 서버는 같은 파일을 읽어 브라우저에 HTML로 표시한다. 이를 통해 기존 TCP 제어 서버와 웹 상태 서버가 간단한 파일 기반 방식으로 상태를 공유한다.

### C Web Server

`web_status`는 C socket API로 HTTP 요청을 받고 HTML 응답을 직접 생성한다. 브라우저에서는 `http://<server_ip>:8080`으로 접속하여 장치 상태를 확인한다.

웹 상태 서버는 `syj_exercise/ch06_network/webserver.c`의 HTTP socket server 구조를 참고하여 구현했다. 기존 예제의 `socket()`, `bind()`, `listen()`, `accept()` 흐름은 유지하되, 정적 HTML 파일을 읽어 전송하는 방식 대신 `exec/status.txt`를 읽어 현재 장치 상태 HTML을 동적으로 생성하도록 변경했다.

## 7. 개발 일정

### 2026-06-02: 장치 제어 기능 구현

- LED on/off 제어와 밝기 단계 조절 기능을 구현했다.
- BUZZER on/off 제어와 멜로디 재생 기능을 구현했다.
- 조도센서 값을 읽고, 빛 감지 여부에 따라 LED를 on/off하는 기능을 구현했다.
- FND에 0~9 숫자를 표시하는 기능을 구현했다.
- LED, BUZZER, LIGHT SENSOR, FND 장치별 동작을 개별 소스 파일로 분리하여 이후 공유 라이브러리 구조로 확장할 수 있도록 정리했다.

### 2026-06-04 오전: 기본 서버/클라이언트 구조 구현

- TCP 기반 장치 제어 서버 구조를 작성했다.
- 클라이언트 메뉴 프로그램을 작성하여 LED, BUZZER, SENSOR, FND 명령을 서버로 전송하도록 구현했다.
- 서버는 TCP 5100 포트에서 클라이언트 연결을 받고 문자열 명령을 해석하도록 구성했다.

### 2026-06-04 오전: 동적 라이브러리 기반 장치 제어 구조 분리

- LED, BUZZER, LIGHT, FND 제어 코드를 각각 공유 라이브러리로 분리했다.
- 서버에서 `dlopen()`, `dlsym()`, `dlclose()`를 사용하여 장치 라이브러리를 실행 중 로드하도록 구현했다.
- 장치별 헤더 파일을 분리하여 각 라이브러리의 인터페이스를 명확히 정리했다.

### 2026-06-04 오후: 빌드 구조 및 실행 파일 배치 정리

- 루트 `Makefile`을 기준으로 전체 빌드를 자동화했다.
- 빌드 산출물이 `exec/` 아래에 모이도록 정리했다.
- `make server`, `make client`, `make web` 타깃을 분리하여 서버 장비와 클라이언트 장비에서 필요한 대상만 빌드할 수 있도록 개선했다.

### 2026-06-04 오후: 데몬, 멀티스레드, 멀티프로세스 기능 보완

- 서버를 `-d` 옵션으로 데몬 프로세스 형태로 실행할 수 있도록 구현했다.
- 조도센서 감지 기능은 서버에서 별도 thread로 주기적으로 동작하도록 구현했다.
- BUZZER 멜로디 재생도 별도 thread로 처리하여 다른 명령 처리를 막지 않도록 구성했다.
- FND countdown 기능은 클라이언트에서 `fork()`를 사용하여 별도 프로세스로 처리했다.

### 2026-06-04 오후: 사용자 편의성 및 로그 정리

- 클라이언트 메뉴를 정리하고 신호 처리를 추가하여 `SIGINT`로만 종료되도록 구성했다.
- 서버와 클라이언트 로그를 `docs/running.txt`로 통합했다.
- 로그에 `[SERVER]`, `[CLIENT]` prefix를 붙여 어느 프로그램에서 발생한 로그인지 구분되도록 개선했다.
- `make server` 실행 시 `docs/running.txt`를 초기화하도록 빌드 흐름을 정리했다.

### 2026-06-04 오후: 추가 기능 구현

- C socket API 기반 웹 상태 서버 `web_status`를 추가했다.
- 서버가 장치 상태를 `exec/status.txt`에 저장하도록 구현했다.
- 웹 상태 서버가 `exec/status.txt`를 읽어 브라우저에 상태 대시보드로 표시하도록 구현했다.
- 웹 화면에서 client 접속 여부, LED 상태, 밝기, BUZZER, SENSOR, LIGHT, FND, 갱신 시간을 확인할 수 있도록 구성했다.

### 2026-06-04 저녁: 문서화 및 제출 정리

- 빌드 방법, 실행 방법, 로그 파일, 상태 파일, 평가 기준별 구현 여부를 README에 정리했다.
- 추가 기능인 웹 상태 대시보드의 실행 방법과 동작 방식을 문서화했다.
- 불필요한 장치 내부 디버그 매크로(`device_log.h`, `DEVICE_LOG`)를 제거하여 로그 경로를 서버/클라이언트 통합 로그 중심으로 단순화했다.

## 8. 문제점 및 보완 사항

### 동적 라이브러리 실행 경로 문제

서버는 장치 공유 라이브러리를 `./lib/lib*.so` 경로로 로드한다. 따라서 프로젝트 루트에서 `./exec/server -d`로 실행하면 현재 작업 디렉터리 기준 `./lib`를 찾게 되어 `dlopen()` 실패가 발생할 수 있다.

보완:

- 실행 방법을 `cd ~/veda_linux_project2/exec && ./server -d`로 문서화했다.
- 빌드 산출물을 `exec/` 아래에 모아 서버 실행 위치와 라이브러리 위치가 일관되도록 정리했다.

### 클라이언트 장비의 wiringPi 의존성 문제

서버용 장치 라이브러리는 wiringPi를 사용하므로, 일반 우분투 클라이언트 장비에서 전체 `make`를 실행하면 `wiringPi.h`가 없어 빌드가 실패할 수 있다.

보완:

- `make server`, `make client`, `make web` 타깃을 분리했다.
- 클라이언트 장비에서는 `make client`만 실행하면 wiringPi 없이 클라이언트 실행 파일을 빌드할 수 있다.

### 센서 출력으로 인한 클라이언트 메뉴 가독성 문제

초기 구조에서는 조도센서 thread가 0.5초마다 client 화면에 값을 계속 출력할 수 있어 메뉴 입력 화면이 깨질 수 있었다.

보완:

- `LIGHT SENSOR ON` 선택 시 client에는 현재 조도센서 값을 한 번만 출력하도록 변경했다.
- 이후 서버 내부 thread가 센서 값을 주기적으로 읽고 LED를 자동 제어한다.
- 지속적인 센서 상태 확인은 `exec/status.txt`와 웹 상태 대시보드에서 확인하도록 역할을 분리했다.

### 서버의 단일 클라이언트 처리 구조

현재 서버는 한 번에 하나의 클라이언트 연결을 처리하는 구조이다. 여러 클라이언트가 동시에 접속하는 환경에서는 다음 클라이언트 처리가 지연될 수 있다.

보완 방향:

- 추후 `pthread_create()`를 사용하여 client 연결별 thread를 생성하면 다중 클라이언트 처리가 가능하다.
- 또는 `select()`/`epoll()` 기반 구조로 확장할 수 있다.

### 상태 공유 방식의 한계

웹 상태 서버는 `exec/status.txt` 파일을 읽어 상태를 표시한다. 구현이 단순하고 디버깅이 쉽지만, 엄밀한 동기화가 필요한 대규모 시스템에는 적합하지 않을 수 있다.

보완 방향:

- 추후 System V shared memory 또는 POSIX shared memory를 사용하면 서버와 웹 상태 서버 간 상태 공유를 더 직접적으로 처리할 수 있다.
- 현재 과제 범위에서는 파일 기반 상태 공유가 구현 난이도와 안정성 측면에서 적합하다고 판단했다.
