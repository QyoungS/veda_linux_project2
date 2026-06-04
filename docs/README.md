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
6. SENSOR ON
7. SENSOR OFF
8. SEGMENT DISPLAY
9. SEGMENT STOP
10. Exit
```

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

서버는 센서 값을 주기적으로 읽기 위해 `pthread_create()`를 사용한다. BUZZER 라이브러리도 멜로디 재생을 별도 thread에서 처리한다.

### Multi Process

클라이언트는 FND countdown 기능에서 `fork()`를 사용하여 countdown 처리를 별도 프로세스로 실행한다.

### File Based Status Sharing

장치 서버는 현재 상태를 `exec/status.txt`에 기록한다. 웹 상태 서버는 같은 파일을 읽어 브라우저에 HTML로 표시한다. 이를 통해 기존 TCP 제어 서버와 웹 상태 서버가 간단한 파일 기반 방식으로 상태를 공유한다.

### C Web Server

`web_status`는 C socket API로 HTTP 요청을 받고 HTML 응답을 직접 생성한다. 브라우저에서는 `http://<server_ip>:8080`으로 접속하여 장치 상태를 확인한다.
