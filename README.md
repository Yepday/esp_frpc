esp_frpc is an intranet penetration client implemented in C language, running on ESP8266 based on the FreeRTOS operating system. It supports TCP connections, with a compiled binary size of approximately 500 KBytes.

Usage Instructions:
1. cd ~/ESP8266_RTOS_SDK/
2. git clone https://github.com/Yepday/esp_frpc.git
3. cd esp_frpc
4. Run make menuconfig:
	Enable options under Component config ---> mbedTLS-related settings
	Configure the correct WiFi SSID and password under Example Connection Configuration
5. Open login.h and configure FRPS parameters:

	#define SERVER_ADDR        "xx.xx.xx.xx"
	#define SERVER_PORT        7000
	#define AUTH_TOKEN         "52010"
	#define HEARTBEAT_INTERVAL 30
	#define HEARTBEAT_TIMEOUT 90

	#define PROXY_NAME         "ssh-ubuntu"
	#define PROXY_TYPE         "tcp"
	#define LOCAL_IP          "127.0.0.1"
	#define LOCAL_PORT         22
	#define REMOTE_PORT        7005

Note: This program does not use the LOCAL_IP and LOCAL_PORT parameters. They can be set arbitrarily here only to maintain consistency with FRPS configurations.
6. make
7. make flash

