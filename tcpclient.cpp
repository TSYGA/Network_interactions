#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>  
#include <winsock2.h>  
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <iostream>
#include <stdio.h> 
#include <string.h>
#include <stdlib.h>

int sock_err(const char* function, int s) {
	int err;
	err = WSAGetLastError();
	fprintf(stderr, "%s: socket error: %d\n", function, err);
	return -1;
}

int init() {
	WSADATA wsa_data;
	return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
}

// Превод числа в 4 байтны формат
union translation
{
	unsigned long int Long;
	unsigned int Int;
	char data[4];
};

int main(int argc, char** argv) {
	// Проверка количества аргументов командной строки
	if (argc != 3)
		return 0;
		
	// Отделение ip и port из пораметров командной строки ip:port 
	char* ip = (char*)calloc(16, sizeof(char));
	char* port = (char*)calloc(16, sizeof(char));
	ip = strtok((char*)argv[1], ":");
	port = strtok(NULL, ":");
	
	// Открытие файла с текстом сообщения на чтение
	FILE* file = fopen(argv[2], "r");
	if (!file)
		return 0;
	
	// Инициалиазация сетевой библиотеки  
	init();
	
	// Создание TCP-сокета
	int s = socket(AF_INET, SOCK_STREAM, 0);
	
	if (s < 0)// Ошибка создания сокета
		return sock_err("socket", s);
	
	// Заполнение структуры с адресом удаленного узла
	struct sockaddr_in addr;  
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port));
	addr.sin_addr.s_addr = inet_addr(ip);
	
	// Установка соединения с удаленным хостом
	int i = 0;
	for (i; i < 10 && connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0; i++)
		Sleep(100);
	
	if (i == 10) {// После 10 попыток соединения завершение с ошибкой соединения
		closesocket(s);
		return sock_err("connect", s);
	}
	
	send(s, (char*)"put", 3, 0); // Отправка запроса на удаленный сервер
	
	char line[19] = { 0 };
	char time1[3] = { 0 };
	char time2[3] = { 0 };
	
	char* str_time1 = (char*)calloc(8, sizeof(char));
	char* str_time2 = (char*)calloc(8, sizeof(char));
	
	union translation translation_message_number, translation_long_number;
	
	int message_number = 0, res;
	unsigned long int long_number;
	char buffer[2];
	char a[1] = { 0 };
	
	while (fgets(line, 19, file) != NULL)
	{
		if (line[0] == '\n')// Пустая строка, нечего не отправляем 
			continue;
		
		// Отделение строки со временем
		str_time1 = strtok(line, " ");
		str_time2 = strtok(NULL, " ");
		
		// Отделение часов, минут и секунд из строки со временем
		time1[0] = atoi(strtok(str_time1, ":"));
		time1[1] = atoi(strtok(NULL, ":"));
		time1[2] = atoi(strtok(NULL, ":"));
		
		time2[0] = atoi(strtok(str_time2, ":"));
		time2[1] = atoi(strtok(NULL, ":"));
		time2[2] = atoi(strtok(NULL, ":"));
		
		// Чтение числа
		long_number = 0;
		for (int i = 0; i < 512 && (a[0] = fgetc(file)) != ' '; i++)
			long_number = long_number * 10 + a[0] - '0';
		translation_long_number.Long = htonl(long_number);
		
		// Отправка номера сообщения
		translation_message_number.Int = htonl(message_number);
		send(s, translation_message_number.data, 4, 0);
		
		// Отправка времени
		send(s, time1, 3, 0);
		send(s, time2, 3, 0);
		
		// Отправка числа
		send(s, translation_long_number.data, 4, 0);
		
		// Отправка текста
		while ((a[0] = fgetc(file)) != EOF && a[0] != '\n' && a[0] != '\0')
			send(s, (char*)a, 1, 0);
		send(s, (char*)"\0", 1, 0);
		
		// Проверка ответа от сервера
		if ((res = recv(s, buffer, 2, 0)) > 0)
			printf("ok  %d bytes received\n", res);
		else if (res < 0)
			return sock_err("recv", s);
		
		message_number++;
	}
	
	// Закрытие файла
	fclose(file);
	// Закрытие соединения  
	closesocket(s);
	WSACleanup();
	
	return 0;
}
