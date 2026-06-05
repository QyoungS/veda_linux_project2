# veda_linux_project2 사용 문서

## 1. 빌드 방법

### 전체 빌드

```bash
cd ~/veda_linux_project2
make
```

`make`는 서버와 클라이언트를 빌드합니다.

주의: 깨끗한 환경에서 `make`를 실행하면 서버와 장치 공유 라이브러리까지 함께 빌드됩니다. 서버 장치 라이브러리는 `wiringPi`에 의존하므로, `wiringPi`가 설치되지 않은 클라이언트 PC에서는 전체 `make` 대신 `make client`만 실행하는 것이 안전합니다.

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

### 클라이언트 빌드

```bash
cd ~/veda_linux_project2
make client
```

생성 파일:

```text
exec/client
```

다른 기기에서 wiringPi가 없고 클라이언트만 필요한 경우에는 `make client`만 실행하면 됩니다.

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

## 2. 실행 방법

### 서버 실행

서버는 `./lib/lib*.so`와 `./exec/lib/lib*.so` 경로를 모두 확인하므로 프로젝트 루트와 `exec` 디렉터리 양쪽에서 실행할 수 있습니다.

프로젝트 루트에서 실행:

```bash
cd ~/veda_linux_project2
./exec/server -d
```

`exec` 디렉터리에서 실행:

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

`LIGHT SENSOR ON`을 선택하면 클라이언트에는 현재 조도센서 값이 한 번 출력됩니다. 이후 서버는 내부 thread에서 센서 값을 주기적으로 읽으며, 빛이 감지되지 않으면 LED를 켜고 빛이 감지되면 LED를 끕니다. 이 자동 제어는 `LIGHT SENSOR OFF`를 선택하면 중지됩니다.

### 웹 상태 서버 실행

웹 상태 서버는 장치 제어 서버와 별도로 실행합니다. 기본 추천 포트는 8080입니다.

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

웹 화면은 `exec/status.txt`를 읽어서 현재 장치 상태를 표시합니다. 화면은 0.3초마다 자동 새로고침됩니다.

### 상태 파일

서버는 장치 상태가 변경될 때마다 다음 파일을 갱신합니다.

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

## 3. 평가 기준 항목별 구현 여부

### 장치 제어 구현 여부

```text
[v] LED 제어
[v] 부저 제어
[v] 조도 센서 제어
[v] 7세그먼트 제어
```

| 평가 요소 | 구현 여부 | 코드 위치 |
| --- | --- | --- |
| LED on/off 제어 | [v] | `code/client/client.c:67`, `code/client/client.c:70`, `code/server/src/server.c:473`, `code/server/src/server.c:478`, `code/server/lib/led.c:37`, `code/server/lib/led.c:49` |
| LED 밝기 조절 | [v] | `code/client/client.c:72`, `code/client/client.c:79`, `code/client/client.c:93`, `code/server/src/server.c:483`, `code/server/lib/led.c:61` |
| 부저 on/off 제어 | [v] | `code/client/client.c:95`, `code/client/client.c:98`, `code/server/src/server.c:496`, `code/server/src/server.c:501`, `code/server/lib/buzzer.c:89`, `code/server/lib/buzzer.c:116` |
| 부저 음악 재생 thread | [v] | `code/server/lib/buzzer.c:58`, `code/server/lib/buzzer.c:64`, `code/server/lib/buzzer.c:104`, `code/server/lib/buzzer.c:111` |
| 조도 센서 값 확인 | [v] | `code/client/client.c:101`, `code/client/client.c:201`, `code/client/client.c:207`, `code/server/src/server.c:506`, `code/server/src/server.c:510`, `code/server/lib/light.c:24` |
| 조도 센서 값에 따른 LED 자동 제어 | [v] | `code/server/lib/light.c:30`, `code/server/lib/light.c:32`, `code/server/lib/light.c:34`, `code/server/lib/light.c:37`, `code/server/src/server.c:265`, `code/server/src/server.c:269` |
| 7세그먼트 숫자 표시 | [v] | `code/client/client.c:107`, `code/client/client.c:116`, `code/server/src/server.c:596`, `code/server/src/server.c:598`, `code/server/lib/fnd.c:28` |
| 7세그먼트 countdown 및 0 도달 시 부저 | [v] | `code/client/client.c:252`, `code/client/client.c:258`, `code/client/client.c:271`, `code/client/client.c:272`, `code/client/client.c:275`, `code/client/client.c:276`, `code/client/client.c:280` |

### 구현 내용

```text
[v] 메인 프로세스 사용
[v] 멀티 프로세스 사용
[v] 스레드 사용
[v] 데몬 프로세스 구현
[v] TCP 통신 구현
[v] shared library 사용
[v] 동적 라이브러리 로딩 사용
[v] 클라이언트 시그널 처리
[v] 빌드 자동화
```

