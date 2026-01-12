#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <mysql/mysql.h>
#include <stdbool.h>
#include <math.h>


#define BUF_SIZE 512
#define NAME_SIZE 512
#define ARR_CNT 64
#define DEVICE_NAME "KCCI"

void *send_msg(void *arg);
void *recv_msg(void *arg);
void error_handling(char *msg);

char name[NAME_SIZE] = "[Default]";
char msg[BUF_SIZE];

int main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in serv_addr;
	pthread_t snd_thread, rcv_thread;
	void *thread_return;

	if (argc != 4)
	{
		printf("Usage : %s <IP> <port> <name>\n", argv[0]);
		exit(1);
	}

	sprintf(name, "%s", argv[3]);

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1)
		error_handling("socket() error");

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(atoi(argv[2]));

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
		error_handling("connect() error");

	sprintf(msg, "[%s:PASSWD]", name);
	write(sock, msg, strlen(msg));
	pthread_create(&rcv_thread, NULL, recv_msg, (void *)&sock);
	pthread_create(&snd_thread, NULL, send_msg, (void *)&sock);

	pthread_join(snd_thread, &thread_return);
	pthread_join(rcv_thread, &thread_return);

	if (sock != -1)
		close(sock);
	return 0;
}

void *send_msg(void *arg)
{
	int *sock = (int *)arg;
	int str_len;
	int ret;
	fd_set initset, newset;
	struct timeval tv;
	char name_msg[NAME_SIZE + BUF_SIZE + 2];

	FD_ZERO(&initset);
	FD_SET(STDIN_FILENO, &initset);

	fputs("Input a message! [ID]msg (Default ID:ALLMSG)\n", stdout);
	while (1)
	{
		memset(msg, 0, sizeof(msg));
		name_msg[0] = '\0';
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		newset = initset;
		ret = select(STDIN_FILENO + 1, &newset, NULL, NULL, &tv);
		if (FD_ISSET(STDIN_FILENO, &newset))
		{
			fgets(msg, BUF_SIZE, stdin);
			if (!strncmp(msg, "quit\n", 5))
			{
				*sock = -1;
				return NULL;
			}
			else if (msg[0] != '[')
			{
				strcat(name_msg, "[ALLMSG]");
				strcat(name_msg, msg);
			}
			else
				strcpy(name_msg, msg);
			if (write(*sock, name_msg, strlen(name_msg)) <= 0)
			{
				*sock = -1;
				return NULL;
			}
		}
		if (ret == 0)
		{
			if (*sock == -1)
				return NULL;
		}
	}
}

