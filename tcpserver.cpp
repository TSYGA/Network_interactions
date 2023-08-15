#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>

#include <iostream>
#include <stdio.h> 
#include <string.h>
#include <stdlib.h>

// Получение дейтограммы, запись в файл
int recv_string(int cs, unsigned int ip, const char* port);

// Превод числа из 4 байтного формата
union translation
{
	unsigned long int Long;
	unsigned int Int;
	char data[4];
};

int sock_err(const char* function, int s)
{
	int err;
	err = errno;
	fprintf(stderr, "%s: socket error: %d\n", function, err);
	return -1;
}

// Перевод сокета в неблокирующий режим
int set_non_block_mode(int s)
{
	int fl = fcntl(s, F_GETFL, 0);
	return fcntl(s, F_SETFL, fl | O_NONBLOCK);
}

int main(int argc, char** argv)
{
	// Проверка количества аргументов командной строки
	if (argc != 2)
		return 0;

	// Создание TCP-сокета
	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return sock_err("socket", s);

	// Перевод сокета в неблокирующий режим
	set_non_block_mode(s);

	// Заполнение структуры с адресом удаленного узла
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[1]));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Связь адреса и сокета
	if (bind(s, (const struct sockaddr*)&addr, sizeof(addr)) < 0)
		return sock_err("bind", s);

	// Начало прослушивания
	if (listen(s, 1) < 0)
		return sock_err("listen", s);

	printf("Listening port: %d\n", atoi(argv[1]));

	int* cs = (int*)calloc(1, sizeof(int)); // Сокеты с подключенными клиентами
	int cs_len = 0;

	fd_set rfd;
	fd_set wfd;
	int nfds = s, new_soket, res;

	struct timeval tv = { 1, 0 };
	socklen_t addrlen = sizeof(addr);
	unsigned int ip;
	char put[4];

	while (1)
	{
		FD_ZERO(&rfd);
		FD_ZERO(&wfd);
		FD_SET(s, &rfd);
		
		for (int i = 0; i < cs_len; i++)
		{
			FD_SET(cs[i], &rfd);
			FD_SET(cs[i], &wfd);
			if (nfds < cs[i])
				nfds = cs[i];
		}
		
		if (select(nfds + 1, &rfd, &wfd, 0, &tv) > 0)
		{
			// Есть события
			if (FD_ISSET(s, &rfd))
			{
				// Подключение сокита, перевод сокета в неблокирующий режим
				new_soket = accept(s, (struct sockaddr*)&addr, &addrlen);
				set_non_block_mode(new_soket);

				if (new_soket < 0)
				{
					sock_err("accept", s);
					break;
				}

				ip = ntohl(addr.sin_addr.s_addr);
				printf("Client connected: %u.%u.%u.%u \n", (ip >> 24) & 0xFF,
					 (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip) & 0xFF);
				

				// Ожидание сообщения о начале предачи сообщений
				put[3] = '\0';
				while(recv(new_soket, put, 3, MSG_NOSIGNAL)<=0);

				if (strcmp(put, "put") == 0) {
					printf("\'put\' message has been recieved.\n");

					// добавление сокета в массив сокетов с подключенными клиентами
					cs = (int*)realloc(cs, (cs_len+1) * sizeof(int));
					cs[cs_len++] = new_soket;
				}
				else {// Закрытие сокета
					close(new_soket);
				}
			}
			
			// Перебор массива подключеных сокетов
			for (int i = 0; i < cs_len; i++)
			{
				if (FD_ISSET(cs[i], &rfd))// Сокет cs[i] доступен для чтения. 
				{
					// Получение дейтограммы, запись в файл
					ip = ntohl(addr.sin_addr.s_addr);
					res = recv_string(cs[i], ip, argv[1]);

					if (res == 1) {// Получено сообщение Stop, закрытие сокетов
						printf("Stop.\n");

						for (int j = 0; j < cs_len; j++)
							close(cs[j]);
						
						close(s);

						return 0;
					}
				}
			}
		}
	}

	// Закрытие сокета
	close(s);
	return 0;
}

// Получение дейтограммы, запись в файл
int recv_string(int cs, unsigned int ip, const char* port)
{
	// Открытие файла для записи полученых сообщений
	FILE* f = fopen("msg.txt", "a");

	char line[4];
	char buffer[5];
	translation translation_long_number;
	char symbol;

	while (recv(cs, line, 4, MSG_NOSIGNAL)>0) {
		// Запись ip клиента в файл
		fprintf(f, "%u.%u.%u.%u:%s ", (ip >> 24) & 0xFF, 
			(ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip) & 0xFF, port);

		// Запись времени в файл
		recv(cs, &symbol, 1, MSG_NOSIGNAL);
		fprintf(f, "%d%d:", (int)symbol/10, (int)symbol %10);
		recv(cs, &symbol, 1, MSG_NOSIGNAL);
		fprintf(f, "%d%d:", (int)symbol / 10, (int)symbol % 10);
		recv(cs, &symbol, 1, MSG_NOSIGNAL);
		fprintf(f, "%d%d ", (int)symbol / 10, (int)symbol % 10);

		recv(cs, &symbol, 1, MSG_NOSIGNAL);
		fprintf(f, "%d%d:", (int)symbol / 10, (int)symbol % 10);
		recv(cs, &symbol, 1, MSG_NOSIGNAL);
		fprintf(f, "%d%d:", (int)symbol / 10, (int)symbol % 10);
		recv(cs, &symbol, 1, MSG_NOSIGNAL);
		fprintf(f, "%d%d ", (int)symbol / 10, (int)symbol % 10);

		// Запись числа в файл
		for (int i = 0; i < 4; i++)
			recv(cs, &translation_long_number.data[i], 1, MSG_NOSIGNAL);
		fprintf(f, "%u ", ntohl(translation_long_number.Long));

		// запись текста в файл
		buffer[4] = '\0';
		recv(cs, &symbol, 1, MSG_NOSIGNAL);
		for (int i = 0; symbol != '\0'; i = ++i % 5) {
			buffer[i] = symbol;
			fprintf(f, "%c", symbol);
			recv(cs, &symbol, 1, MSG_NOSIGNAL);
		}
		fprintf(f, "\n");

		// Подтверждение получения сообщения, отправляем ok
		send(cs, "ok", 2, MSG_NOSIGNAL);

		// Проверка сообщения на завершение работы сервера
		if (strcmp(buffer, "stop") == 0)
			return 1;
	}

	// Закрытие файла
	fclose(f);
	return 0;
}