| 평가 요소 | 구현 여부 | 코드 위치 |
| --- | --- | --- |
| 메인 프로세스 사용 | [v] | `code/server/src/server.c:292`, `code/client/client.c:305` |
| 멀티 프로세스 사용 | [v] | `code/client/client.c:252`, `code/client/client.c:258`, `code/client/client.c:264`, `code/client/client.c:283` |
| 스레드 사용 | [v] | `code/server/src/server.c:265`, `code/server/src/server.c:516`, `code/server/lib/buzzer.c:58`, `code/server/lib/buzzer.c:104` |
| 데몬 프로세스 구현 | [v] | `code/server/src/server.c:317`, `code/server/src/server.c:319`, `code/server/src/daemon.c:14`, `code/server/src/daemon.c:25`, `code/server/src/daemon.c:31` |
| TCP 서버 socket 구현 | [v] | `code/server/src/server.c:371`, `code/server/src/server.c:401`, `code/server/src/server.c:412`, `code/server/src/server.c:434`, `code/server/src/server.c:447` |
| TCP 클라이언트 socket 구현 | [v] | `code/client/client.c:131`, `code/client/client.c:136`, `code/client/client.c:152`, `code/client/client.c:166`, `code/client/client.c:172` |
| shared library 빌드 | [v] | `Makefile:11`, `Makefile:12`, `Makefile:13`, `Makefile:14`, `Makefile:35`, `Makefile:38`, `Makefile:41`, `Makefile:44` |
| 동적 라이브러리 로딩 | [v] | `code/server/src/server.c:56`, `code/server/src/server.c:61`, `code/server/src/server.c:303`, `code/server/src/server.c:306`, `code/server/src/server.c:326`, `code/server/src/server.c:331`, `code/server/src/server.c:337`, `code/server/src/server.c:344`, `code/server/src/server.c:627` |
| 클라이언트 시그널 처리 | [v] | `code/client/client.c:19`, `code/client/client.c:25`, `code/client/client.c:30`, `code/client/client.c:37`, `code/client/client.c:39`, `code/client/client.c:41` |
| 빌드 자동화 | [v] | `Makefile:21`, `Makefile:23`, `Makefile:25`, `Makefile:27`, `Makefile:47`, `Makefile:50`, `Makefile:53` |

### 사용자 편의성, 문서, 추가 기능

| 평가 요소 | 구현 여부 | 코드/문서 위치 |
| --- | --- | --- |
| 메뉴 기반 클라이언트 | [v] | `code/client/client.c:44`, `code/client/client.c:61`, `code/client/client.c:322` |
| 실행 방법 및 평가 기준 문서화 | [v] | `docs/README.md`, `docs/manual.md` |
| C 기반 웹 상태 서버 | [v] | `code/server/src/web_status.c:149`, `code/server/src/web_status.c:225`, `code/server/src/web_status.c:252`, `code/server/src/web_status.c:261`, `code/server/src/web_status.c:270` |
| 파일 기반 상태 공유 | [v] | `code/server/src/server.c:71`, `code/server/src/server.c:159`, `code/server/src/web_status.c:11`, `code/server/src/web_status.c:29` |
| 웹 화면 자동 새로고침 및 접속 상태 표시 | [v] | `code/server/src/web_status.c:160`, `code/server/src/web_status.c:171`, `code/server/src/web_status.c:177`, `code/server/src/web_status.c:184` |

## 4. 사용한 기능 설명

### TCP Socket

서버는 TCP 5100 포트에서 클라이언트 연결을 기다립니다. 클라이언트는 문자열 명령을 서버로 전송하고, 서버는 명령에 따라 장치 제어 함수를 호출한 뒤 결과를 응답합니다.

### Daemon Process

서버는 `-d` 옵션으로 실행하면 `fork()`와 `setsid()`를 사용하여 데몬 프로세스로 동작합니다.

### Dynamic Library

LED, BUZZER, LIGHT, FND 제어 코드는 각각 `.so` 파일로 빌드됩니다. 서버는 실행 중 `dlopen()`, `dlsym()`, `dlclose()`를 사용하여 장치 라이브러리를 동적으로 로드합니다.

### Thread

서버는 `LIGHT SENSOR ON` 상태에서 센서 값을 주기적으로 읽고 LED를 자동 제어하기 위해 `pthread_create()`를 사용합니다. BUZZER 라이브러리도 멜로디 재생을 별도 thread에서 처리합니다.

### Multi Process

클라이언트는 FND countdown 기능에서 `fork()`를 사용하여 countdown 처리를 별도 프로세스로 실행합니다.

### File Based Status Sharing

장치 서버는 현재 상태를 `exec/status.txt`에 기록합니다. 웹 상태 서버는 같은 파일을 읽어 브라우저에 HTML로 표시합니다.

### C Web Server

`web_status`는 C socket API로 HTTP 요청을 받고 HTML 응답을 직접 생성합니다. 브라우저에서는 `http://<server_ip>:8080`으로 접속하여 장치 상태를 확인합니다.
