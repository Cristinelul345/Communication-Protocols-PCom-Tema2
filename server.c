#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#define TOPIC_SIZE 50
#define TOPIC_SIZE_STRUC 51
#define TCP_SUBSCRIBE 1
#define TCP_UNSUBSCRIBE 0
#define TRUE 1
#define ID_SIZ 10
#define MAX_CLIENTS	50	
#define BUFLEN		1000
#define CONTENT_SIZE 1500


typedef struct {
	char topic[TOPIC_SIZE];
	unsigned char data_type;
	char content[CONTENT_SIZE];
	struct in_addr ip;
	unsigned short port;
} UDP_MSG_STRUCT;

typedef struct {
	char topic[TOPIC_SIZE_STRUC];
	unsigned int type;
	unsigned int SF;
} TCP_MSG_STRUCT;

typedef struct {
	unsigned int sockfd;
	unsigned int SF;
	char id[ID_SIZ];
} CLIENT_STRUCT;

typedef struct list {
	struct list* next;
	CLIENT_STRUCT info;
} CELL_STRUCT, *LIST_STRUCT;

typedef struct list_udp {
	struct list_udp* next;
	UDP_MSG_STRUCT info;
} UDP_CELL_STRUCT, *UDP_LIST_STRUCT;

typedef struct {
	UDP_LIST_STRUCT msg;
	char id[ID_SIZ];
} STORAGE_STRUCT;

typedef struct {
	LIST_STRUCT clients;
	char topic[TOPIC_SIZE_STRUC];
} TOPIC_STRUCT;

typedef struct {
	TOPIC_STRUCT* array;
	int crt_len;
	int capacity;
} TABLE_STRUCT;

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

/*Create backup for the client */
STORAGE_STRUCT init_backup(CLIENT_STRUCT cli) {
	STORAGE_STRUCT storage;
	storage.msg = NULL;
	memmove(storage.id, cli.id, ID_SIZ);
	return storage;
}

/*Adds to the begginning of the list */
LIST_STRUCT add_to_start_cli(CLIENT_STRUCT elem, LIST_STRUCT list)
{
	int cell_siz = sizeof(CELL_STRUCT);
	LIST_STRUCT aux_elem = calloc(1, cell_siz);
	if(aux_elem == NULL) {
		exit(EXIT_FAILURE);
	}
	aux_elem->next = list;
	aux_elem->info = elem;
	return aux_elem;
}

/*Adds to the begginning of the list */
UDP_LIST_STRUCT add_to_start_UDP(UDP_MSG_STRUCT elem, UDP_LIST_STRUCT list)
{
	int cell_siz = sizeof(UDP_CELL_STRUCT);
	UDP_LIST_STRUCT temp = calloc(1, cell_siz);
	temp->next = list;
	temp->info = elem;
	if(temp == NULL) {
		exit(EXIT_FAILURE);
	}

	return temp;
}

/*Creates table of topics */
TABLE_STRUCT* create_table() {
	int start_len = 0;
	int start_cap = 1;
	int table_siz = sizeof(TABLE_STRUCT);
	int topic_siz = sizeof(TOPIC_STRUCT);
	TABLE_STRUCT* aux_table = calloc(1, table_siz);
	aux_table->capacity = start_cap;
	aux_table->crt_len = start_len;
	aux_table->array = calloc(1, topic_siz);

	return aux_table;
}

/* Adds topic to the table of topics */
void push_topic(TABLE_STRUCT* map, TOPIC_STRUCT top) {
	int index = map->crt_len;
	int topic_siz = sizeof(TOPIC_STRUCT);
	int cap = map->capacity;

	map->crt_len++;
	*(map->array + index) = top;
	if(cap == index + 1) {
		TOPIC_STRUCT* aux = realloc(map->array, 2 * cap * topic_siz);
		if (!aux) exit(EXIT_FAILURE);

		map->array = aux;
		map->capacity *= 2;
	}
}

/* Creates a topic from string */
TOPIC_STRUCT create_topic(char* topic) {
	int topic_siz = strlen(topic) + 1;
	TOPIC_STRUCT top;
	top.clients = NULL;
	memmove(top.topic, topic, topic_siz);
	return top;
}

