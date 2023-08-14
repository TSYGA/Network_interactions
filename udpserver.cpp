#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

#include <iostream>
#include <stdio.h> 
#include <string.h>
#include <stdlib.h>

// Поиск клиента, в случии отсутствия добавление
int client_search(unsigned int ip, unsigned int port);

// Запись полученного сообщения в файл
int recv_string(int j, char* buf, unsigned int ip, unsigned int port);

struct client* clients_mas;// Массив клиентов
int len_clients_mas = 0;// Длинна массива len_clients_mas

// Структура клиента
struct client {
	// Идентификаторы клиента
	unsigned int ip;
	unsigned int port;

	/* Массив номеров сообщений полученных 
	от этого клиента*/ 
	int* messages_numbers;

	int len;// Длинна массива messages_numbers
};

// Превод числа из 4 байтного формата
union translation
{
	unsigned long int Long;
	unsigned int Int;
	char data[4];
};
bool stop = false;


int init()
{
	// Для Windows следует вызвать WSAStartup перед началом использования сокетов
	WSADATA wsa_data;
	return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
}

int sock_err(const char* function, int s)
{
	int err;
	err = WSAGetLastError();
	fprintf(stderr, "%s: socket error: %d\n", function, err);
	return -1;
}

int set_non_block_mode(int s)
{
	unsigned long mode = 1;
	return ioctlsocket(s, FIONBIO, &mode);
}

int main(int argc, char** argv)
{
	// Проверка количества аргументов командной строки
	if (argc != 3)
		return 0;

	// Получение границ диапазона портов
	int port1 = atoi(argv[1]);
	int port2 = atoi(argv[2]);

	int* sockets = (int*)calloc(port2 - port1+1, sizeof(int));// Массив сокетов
	int number_ports = port2 - port1 + 1;// Количество портов

	// Инициалиазация сетевой библиотеки
	init();
	
	struct sockaddr_in addr;
	// Создание UDP - сокетов для каждого порта из диапазона
	for (int i = 0; i < number_ports;i++) {
		// Создание UDP-сокета
		sockets[i] = socket(AF_INET, SOCK_DGRAM, 0);

		if (sockets[i] < 0)
			return sock_err("socket", sockets[i]);
		
		// Перевод сокета в неблокирующий режим
		set_non_block_mode(sockets[i]);

		// Заполнение структуры с адресом прослушивания узла
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port1+i);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);

		// Связь адреса и сокета, чтобы он мог принимать входящие дейтаграммы
		if (bind(sockets[i], (struct sockaddr*)&addr, sizeof(addr)) < 0)
			return sock_err("bind", sockets[i]);
	}

	WSAEVENT events = WSACreateEvent();// Событие

	for (int i = 0; i < number_ports; i++)//Сопоставили прослушивающим сокетам событие
		WSAEventSelect(sockets[i], events, FD_READ | FD_WRITE | FD_CLOSE);
	
	unsigned int ip;
	unsigned int port;
	char* message = (char*)calloc(100000, sizeof(char*));// Полученная дейтограмма

	int addrlen = sizeof(addr);
	int message_number, client_index;

	while (1)
	{
		WSANETWORKEVENTS ne;
		// Ожидание событий в течение секунды
		DWORD dw = WSAWaitForMultipleEvents(1,   &events, FALSE, 1, FALSE);
		WSAResetEvent(events);

		// Перебор массива сокетов
		for (int i = 0; i < number_ports; i++)
		{
			if (0 == WSAEnumNetworkEvents(sockets[i], events, &ne) && ne.lNetworkEvents & FD_READ)
			{
				// По сокету cs[i] есть события
				// Получаем дейтограмму
				recvfrom(sockets[i], message, 100000, 0, (struct sockaddr*)&addr, &addrlen);

				ip = ntohl(addr.sin_addr.s_addr);
				port = addr.sin_port;
				
				// Поиск клиента
				client_index = client_search(ip, port);

				// Запись полученного сообщения в файл
				message_number = recv_string(client_index, message, ip, port);

				printf("%u.%u.%u.%u:%u %d\n", (ip >> 24) & 0xFF, 
					(ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip) & 0xFF, port, ntohl(message_number));
				
				// Отправляем подтверждение получения сообщения
				if (sendto(sockets[i], (const char*)&message_number, 4, 0, (struct sockaddr*)&addr, addrlen) < 0)
					return sock_err("Send", sockets[i]);
				
				if (stop) {// Получено сообщение Stop, закрытие сокетов
					for (int j = 0; j < port2 - port1 + 1; j++)
						closesocket(sockets[j]);

					WSACleanup();
					return 0;
				}
			}
		}
	}
	return 0;
}