void *recv_msg(void *arg)
{
	MYSQL *conn;
	MYSQL_ROW sqlrow;
	int res;
	char sql_cmd[512] = {0};
	char *host = "10.10.14.75";
	char *user = "iot";
	char *pass = "pwiot";
	char *dbname = "iotdb";

	int *sock = (int *)arg;
	int i;
	char *pToken;
	char *pArray[ARR_CNT] = {0};

	char name_msg[NAME_SIZE + BUF_SIZE + 1];
	int str_len;

	conn = mysql_init(NULL);

	puts("MYSQL startup");
	if (!(mysql_real_connect(conn, host, user, pass, dbname, 0, NULL, 0)))
	{
		fprintf(stderr, "ERROR : %s[%d]\n", mysql_error(conn), mysql_errno(conn));
		exit(1);
	}
	else
		printf("Connection Successful!\n\n");

	while (1)
	{
		memset(name_msg, 0x0, sizeof(name_msg));
		str_len = read(*sock, name_msg, NAME_SIZE + BUF_SIZE);
		if (str_len <= 0)
		{
			*sock = -1;
			return NULL;
		}
		fputs(name_msg, stdout);
		//		name_msg[str_len-1] = 0;   //'\n' 제거
		name_msg[strcspn(name_msg, "\n")] = '\0';

		pToken = strtok(name_msg, "[:@]");
		i = 0;
		while (pToken != NULL)
		{
			pArray[i] = pToken;
			if (++i >= ARR_CNT)
				break;
			pToken = strtok(NULL, "[:@]");
		}

		if (i >= 6 && !strcmp(pArray[1], "BEACON"))
		{
			char uuid[128] = {0};
			int rssi = 0;

			
			for (int k = 2; k + 1 < i; k += 2)
			{
				if (!strcasecmp(pArray[k], "UUID"))
				{
					strncpy(uuid, pArray[k + 1], sizeof(uuid) - 1);
				}
				else if (!strcasecmp(pArray[k], "RSSI"))
				{
					rssi = atoi(pArray[k + 1]);
				}
			}

			if (strlen(uuid) == 0)
			{
				fprintf(stderr, "[BEACON] UUID missing\n");
				continue;
			}

			
			char uuid_esc[128] = {0};
			mysql_real_escape_string(conn, uuid_esc, uuid, strlen(uuid));

			
			double txPower = -59.0;
			double n = 2.0;
			double distance = pow(10.0, (txPower - (double)rssi) / (10.0 * n));

			
			snprintf(sql_cmd, sizeof(sql_cmd),
					"INSERT INTO beacon_log (ts, reporter, uuid, rssi, distance_m) "
					"VALUES (NOW(), '%s', '%s', %d, %.3f)",
					pArray[0], uuid_esc, rssi, distance);

			if (!mysql_query(conn, sql_cmd))
			{
				printf("[BEACON] inserted uuid=%s rssi=%d dist=%.2f\n",
					uuid, rssi, distance);
			}
			else
			{
				fprintf(stderr, "[BEACON] SQL ERROR: %s\n", mysql_error(conn));
			}

			continue;
		}

		if (i >= 6 && !strcmp(pArray[1], "BEACON_STATE"))
		{
			int rssi = atoi(pArray[3]);
			double dist = atof(pArray[4]);
			char *state = pArray[5];

			
			char uuid_esc[128] = {0};
			mysql_real_escape_string(conn, uuid_esc, pArray[2], strlen(pArray[2]));


			char prev_state[16] = {0};

			snprintf(sql_cmd, sizeof(sql_cmd),
				"SELECT state FROM beacon_state WHERE uuid='%s'",
				uuid_esc);

			if (!mysql_query(conn, sql_cmd))
			{
				MYSQL_RES *res_state = mysql_store_result(conn);
				if (res_state)
				{
					MYSQL_ROW row = mysql_fetch_row(res_state);
					if (row && row[0])
						strncpy(prev_state, row[0], sizeof(prev_state)-1);
					mysql_free_result(res_state);
				}
			}

			snprintf(sql_cmd, sizeof(sql_cmd),
				"INSERT INTO beacon_state "
				"(uuid, last_rssi, last_distance, state, last_seen) "
				"VALUES ('%s', %d, %.2f, '%s', NOW()) "
				"ON DUPLICATE KEY UPDATE "
				"last_rssi=%d, last_distance=%.2f, state='%s', last_seen=NOW()",
				uuid_esc, rssi, dist, state,
				rssi, dist, state);

			mysql_query(conn, sql_cmd);


			if (strlen(prev_state) == 0 || strcmp(prev_state, state) != 0)
			{
				snprintf(sql_cmd, sizeof(sql_cmd),
					"INSERT INTO beacon_state_log "
					"(uuid, prev_state, new_state, ts) "
					"VALUES ('%s', '%s', '%s', NOW())",
					uuid_esc,
					strlen(prev_state) ? prev_state : "INIT",
					state);

				mysql_query(conn, sql_cmd);

				printf("[STATE] %s : %s -> %s\n",
					uuid_esc,
					strlen(prev_state) ? prev_state : "INIT",
					state);
			}

			continue;
		}

		if (!strcmp(pArray[1], "GETBEACON") && i == 3)
		{
			char uuid_esc[128] = {0};
			mysql_real_escape_string(conn, uuid_esc, pArray[2],
									(unsigned long)strlen(pArray[2]));

			snprintf(sql_cmd, sizeof(sql_cmd),
					"SELECT rssi, distance_m, "
					"DATE_FORMAT(ts,'%%Y-%%m-%%d_%%H:%%i:%%s') AS ts_fmt "
					"FROM beacon_log "
					"WHERE uuid='%s' "
					"ORDER BY ts DESC LIMIT 1",
					uuid_esc);

			if (mysql_query(conn, sql_cmd))
			{
				fprintf(stderr, "GETBEACON query err: %s\n", mysql_error(conn));
				snprintf(sql_cmd, sizeof(sql_cmd),
						"[%s]GETBEACON@ERR@QUERY\n", pArray[0]);
				write(*sock, sql_cmd, strlen(sql_cmd));
				continue;
			}

			MYSQL_RES *result = mysql_store_result(conn);
			if (!result)
			{
				fprintf(stderr, "GETBEACON store_result err: %s\n", mysql_error(conn));
				snprintf(sql_cmd, sizeof(sql_cmd),
						"[%s]GETBEACON@ERR@RESULT\n", pArray[0]);
				write(*sock, sql_cmd, strlen(sql_cmd));
				continue;
			}

			MYSQL_ROW row = mysql_fetch_row(result);
			if (!row)
			{
				snprintf(sql_cmd, sizeof(sql_cmd),
						"[%s]GETBEACON@ERR@NoData\n", pArray[0]);
				write(*sock, sql_cmd, strlen(sql_cmd));
				mysql_free_result(result);
				continue;
			}

			snprintf(sql_cmd, sizeof(sql_cmd),
					"[%s]GETBEACON@%s@%s@%s@%s\n",
					pArray[0],          // ID 
					pArray[2],          // UUID
					row[0],             // RSSI
					row[1],             // distance_m
					row[2]);            // timestamp

			write(*sock, sql_cmd, strlen(sql_cmd));
			mysql_free_result(result);
		}
		else if (!strcmp(pArray[1], "SETDB"))
		{
		 	sprintf(sql_cmd, "update device set value='%s' where name='%s'", pArray[3], pArray[2]);

		 	res = mysql_query(conn, sql_cmd);
		 	if (!res)
		 	{
		 		if (i == 4)
		 			sprintf(sql_cmd, "[%s]%s@%s@%s\n", pArray[0], pArray[1], pArray[2], pArray[3]);
		 		else if (i == 5)
		 			sprintf(sql_cmd, "[%s]%s@%s\n", pArray[4], pArray[2], pArray[3]);
				else
		 			continue;

		 		printf("inserted %lu rows\n", (unsigned long)mysql_affected_rows(conn));
		 		write(*sock, sql_cmd, strlen(sql_cmd));
		 	}
		 	else
		 		fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
		 }


		else if ((!strcmp(pArray[1], "PLUG1") || !strcmp(pArray[1], "PLUG2")) && i == 3)
		{
    		
    		sprintf(sql_cmd, "update device set value='%s', date=now(), time=now() where name='%s'", pArray[2],  pArray[1]);
   		 res = mysql_query(conn, sql_cmd);
    		if (!res)
			{
    		    printf("Updated %s to %s in MySQL\n",  pArray[1], pArray[2]);
   			}
			else
			{
    		    fprintf(stderr, "SQL ERROR: %s [%d]\n", mysql_error(conn), mysql_errno(conn));
			}

			
			 sprintf(sql_cmd, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
			 write(*sock, sql_cmd, strlen(sql_cmd));
		}
	}
	mysql_close(conn);
}

void error_handling(char *msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