/* Adds an entry to the backup table */
void add_to_storage_table(int* ok, STORAGE_STRUCT** storage, int* storage_siz, UDP_MSG_STRUCT* udp_message, LIST_STRUCT start) {
	int backup_count_var = *storage_siz;
	STORAGE_STRUCT* backup_var = *storage;
	int k = 0;
	while(k < backup_count_var) {
		UDP_LIST_STRUCT udpList = add_to_start_UDP(*udp_message, (*storage)[k].msg);
		if(strcmp((*storage)[k].id, start->info.id)) {
			++k; continue;
		}

		backup_var[k].msg = udpList;
		*ok = 1;
		k = backup_count_var;
	}
}

/* Creates a backup table. This table will be used by
clients who exited with SF = 1 */
void create_storage(int* ok, STORAGE_STRUCT** storage, int dup, int siz, int* storage_siz,
UDP_MSG_STRUCT* udp_msg, int* backup_capacity, LIST_STRUCT p) {
	int found_var = *ok;
	if(found_var) return;
	STORAGE_STRUCT* backup_var = *storage;
	int backup_count_var = *storage_siz;
	int backup_capacity_var = *backup_capacity;
	UDP_LIST_STRUCT msgList = add_to_start_UDP(*udp_msg, backup_var[backup_count_var].msg);
	
	*(backup_var + backup_count_var) = init_backup(p->info);	
	(*storage_siz)++;
	(backup_var + backup_count_var)->msg = msgList;
	backup_count_var++;
	
	if(backup_capacity_var == backup_count_var) {
		*backup_capacity *= dup;
		backup_capacity_var *= dup;
		STORAGE_STRUCT* aux = realloc(backup_var, backup_capacity_var * siz);
		if(aux) *storage = aux;
	}
}

/* Procedure used when the server rceives the exit command. It sends the
exit to all the clients */
int recv_exit(int descriptor_max, fd_set* read_fds, int udp_sockfd, int tcp_sockfd) {
	char buffer[BUFLEN];
	int index = 1;
	scanf("%s", buffer);
	if (strcmp(buffer, "exit") != 0) return -1;

	while(index <= descriptor_max) {
		if (index == udp_sockfd) {++index; continue;}
		if (index == tcp_sockfd) {++index; continue;}
		int ret;
		if(FD_ISSET(index, read_fds)) {
			DIE((ret = send(index, buffer, BUFLEN, 0)) < 0, "exit send");
			close(index);
		}
		++index;
	}
	return 0;
}

/* Inserts a client in the list of subscribed clients */
void add_to_end_list(CLIENT_STRUCT* client, int ind, TABLE_STRUCT* map, TCP_MSG_STRUCT* tcp_message) {
	LIST_STRUCT found = NULL;
	LIST_STRUCT list = map->array[ind].clients;
	while(list) {
		if(strcmp(list->info.id, client->id)) {
			list = list->next;
			continue;
		}

		found = list;
		list = NULL;
	}
	if(found != NULL) {
		found->info.SF = tcp_message->SF;
		return;
	} 
	map->array[ind].clients = add_to_start_cli(*client, map->array[ind].clients);
}

/* Deletes a client from the list */
void delete_from_list(TABLE_STRUCT* map, int index, CLIENT_STRUCT* client) {
	LIST_STRUCT prev;
	LIST_STRUCT crt = prev = NULL;
	LIST_STRUCT list = map->array[index].clients;
	
	while(list) {
		if (strcmp(list->info.id, client->id)) {
			prev = list;
			list = list->next;
		} else {
			crt = list;
			list = NULL;
		}
	}
	if (crt == NULL) return;
	
	if(prev) {
		prev->next = crt->next;
		return;
	}
	
	map->array[index].clients = map->array[index].clients->next;
	return;
}

/* Creates the structure of a udp client */
void complete_udp_cli(struct sockaddr_in* udp_cli_addr, int portno) {
	udp_cli_addr->sin_port = htons(portno);
	udp_cli_addr->sin_family = AF_INET;
	udp_cli_addr->sin_addr.s_addr = INADDR_ANY;
}

