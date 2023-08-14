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

#include <iostream>
#include <stdio.h> 
#include <string.h>
#include <stdlib.h>

// Чтение файла и формирование дейтограммы
int read(char* name);

// Функция принимает дейтаграмму от удаленной стороны.
// Возвращает 0, если в течение 100 миллисекунд не было получено ни одной дейтаграммы
unsigned int recv_response(int s);

// Структура массива дейтограмм
struct out
{
	bool flag;// Получил ли сервер дейтограмму
	int len;// Длинна массива output
	char* output;// Дейтограмма
};
// Массив дейтограмм
struct out* out;

// Превод числа в 4 байтны формат
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

int main(int argc, char** argv)
{
	// Проверка количества аргументов командной строки
	if (argc != 3)
		return 0;
	
	// Отделение ip и port из пораметров командной строки ip:port 
	char* ip = (char*)calloc(16, sizeof(char));
	char* port = (char*)calloc(16, sizeof(char));
	ip = strtok((char*)argv[1], ":");
	port = strtok(NULL, ":");
	
	// Создание UDP-сокета
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return sock_err("socket", s);
	
	// Заполнение структуры с адресом удаленного узла
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port));
	addr.sin_addr.s_addr = inet_addr(ip);

	// Чтение файла и формирование дейтограммы
	int len_out = read(argv[2]);
	if (len_out==-1)
		return 0;
	
	int k; // Количество отправленных сообщений на текущей итерации цикла
	/* Бесконечно повторяем отправку тех 
	дейтограмм о которых не получен ответ*/
	while (true)
	{
		k = 0;
		
		// Отправляем все сообщения из массива
		for (int i = 0; i < len_out; i++)
		{
			if (!out[i].flag) {// Проверка было ли дейтограмм получено сервером
				sendto(s, out[i].output, out[i].len, MSG_NOSIGNAL, 
					(struct sockaddr*)&addr, sizeof(struct sockaddr_in));

				k++;
			}
		}

		if (k == 0)// Все сообщения отправлены
			break;
		
		if (recv_response(s) < 0)// Ответ сервера
			return -1;
	}

	// Закрытие сокета
	close(s);
	return 0;
}


// Чтение файла и формирование дейтограммы
int read(char* name) {
	// Открытие файла с текстом сообщений на чтение
	FILE* f = fopen(name, "r");
	if (!f)
		return -1;

	char line[19] = { 0 };
	char* output = (char*)calloc(14, sizeof(char));
	char* str_time1 = (char*)calloc(8, sizeof(char));
	char* str_time2 = (char*)calloc(8, sizeof(char));
	
	union translation translation_message_number, translation_long_number;
	
	int message_number = 0, res, len_out = 0;
	unsigned long int long_number;
	char symbol;

	while (fgets(line, 19, f) != NULL)
	{
		if (line[0] == '\n')// Пустая строка, нечего не отправляем 
			continue;
		
		// Отделение строки со временем
		str_time1 = strtok(line, " ");
		str_time2 = strtok(NULL, " ");

		// Чтение числа
		long_number = 0;
		for (int i = 0; i < 512 && (symbol = fgetc(f)) != ' '; i++)
			long_number = long_number * 10 + symbol - '0';
		translation_long_number.Long = htonl(long_number);

		// Добавление номера сообщения в дейтограмму
		int j = 0;
		translation_message_number.Int = htonl(message_number);
		for (j; j < 4; j++)
			output[j] = translation_message_number.data[j];
		
		/* Отделение часов, минут и секунд из строки
		 со временем и добавление их в дейтограмму*/
		output[j++] = atoi(strtok(str_time1, ":"));
		output[j++] = atoi(strtok(NULL, ":"));
		output[j++] = atoi(strtok(NULL, ":"));

		output[j++] = atoi(strtok(str_time2, ":"));
		output[j++] = atoi(strtok(NULL, ":"));
		output[j++] = atoi(strtok(NULL, ":"));

		// Добавление числа в дейтограмму
		for (int i=0; i < 4; i++)
			output[j + i] = translation_long_number.data[i];
		j += 4;

		// Добавление сообщения в дейтограмму 
		while ((symbol = fgetc(f)) != EOF && symbol != '\n' && symbol != '\0') {
			output = (char*)realloc(output, (j+1) * sizeof(char));
			output [j++] = symbol;
		}
		output = (char*)realloc(output, j + 1 * sizeof(char));
		output[j] = '\0';
		out = (struct out*)realloc(out, (++len_out) * sizeof(struct out));

		// Добавление дейтограммы в массив дейтограмм
		struct out Out;
		Out.flag = false;
		Out.output = output;
		Out.len = ++j;
		out[message_number] = Out;
		
		message_number++;
		output = (char*)calloc(14, sizeof(char));
	}

	// Закрытие файла
	fclose(f);
	return len_out++;;
}

// Функция принимает дейтаграмму от удаленной стороны.
// Возвращает 0, если в течение 100 миллисекунд не было получено ни одной дейтаграммы
unsigned int recv_response(int s)
{
	char datagram[1024];
	struct timeval tv = { 0, 100 * 1000 }; // 100 msec
	
	fd_set fds;
	FD_ZERO(&fds); 
	FD_SET(s, &fds);
	// Проверка - если в сокете входящие дейтаграммы
	// (ожидание в течение tv)
	int res = select(s + 1, &fds, 0, 0, &tv);
	int message_number = -1;
	while (res > 0)
	{
		// Данные есть, считывание их
		struct sockaddr_in addr;
		int addrlen = sizeof(addr);
		int received = recvfrom(s, datagram, sizeof(datagram), 0, 
			(struct sockaddr*)&addr, (socklen_t*)&addrlen);

		if (received <= 0)// Ошибка считывания полученной дейтаграммы
		{
			sock_err("recvfrom", s);
			return 0;
		}

		// Помечаем дейтограмму как полученную
		memcpy(&message_number, datagram, 4);
		message_number = ntohl(message_number);

		if (!out[message_number].flag)
			out[message_number].flag = true;
		res = select(s + 1, &fds, 0, 0, &tv);
	}

	if (res == 0)// Данных в сокете нет, возврат ошибки
	{
		return 0;
	}
	else
	{
		return sock_err("select", s);
	}
}


