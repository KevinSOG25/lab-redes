#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE   1024
#define SOCKETERROR   (-1)

int check(int exp, const char *msg);

int main(int argc, char **argv){

    setbuf(stdout, NULL);

    // se puede suscribir a uno o mas temas: ./subscriber 6001 broker 5000 deportes clima
    if(argc < 5){
        fprintf(stderr, "Usage: %s <my_port> <broker_ip> <broker_port> <topic> [topic2 ...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int my_port          = atoi(argv[1]);
    const char *broker_ip   = argv[2];
    int broker_port      = atoi(argv[3]);

    // socket que sigue la logica de los anteriores pero para suscriber
    int udp_socket;
    if((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("Error creando socket");
        return EXIT_FAILURE;
    }

    // al igul que broker este debe tener bind que se encarga de asignarle un puerto de escucha para que el broker sepa a donde enviar los mensajes 
    struct sockaddr_in my_addr = {.sin_family = AF_INET,.sin_addr.s_addr = INADDR_ANY,.sin_port = htons(my_port)};
    check(bind(udp_socket, (struct sockaddr*)&my_addr, sizeof(my_addr)),"Error en bind");

    // getaddrinfo se encarga de resolver la direccion del broker a una estructura para usar luego el sendto
    //snprintf se encarga de convertir el puerto a cadena de texto 
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM};
    struct addrinfo *res;
    char broker_port_str[16];
    snprintf(broker_port_str, sizeof(broker_port_str), "%d", broker_port);
    if(getaddrinfo(broker_ip, broker_port_str, &hints, &res) != 0){
        fprintf(stderr, "Error resolviendo host '%s'\n", broker_ip);
        close(udp_socket);
        return EXIT_FAILURE;
    }

    // sendto que en este este caso es para registrarse en el broker, de manera que cuando el broker reciba un mensaje de un publisher
    // en alguno de estos temas, sepa a donde enviarlo. Se envia un "SUBSCRIBE <tema>" por cada tema indicado
    for(int i = 4; i < argc; i++){
        const char *topic = argv[i];
        if(topic[0] == '\0' || strchr(topic, ' ') != NULL){
            fprintf(stderr, "Tema invalido '%s' (no puede estar vacio ni tener espacios), se omite\n", topic);
            continue;
        }
        char sub_msg[BUFFER_SIZE];
        int len = snprintf(sub_msg, sizeof(sub_msg), "SUBSCRIBE %s", topic);
        check(sendto(udp_socket, sub_msg, len + 1, 0, res->ai_addr, res->ai_addrlen), "Error enviando SUBSCRIBE");
        printf("[SUBSCRIBER] Suscrito al tema '%s'\n", topic);
    }
    // freeaddrinfo libera la memoria que reservo getaddrinfo ya que no la necesitamos mas despues de enviar el subscribe
    freeaddrinfo(res);

    printf("[SUBSCRIBER] Registrado en broker %s:%d, esperando mensajes...\n",broker_ip, broker_port);

    while(1){
        char buffer[BUFFER_SIZE];
        struct sockaddr_in sender_addr;
        socklen_t sender_len = sizeof(sender_addr);

        // recvfrom bloquea hasta recibir un datagrama UDP que en este caso es del broker que lo envia al subscriber, 
        // el buffer se encarga de almacenar el mensaje recibido 
        int bytes = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*)&sender_addr, &sender_len);
        check(bytes, "Error en recvfrom");
        buffer[bytes] = '\0';

        // inet_ntop se encarga de convertir la direccion IP binaria del broker a cadena de texto legible para mostrar quien envio mensajes y quien se suscribio
        char sender_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, sizeof(sender_ip));

        printf("[SUBSCRIBER] Mensaje de %s:%d -> \"%s\"\n", sender_ip, ntohs(sender_addr.sin_port), buffer);
    }

    // close cierra el socket y libera los recursos que estaba usando
    close(udp_socket);
    return EXIT_SUCCESS;
}

int check(int exp, const char *msg){
    if(exp == SOCKETERROR){
        perror(msg);
        exit(EXIT_FAILURE);
    }
    return exp;
}