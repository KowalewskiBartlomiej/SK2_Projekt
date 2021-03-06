#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <pthread.h>

#define SERVER_PORT 1234
#define QUEUE_SIZE 5
#define MAX_TOPICS 10
#define MAX_USERS 3
#define TOPIC_LENGTH 99
#define MAX_MESSAGES 100
#define BUFFSIZE 302

pthread_mutex_t topics_m = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t subs_m = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mess_m = PTHREAD_MUTEX_INITIALIZER;
pthread_t threads[MAX_USERS];

char topics[MAX_TOPICS][TOPIC_LENGTH];
int connection_client_descriptors[MAX_USERS];
int subscriptions[MAX_USERS][MAX_TOPICS] = {0};
char messages[MAX_TOPICS][MAX_MESSAGES][BUFFSIZE] = {0};

//struktura zawierająca dane, które zostaną przekazane do wątku
struct thread_data_t
{
    int serverStatus;
    int socket_nr;
    int *topics_num;
    char tekst[BUFFSIZE];
};

struct message
{
    char akcja;
    int topic_len;
    char tytul[TOPIC_LENGTH];
};

int findFreeIndex(int topic)
{
    for (int i = 0; i < MAX_MESSAGES; i++)
    {
        if (strlen(messages[topic][i]) == 0)
            return i;
    }
    return -1;
}

int numberOfMess(int user)
{
    int sum = 0;
    for (int i = 0; i < MAX_TOPICS; i++)
    {
        if (subscriptions[user][i] == 1)
            sum += (findFreeIndex(i));
    }
    return sum;
}

int readError(int flag)
{
    if (flag == -1)
    {
        fprintf(stderr, "Błąd przy próbie odczytania wiadomosci.\n");
    }
    else if (flag == 0)
    {
        fprintf(stderr, "Gniazdo jest zamknięte.r\n");
    }
    return flag;
}

int writeFeedback(int flag)
{
    if (flag == -1)
    {
        fprintf(stderr, "Błąd przy próbie wysłania wiadomosci.\n");
    }
    else if (flag == 0)
    {
        fprintf(stderr, "Gniazdo jest zamknięte.w\n");
    }
    return flag;
}

int topicExist(char topic[TOPIC_LENGTH])
{
    for (int i = 0; i < MAX_TOPICS; i++)
    {
        if (strncmp(topics[i], topic, TOPIC_LENGTH) == 0)
            return i;
    }
    return -1;
}

int addConnection(int client)
{
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (connection_client_descriptors[i] == -1)
        {
            connection_client_descriptors[i] = client;
            fprintf(stderr, "Nawiązano nowe połączenie!\n");
            write(client, "Nawiązano połączenie!\n", 100);
            return i;
        }
    }
    fprintf(stderr, "Serwer jest pełny!\n");
    write(client, "rSerwer jest pełny! Spróbuj ponownie później\n", 100);
    return -1;
}

int writeFeedbackMsg(int nr, char *error_msg)
{
    long len = strlen(error_msg);
    int feedbackW = writeFeedback(write(connection_client_descriptors[nr], error_msg, len));
    return feedbackW;
}

void printSubs()
{
    for (int i = 0; i < MAX_USERS; i++)
    {
        for (int j = 0; j < MAX_TOPICS; j++)
        {
            printf("%d ", subscriptions[i][j]);
        }
        printf("\n");
    }
}

void cleanAfterClient(int index)
{
    for (int i = 0; i < MAX_TOPICS; i++)
    {
        subscriptions[index][i] = 0;
    }
    connection_client_descriptors[index] = -1;
}

