#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define MAX_CLIENTS	50	
#define BUFLEN		1000
#define STRING_MSG 3
#define FLOAT_MSG 2
#define SHORT_MSG 1
#define INT_MSG 0

typedef struct {
	char topic[50];
	unsigned char data_type;
	char content[1500];
	struct in_addr ip;
	unsigned short port;
} UDP_MSG_STRUCT;

typedef struct {
	char topic[51];
	unsigned int type;
	unsigned int SF;
} TCP_MSG_STRUCT;

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

double adjust(double value, int pow) {
    while (pow-- > 0) {
        value /= 10;
    }
    return value;
}

void disable_neagle(int sockfd, char* flag) {
        int result = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, flag, sizeof(int));
    DIE(result < 0, "setsockopt");
}

int main(int argc, char *argv[]) {

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc < 4) {
        exit(0);
    }

    int sockfd, ret;
    struct sockaddr_in serv_addr;
    char buf[BUFLEN];
    int n;
    fd_set tmp_file_descriptors;
    fd_set read_file_descriptors;

    char* ip_address = argv[1];
    int ip_address_len = strlen(ip_address) + 1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    char* port = argv[2];

    ret = inet_aton(port, &serv_addr.sin_addr);
    if(ret == 0) {
        perror("port");
        exit(EXIT_FAILURE);
    }
    
    int host_port = atoi(argv[3]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(host_port);

    FD_ZERO(&read_file_descriptors);
    FD_ZERO(&tmp_file_descriptors);

    ret = connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if(ret < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    n = send(sockfd, argv[1], ip_address_len, 0);
    if(n < 0) {
        perror("connect");
        exit(EXIT_FAILURE);  
    }
    char* delim = " \n";

    FD_SET(sockfd, &read_file_descriptors);
    FD_SET(0, &read_file_descriptors);

    char flag = 1;
    disable_neagle(sockfd, &flag);

    ret = recv(sockfd, &buf, BUFLEN, 0);
    if(ret < 0) {
        perror("id");
        exit(EXIT_FAILURE);
    }
    if(strcmp("error", buf) == 0) {
        close(sockfd);
        return 0;
    }

    while (flag) {
    	int max_select = sockfd + 1;
        tmp_file_descriptors = read_file_descriptors;

        ret = select(max_select, &tmp_file_descriptors, NULL, NULL, NULL);
        if(ret < 0) {
            perror("select");
            exit(EXIT_FAILURE);
        }
        if (FD_ISSET(0, &tmp_file_descriptors)) {
			TCP_MSG_STRUCT tcp_msg;
            fgets(buf, BUFLEN - 1, stdin);

            char* aux_buffer = calloc(strlen(buf) + 1, sizeof(char));
            strcpy(aux_buffer, buf);

			char* tok = strtok(aux_buffer, delim);

			if(tok == NULL) continue;
            
            if (!strcmp(tok, "exit")) {
                flag = 0;
            }else if(strcmp(tok, "subscribe") == 0) {

			    printf("Subscribed to topic.\n");

				tok = strtok(NULL, delim);
				char* top = strdup(tok);
				
				tok = strtok(NULL, delim);
				unsigned int SF = atoi(tok);
				int tcp_siz = sizeof(TCP_MSG_STRUCT);
				tcp_msg.type = 1;
				strcpy(tcp_msg.topic, top);
				tcp_msg.SF = SF;

                free(top);
				ret = send(sockfd, &tcp_msg, tcp_siz, 0);
				if(ret < 0) {
                    perror("subscribe message");
                    exit(EXIT_FAILURE);
                }
            } else if(strcmp(tok, "unsubscribe") == 0) {

				tok = strtok(NULL, delim);
				char* topic = strdup(tok);
				
				strcpy(tcp_msg.topic, topic);
				tcp_msg.SF = -1;
				tcp_msg.type = 0;
                free(topic);
                int tcp_siz = sizeof(TCP_MSG_STRUCT);

				ret = send(sockfd, &tcp_msg, tcp_siz, 0);
                if (ret < 0) {
                    perror("unsubscribe message");
                    exit(EXIT_FAILURE);
                }
                printf("Unsubscribed from topic.\n");
            }
            free(aux_buffer);
        }

        if (FD_ISSET(sockfd, &tmp_file_descriptors)) {
			UDP_MSG_STRUCT udp_msg;
		    ret = recv(sockfd, &udp_msg, sizeof(udp_msg), 0);
            if (ret < 0) {
                perror("recv err");
                exit(EXIT_FAILURE);
            }
     		int32_t integer_val;

            if(strncmp(udp_msg.topic, "exit", 4) == 0) {
                close(sockfd);
                return 0;
            }
            int sign_bit = udp_msg.content[0] == 1 ? - 1 : 1;

            if(udp_msg.data_type == FLOAT_MSG) {
                
				int8_t power = udp_msg.content[5];
				double float_val = (double)ntohl(*((uint32_t*)(&udp_msg.content[1])));
				
                float_val *= sign_bit;
                while(power > 0) {
                    float_val /= 10;
                    power-= 1;
                }

				printf("%s:%d - %s - FLOAT - %f\n", inet_ntoa(udp_msg.ip),
				ntohs(udp_msg.port), udp_msg.topic, float_val);

            } else if(udp_msg.data_type == STRING_MSG) {

				printf("%s:%d - %s - STRING - %s\n", inet_ntoa(udp_msg.ip),
				ntohs(udp_msg.port), udp_msg.topic, udp_msg.content);

            } else if(udp_msg.data_type == INT_MSG) {
				integer_val = ntohl(*((uint32_t*)(&udp_msg.content[1])));

				printf("%s:%d - %s - INT - %d\n", inet_ntoa(udp_msg.ip),
				ntohs(udp_msg.port), udp_msg.topic, integer_val * sign_bit);

            } else if(udp_msg.data_type == SHORT_MSG) {
				double short_real = ((double)ntohs(*((uint16_t*)(udp_msg.content)))) / 100;

				printf("%s:%d - %s - SHORT_REAL - %.2f\n", inet_ntoa(udp_msg.ip),
				ntohs(udp_msg.port), udp_msg.topic, short_real);
            }
        } 
    }

    close(sockfd);
    return 0;
}