/* checks that the specific client is associated with the socket */
int has_socket(int sock, CLIENT_STRUCT* clients, int index) {
	return (clients + index)->sockfd == sock;
}

/* Creates the structure of a server address */
void complete_serv_addr(struct sockaddr_in* serv_addr, int portno)  {
	serv_addr->sin_port = htons(portno);
	serv_addr->sin_family = AF_INET;
	serv_addr->sin_addr.s_addr = INADDR_ANY;
}

/* Swaps the clients given by indices a and b */
void swapClients(CLIENT_STRUCT* clients, int a, int b) {
	CLIENT_STRUCT aux = clients[a];
	clients[a] = clients[b];
	clients[b] = aux;
}

/*Removes client with the specific socket */
void delete_client(int* count, CLIENT_STRUCT* clients, int sock) {
	int i = 0;
	char* template = "Client %s disconnected.\n";
	while (i < (*count)) {
		if(has_socket(sock, clients, i)) {
			printf(template, clients[i].id);
			swapClients(clients, i, (*count) - 1);
			(*count)--;
		}
		++i;
	}
}

/*Function used when the server receives a message from the client. It
can be subscribe or unsubscribe */
void receive_from_client(int descriptor_max, fd_set* tmp_fds, fd_set* read_fds,
CLIENT_STRUCT* clients, int* dim, TABLE_STRUCT* map) {

	int crt_sock = 1;
	int tcp_msg_siz = sizeof(TCP_MSG_STRUCT);
	int cli_siz = sizeof(CLIENT_STRUCT);
	CLIENT_STRUCT* client;
	TOPIC_STRUCT top;
	
	while (crt_sock <= descriptor_max) {
		TCP_MSG_STRUCT* tcp_message = calloc(1, tcp_msg_siz); 
		if(!FD_ISSET(crt_sock, tmp_fds)) {
			crt_sock++;
			continue;
		}

		int n = recv(crt_sock, tcp_message, tcp_msg_siz, 0);
		DIE (n < 0, "recv msg");

		if (n == 0) {
			delete_client(dim, clients, crt_sock);
			FD_CLR(crt_sock, read_fds);
			close(crt_sock);
		} else {

			top = create_topic(tcp_message->topic);
			client = calloc(1, cli_siz);
			int i = 0;
			while (i < *dim) {
				if((clients + i)->sockfd != crt_sock) {
					++i; continue;
				}
				memmove(client, clients + (i++), cli_siz);
			}
			top.clients = add_to_start_cli(*client, top.clients);
			int flag = 0;

			client->SF = tcp_message->SF;
			if(tcp_message->type == TCP_SUBSCRIBE) {
				i = 0;
				flag = 0;
				while(i < map->crt_len) {

					if(!strncmp(map->array[i].topic, tcp_message->topic, TOPIC_SIZE)) {
						flag = 1;

						add_to_end_list(client, i, map, tcp_message);
						break;

					}
					++i;
				}
				if (flag) {
					free(tcp_message);
					crt_sock++;
					continue;
				}
				push_topic(map, top);
			} else if(tcp_message->type == TCP_UNSUBSCRIBE) {
				int j;
				j = 0;
				while(j < map->crt_len) {
					if(!strcmp(map->array[j].topic, tcp_message->topic)) {
						delete_from_list(map, j, client);
						
						break;
					}
					++j;
				}
			}
		}
		free(tcp_message);
		
		crt_sock++;
	}
}

/*Disables the NAGLE algorithm */
void disable_nagle(int sockfd) {
	int flag = 1;
	int ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,
	(char*)&flag, sizeof(int));
	if(ret < 0) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
}

/* Receives a message from the UDP clients */
void receive_from_udp(UDP_MSG_STRUCT* udp_message, int sockfd, struct sockaddr_in* client_address,
socklen_t* client_len) {


	int ret = recvfrom(sockfd, udp_message, sizeof(UDP_MSG_STRUCT), 0, 
						(struct sockaddr*) client_address, client_len);

	udp_message->ip = client_address->sin_addr;
	udp_message->port = client_address->sin_port;

	if (ret < 0) {
		perror("recv udp");
		exit(EXIT_FAILURE);
	}

	return;
}