// Поиск клиента, в случии отсутствия добавление
int client_search(unsigned int ip, unsigned int port){
	// Поиск клиента
	for (int j = 0; j < len_clients_mas; j++) {
		if (clients_mas[j].ip == ip && clients_mas[j].port == port){
			return j;
		}
	}

	// Клиент не найден
	// Добавление клиента
	clients_mas = (struct client*)realloc(clients_mas, ++len_clients_mas * sizeof(struct client));
	struct client new_client;
	new_client.ip = ip;
	new_client.port = port;
	new_client.messages_numbers = NULL;
	new_client.len = 0;
	clients_mas[len_clients_mas - 1] = new_client;

	return len_clients_mas - 1;
}

// Запись полученного сообщения в файл
int recv_string(int client_index, char* message, unsigned int ip, unsigned int port)
{
	// Открытие файла
	FILE* f = fopen("msg.txt", "a");

	translation translation_long_number;

	// Чтение номера сообщения
	unsigned int number_messag;
	memcpy(&number_messag, message, 4);
	
	// Проверка было ли получено сообщение с таким номером
	for (int i = 0; i < clients_mas[client_index].len; i++) {
		if (clients_mas[client_index].messages_numbers[i] == ntohl(number_messag)) {
			// Сообщение было получено раньше, возращаем номер сообщения
			return number_messag;
		}
	}

	// Добавляем номер сообщения в масиив номеров
	clients_mas[client_index].messages_numbers = (int*)realloc(clients_mas[client_index].messages_numbers, 
		++clients_mas[client_index].len * sizeof(int));
	clients_mas[client_index].messages_numbers[clients_mas[client_index].len - 1] = ntohl(number_messag);

	// Запись идентификатора пользователя в файл
	fprintf(f, "%u.%u.%u.%u:%u ", (ip >> 24) & 0xFF, 
		(ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip) & 0xFF, port);

	// Запись времени в файл
	fprintf(f, "%d%d:", (int)message[4] / 10, (int)message[4] % 10);
	fprintf(f, "%d%d:", (int)message[5] / 10, (int)message[5] % 10);
	fprintf(f, "%d%d ", (int)message[6] / 10, (int)message[6] % 10);

	fprintf(f, "%d%d:", (int)message[7] / 10, (int)message[7] % 10);
	fprintf(f, "%d%d:", (int)message[8] / 10, (int)message[8] % 10);
	fprintf(f, "%d%d ", (int)message[9] / 10, (int)message[9] % 10);

	// Запись числа в файл
	memcpy(&translation_long_number.data, message + 10, 4);
	fprintf(f, "%u ", ntohl(translation_long_number.Long));

	// Запись текста в файл
	char stop_message[5];
	memcpy(&stop_message, message + 14, 5);

	for (int i = 14; message[i] != '\0'; i ++)
		fprintf(f, "%c", message[i]);
	
	fprintf(f, "\n");

	// Проверка сообщения на завершение работы сервера
	if (strcmp(stop_message, "stop") == 0)
		stop = true;

	// Закрываем файл, возвращаем номер сообщения
	fclose(f);
	return number_messag;
}