//funkcja opisującą zachowanie wątku - musi przyjmować argument typu (void *) i zwracać (void *)
void *ThreadBehavior(void *t_data)
{
    pthread_detach(pthread_self());
    struct thread_data_t *th_data = (struct thread_data_t *)t_data;
    int nr = (*th_data).socket_nr;
    struct message mess;
    int feedbackW;
    char subscribed_topic[TOPIC_LENGTH];
    char mark[2] = "*";

    while (1)
    {   
        if ((*th_data).serverStatus == 0)
        {
            break;
        }
        bzero((*th_data).tekst, sizeof((*th_data).tekst));
        int errR = readError(read(connection_client_descriptors[nr], (*th_data).tekst, sizeof((*th_data).tekst)));
        printf("%s\n", (*th_data).tekst);
        if (errR == -1)
        {
            exit(1);
        }
        else if (errR == 0)
        {
            connection_client_descriptors[nr] = -1;
            break;
        }
        else
        {
            mess.akcja = (*th_data).tekst[0];
            if (strncmp((*th_data).tekst, "e", 1) != 0 && strncmp((*th_data).tekst, "t", 1) != 0 && strncmp((*th_data).tekst, "w", 1) != 0)
            {
                mess.topic_len = ((*th_data).tekst[1] - '0') * 10 + (*th_data).tekst[2] - '0';
                bzero(mess.tytul, sizeof(mess.tytul));
                printf("po atoi: %d\n", mess.topic_len);
                for (int i = 3; i < mess.topic_len + 3; i++)
                {
                    mess.tytul[i - 3] = (*th_data).tekst[i];
                }
                printf("tytul: %s\n", mess.tytul);
            }

            // Rozlaczenie z serwerem
            if (strncmp((*th_data).tekst, "e", 1) == 0)
            {
                printf("Rozlaczono klienta o numerze: %d\n", nr);
                write(connection_client_descriptors[nr], (*th_data).tekst, sizeof((*th_data).tekst));
                cleanAfterClient(nr);
                break;
            }
            //wysyłanie tematów
            if (strncmp((*th_data).tekst, "t", 1) == 0)
            {
                char str[2];
                bzero(str, sizeof(str));
                pthread_mutex_lock(&topics_m);
                sprintf(str, "%d", *(*th_data).topics_num);
                write(connection_client_descriptors[nr], str, sizeof(str));
                for (int i = 0; i < *(*th_data).topics_num; i++)
                {
                    if(subscriptions[nr][i] == 1)
                    {
                        bzero(subscribed_topic, sizeof(subscribed_topic));
                        strncpy(subscribed_topic, topics[i], TOPIC_LENGTH);
                        strcat(subscribed_topic, mark);
                        write(connection_client_descriptors[nr], subscribed_topic, sizeof(topics[i]));
                    }
                    else
                    {
                        write(connection_client_descriptors[nr], topics[i], sizeof(topics[i]));
                    }
                }
                pthread_mutex_unlock(&topics_m);
            }

            //Odbieranie wiadomości
            if (strncmp((*th_data).tekst, "w", 1) == 0)
            {
                char str[2];
                bzero(str, sizeof(str));
                pthread_mutex_lock(&subs_m);
                sprintf(str, "%d", numberOfMess(nr));
                printf("%d\n", numberOfMess(nr));
                write(connection_client_descriptors[nr], str, sizeof(str));
                pthread_mutex_lock(&mess_m);
                for (int i = 0; i < MAX_TOPICS; i++)
                {
                    for (int j = 0; j < findFreeIndex(i); j++)
                    {
                        if (subscriptions[nr][i] == 1)
                        {
                            write(connection_client_descriptors[nr], messages[i][j], sizeof(messages[i][j]));
                        }
                    }
                }
                pthread_mutex_unlock(&mess_m);
                pthread_mutex_unlock(&subs_m);
            }

            // Wyslanie wiadomosci
            if (strncmp((*th_data).tekst, "s", 1) == 0)
            {
                pthread_mutex_lock(&topics_m);
                int topic_index = topicExist(mess.tytul);
                pthread_mutex_unlock(&topics_m);
                printf("%s %d\n", mess.tytul, topic_index);
                if (topic_index == -1)
                {
                    feedbackW = writeFeedbackMsg(nr, "rPodany temat nie istnieje!");
                    if (feedbackW == -1)
                        exit(1);
                    else if (feedbackW == 0)
                    {
                        cleanAfterClient(nr);
                        break;
                    }
                }
                else
                {
                    pthread_mutex_lock(&mess_m);
                    int message_index = findFreeIndex(topic_index);
                    printf("Wysylam wiadomosc o temacie %s\n", topics[topic_index]);
                    if (message_index != -1)
                        strncpy(messages[topic_index][findFreeIndex(topic_index)], (*th_data).tekst, sizeof((*th_data).tekst));
                    pthread_mutex_unlock(&mess_m);
                    feedbackW = writeFeedbackMsg(nr, "Wiadomość dotarła do serwera!");
                    if (feedbackW == -1)
                        exit(1);
                    else if (feedbackW == 0)
                    {
                        cleanAfterClient(nr);
                        break;
                    }
                }
            }
            // Dodanie tematu
            if (strncmp((*th_data).tekst, "a", 1) == 0)
            {
                if (*(*th_data).topics_num < MAX_TOPICS)
                {
                    if (topicExist(mess.tytul) != -1)
                    {
                        printf("Istnieje juz temat o podanej nazwie!\n");
                        feedbackW = writeFeedbackMsg(nr, "rIstnieje juz temat o podanej nazwie!");
                        if (feedbackW == -1)
                        {
                            exit(1);
                        }
                        else if (feedbackW == 0)
                        {
                            cleanAfterClient(nr);
                            break;
                        }
                    }
                    else
                    {
                        pthread_mutex_lock(&topics_m);
                        strcpy(topics[*(*th_data).topics_num], mess.tytul);
                        printf("Dodano nowy temat: %s\n", topics[*(*th_data).topics_num]);
                        *(*th_data).topics_num += 1;
                        for (int i = 0; i < *(*th_data).topics_num; i++)
                        {
                            printf("Temat %d: %s\n", i, topics[i]);
                        }
                        pthread_mutex_unlock(&topics_m);
                        feedbackW = writeFeedbackMsg(nr, "Dodano nowy temat!");
                        if (feedbackW == -1)
                            exit(1);
                        else if (feedbackW == 0)
                        {
                            cleanAfterClient(nr);
                            break;
                        }
                    }
                }
                else
                {
                    printf("Przekroczono maksymalna ilosc tematow!\n");
                    feedbackW = writeFeedbackMsg(nr, "rPrzekroczono maksymalna ilosc tematow!");
                    if (feedbackW == -1)
                        exit(1);
                    else if (feedbackW == 0)
                    {
                        cleanAfterClient(nr);
                        break;
                    }
                }
            }
            // Subskrypcja tematu
            if (strncmp((*th_data).tekst, "f", 1) == 0)
            {
                pthread_mutex_lock(&topics_m);
                int index = topicExist(mess.tytul);
                pthread_mutex_unlock(&topics_m);
                if (index != -1)
                {
                    pthread_mutex_lock(&subs_m);
                    if (subscriptions[nr][index] == 1)
                    {
                        printf("Uzytkownik %d juz subskrybuje temat %s!\n", nr, topics[index]);
                        feedbackW = writeFeedbackMsg(nr, "rJuz subskrybujesz ten temat!");
                        if (feedbackW == -1)
                            exit(1);
                        else if (feedbackW == 0)
                        {
                            cleanAfterClient(nr);
                            break;
                        }
                    }
                    else
                    {
                        subscriptions[nr][index] = 1;
                        printSubs();
                        feedbackW = writeFeedbackMsg(nr, "Zasubskrybowano podany temat!");
                        if (feedbackW == -1)
                            exit(1);
                        else if (feedbackW == 0)
                        {
                            cleanAfterClient(nr);
                            break;
                        }
                    }
                    pthread_mutex_unlock(&subs_m);
                }
                else
                {
                    printf("Podany temat nie istnieje!\n");
                    feedbackW = writeFeedbackMsg(nr, "rPodany temat nie istnieje!");
                    if (feedbackW == -1)
                        exit(1);
                    else if (feedbackW == 0)
                    {
                        cleanAfterClient(nr);
                        break;
                    }
                }
            }
            // Anulowanie Subskrypcji
            if (strncmp((*th_data).tekst, "u", 1) == 0)
            {
                pthread_mutex_lock(&topics_m);
                int index = topicExist(mess.tytul);
                pthread_mutex_unlock(&topics_m);
                if (index != -1)
                {
                    pthread_mutex_lock(&subs_m);
                    if (subscriptions[nr][index] == 0)
                    {
                        printf("Uzytkownik %d nie subskrybuje tematu %s!\n", nr, topics[index]);
                        feedbackW = writeFeedbackMsg(nr, "rNie subskrybujesz tego tematu!");
                        if (feedbackW == -1)
                            exit(1);
                        else if (feedbackW == 0)
                        {
                            cleanAfterClient(nr);
                            break;
                        }
                    }
                    else
                    {
                        subscriptions[nr][index] = 0;
                        printSubs();
                        feedbackW = writeFeedbackMsg(nr, "Anulowano subskrypcję podanego tematu!");
                        if (feedbackW == -1)
                            exit(1);
                        else if (feedbackW == 0)
                        {
                            cleanAfterClient(nr);
                            break;
                        }
                    }
                    pthread_mutex_unlock(&subs_m);
                }
                else
                {
                    printf("Podany temat nie istnieje!\n");
                    feedbackW = writeFeedbackMsg(nr, "rPodany temat nie istnieje!");
                    if (feedbackW == -1)
                        exit(1);
                    else if (feedbackW == 0)
                    {
                        cleanAfterClient(nr);
                        break;
                    }
                }
            }
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    int server_socket_descriptor;
    int connection_socket_descriptor;
    int bind_result;
    int listen_result;
    int topics_num = 4;
    char reuse_addr_val = 1;
    struct sockaddr_in server_address;

    strcpy(topics[0], "football");
    strcpy(topics[1], "e-sport");
    strcpy(topics[2], "books");
    strcpy(topics[3], "computers");

    //inicjalizacja gniazda serwera

    memset(&server_address, 0, sizeof(struct sockaddr));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(SERVER_PORT);

    server_socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_descriptor < 0)
    {
        fprintf(stderr, "%s: Błąd przy próbie utworzenia gniazda..\n", argv[0]);
        exit(1);
    }
    setsockopt(server_socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_addr_val, sizeof(reuse_addr_val));

    bind_result = bind(server_socket_descriptor, (struct sockaddr *)&server_address, sizeof(struct sockaddr));
    if (bind_result < 0)
    {
        fprintf(stderr, "%s: Błąd przy próbie dowiązania adresu IP i numeru portu do gniazda.\n", argv[0]);
        exit(1);
    }

    listen_result = listen(server_socket_descriptor, QUEUE_SIZE);
    if (listen_result < 0)
    {
        fprintf(stderr, "%s: Błąd przy próbie ustawienia wielkości kolejki.\n", argv[0]);
        exit(1);
    }

    for (int i = 0; i < sizeof(connection_client_descriptors) / sizeof(int); i++)
        connection_client_descriptors[i] = -1;

    struct thread_data_t *t_data = malloc(sizeof(struct thread_data_t));
    (*t_data).serverStatus = 1;
    (*t_data).topics_num = &topics_num;

    while (1)
    {
        connection_socket_descriptor = accept(server_socket_descriptor, NULL, NULL);
        if (connection_socket_descriptor < 0)
        {
            fprintf(stderr, "%s: Błąd przy próbie utworzenia gniazda dla połączenia.\n", argv[0]);
            exit(1);
        }
        int nr = addConnection(connection_socket_descriptor);
        for (int i = 0; i < MAX_USERS; i++)
            printf("Klient id: %d\n", connection_client_descriptors[i]);
        if (nr >= 0)
        {
            (*t_data).socket_nr = nr;
            int create_result = pthread_create(&threads[nr], NULL, ThreadBehavior, (void *)t_data);
            if (create_result)
            {
                printf("Błąd przy próbie utworzenia wątku, kod błędu: %d\n", create_result);
                (*t_data).serverStatus = 0;
                exit(-1);
            }
        }
    }
    close(server_socket_descriptor);
    free(t_data);
    pthread_mutex_destroy(&topics_m);
    pthread_mutex_destroy(&subs_m);
    pthread_mutex_destroy(&mess_m);
    return (0);
}