/* Searches for a client with a specific ID. */
int find_client(int clients_no, CLIENT_STRUCT* arr, char buf[BUFLEN], int sock){
	char* template = "Client %s already connected.\n";
	int i = 0;

	int failure_code = 1;
	while(i < clients_no) {
		char msg[BUFLEN];
		strcpy(msg, "error");
		if (strcmp (buf, arr[i].id)) {
			i++; continue;
		}

		printf(template, buf);

		int ret = send(sock, msg, BUFLEN, 0);
		if(ret < 0) {
			perror("send msg");
			exit(EXIT_FAILURE);
		}

		close(sock);
		return failure_code;
	}
	return 0;
}

/* Used to reallocate memory when the clients capacity has exceeded */
void reallocate(CLIENT_STRUCT** clients_addr, int* clients_cap, int dup, int size) {
	*clients_addr = realloc(*clients_addr, *clients_cap * size * dup);
	*clients_cap *= dup;
}


int max(int a, int b) {
	if (a > b) return a;
	return b;
}

int cnt = 0;
int cap_clients = 1;
int storage_cnt = 0;
int storage_cap = 1;
int descriptor_max;

int main(int argc, char *argv[])
{
	//Initialise all the structures needed for communication:
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int tcp_sockfd, udp_sockfd;
	socklen_t tcp_cli_len;
	int port_num;
	int map_index = 0;
	port_num = atoi(argv[1]);
	if(port_num <= 0) {
		perror("Bad port..");
		exit(EXIT_FAILURE);
	}
	int ret;
	int size = sizeof(struct sockaddr);
	socklen_t udp_cli_len = sizeof(struct sockaddr_in);
	struct sockaddr_in serv_addr, tcp_cli_addr, udp_cli_addr;
	fd_set tmp_fds;
	fd_set read_fds;

	TABLE_STRUCT* map = create_table();
	CLIENT_STRUCT* clients = calloc(cap_clients, sizeof(CLIENT_STRUCT));
	STORAGE_STRUCT* storage = calloc(storage_cap, sizeof(STORAGE_STRUCT));
	udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (udp_sockfd < 0) {
		perror("sock udp");
		exit(EXIT_FAILURE);
	}

	int storage_index = 0;
	tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (tcp_sockfd < 0) {
		perror("tcp sock");
		exit(EXIT_FAILURE);
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	complete_udp_cli(&udp_cli_addr, port_num);

	tcp_cli_len = sizeof(tcp_cli_addr);
	complete_serv_addr(&serv_addr, port_num);

	disable_nagle(tcp_sockfd);
	int crt_sock = 0;

	ret = bind(tcp_sockfd, (struct sockaddr *) &serv_addr, size);
	if (ret < 0) {
		perror("tcp bind failed");
		exit(EXIT_FAILURE);
	}

	ret = bind(udp_sockfd, (struct sockaddr *) &udp_cli_addr, size);
	if (ret < 0) {
		perror("udp bind failed");
		exit(EXIT_FAILURE);
	}

	ret = listen(tcp_sockfd, MAX_CLIENTS);
	if (ret < 0) {
		perror("listen failed");
		exit(EXIT_FAILURE);
	}

	descriptor_max = max(tcp_sockfd, udp_sockfd);
	FD_SET(0, &read_fds);
	FD_SET(tcp_sockfd, &read_fds);
	FD_SET(udp_sockfd, &read_fds);
	
	while (TRUE) {
		UDP_MSG_STRUCT udp_msg;
		tmp_fds = read_fds; 		
		DIE((ret = select(descriptor_max + 1, &tmp_fds, NULL, NULL, NULL)) < 0, "err select");

		if (FD_ISSET(udp_sockfd, &tmp_fds)) {
			receive_from_udp(&udp_msg, udp_sockfd, &udp_cli_addr, &udp_cli_len);
			int udp_siz = sizeof(UDP_MSG_STRUCT);
			LIST_STRUCT list;
			for(int j = 0; j < map->crt_len; j++) {
				list = map->array[j].clients;
				if(strncmp(map->array[j].topic, udp_msg.topic,TOPIC_SIZE) == 0) {

					while(list) {
						int sock = list->info.sockfd;

						if(FD_ISSET(sock, &read_fds)) {
							int ret = send(sock, &udp_msg, udp_siz, 0);
							DIE(ret < 0, "send");
						} else if(list->info.SF == 1) {

							int found = 0;

							add_to_storage_table(&found, &storage, &storage_cnt, &udp_msg, list);

							create_storage(&found, &storage, 2, sizeof(STORAGE_STRUCT), &storage_cnt, &udp_msg, &storage_cap, list);

						}
						list = list->next;
					}
				}
			}
		}

		//Check if we received exit signal
		else if(FD_ISSET(0, &tmp_fds)) {
			if(recv_exit(descriptor_max, &read_fds, udp_sockfd, tcp_sockfd) == 0) {
				goto end;
			}
		}
		//Check if the client has connected
		else if(FD_ISSET(tcp_sockfd, &tmp_fds)) {
			CLIENT_STRUCT clnt;

			clnt.SF = 0;
			char buf[BUFLEN];
			char msg[BUFLEN];
			strcpy(msg, "success");

			int cli_id_dim = 10;
			int newsockfd = accept(tcp_sockfd, (struct sockaddr*) &tcp_cli_addr, &tcp_cli_len);
			DIE(newsockfd < 0, "accept failed");

			int ret = recv(newsockfd, buf, BUFLEN, 0);
			DIE (ret < 0, "fail recv");
			memcpy(clnt.id, buf, cli_id_dim);
			char* template = "New client %s connected from %s:%d.\n";
			int ok = 0, k = 0;
			descriptor_max = max(newsockfd, descriptor_max);
			char* ip_conn = inet_ntoa(tcp_cli_addr.sin_addr);
			int port_conn = ntohs(tcp_cli_addr.sin_port);

			if(find_client(cnt, clients, buf, newsockfd))
				continue;

			ret = send(newsockfd, msg, BUFLEN, 0);
			DIE(ret < 0, "send");
			int new_cnt = (cnt)++;

			clnt.sockfd = newsockfd;
			*((clients) + new_cnt) = clnt;

			printf(template,
					buf, ip_conn,
					port_conn);

			disable_nagle(newsockfd);
			FD_SET(newsockfd, &read_fds);

			if(cnt == cap_clients) {
				reallocate(&clients, &cap_clients, 2, sizeof(CLIENT_STRUCT));
			}

			int k_index;

			while (k < storage_cnt) {
				if (strcmp(storage[k].id, clnt.id)) {
					k++;
				}
				else {
					ok = 1;
					k_index = k;
					k = storage_cnt;
				}
			}

			//Try to send messages to clients with SF = 1;
			if(ok) {
				UDP_LIST_STRUCT p = (storage + k_index)->msg;
				while(p) {
					int message_size = sizeof(UDP_MSG_STRUCT);
					ret = send(newsockfd, &p->info, message_size, 0);
					
					if (ret < 0) {
						perror("backup send");
						exit(EXIT_FAILURE);
					}
					
					p = p->next;
				}
				(storage + k_index)->msg = NULL;
			}
		} else {
			receive_from_client(descriptor_max, &tmp_fds, &read_fds, clients, &cnt, map);		
		}
	}
end:

	//Deallocate all the structures; close all the sockets
	while ( crt_sock <= descriptor_max) {
		if (!FD_ISSET(crt_sock, &read_fds)) {
			crt_sock++; continue;
		}
		close(crt_sock++);
	}

	while (storage_index < storage_cnt) {
		UDP_LIST_STRUCT l = storage[storage_index++].msg;
		while (l != NULL) {
			UDP_LIST_STRUCT aux = l->next;
			free(l);
			l = aux;
		}
	}
	while (map_index < map->crt_len) {
		LIST_STRUCT l = map->array[map_index++].clients;
		while (l != NULL) {
			LIST_STRUCT temp = l->next;
			free(l);
			l = temp;
		}
	}

	free(storage);
	free(clients);
	free(map->array);
	close(udp_sockfd);
	close(tcp_sockfd);
	free(map);
}