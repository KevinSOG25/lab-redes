#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE        1024
#define MAX_SUBSCRIPTIONS  50
#define TOPIC_SIZE         64
#define SUBSCRIBE_PREFIX   "SUBSCRIBE "
#define PUBLISH_PREFIX     "PUBLISH "

// cada suscripcion asocia la direccion de un subscriber con un tema
// un mismo subscriber puede tener varias suscripciones (una por cada tema)
struct subscription {
    struct sockaddr_in addr;
    char topic[TOPIC_SIZE];
};

struct subscription subscriptions[MAX_SUBSCRIPTIONS];
int num_subscriptions = 0;

// funciones auxiliares para que el codigo sea mas legible y limpio

// check se encarga de verificar si el resultado de una operacion es menor a 0, en caso de serlo, imprime un mensaje de error y sale del programa
// en caso contrario, retorna el resultado de la operacion
int check(int exp, const char *msg){
    if(exp < 0){
        perror(msg);
        exit(EXIT_FAILURE);
    }
    return exp;
}

// same_addr compara dos direcciones (ip y puerto)
int same_addr(struct sockaddr_in *a, struct sockaddr_in *b){
    return a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port;
}

// already_registered se encarga de verificar si un subscriber ya esta registrado en un tema especifico
int already_registered(struct sockaddr_in *addr, const char *topic){
    for(int i = 0; i < num_subscriptions; i++){
        if(same_addr(&subscriptions[i].addr, addr) && strcmp(subscriptions[i].topic, topic) == 0){
            return 1;
        }
    }
    return 0;
}

// handle_subscribe procesa un mensaje "SUBSCRIBE <tema>"
void handle_subscribe(struct sockaddr_in *sender_addr, const char *sender_ip, const char *topic){
    int port = ntohs(sender_addr->sin_port);

    if(topic[0] == '\0' || strlen(topic) >= TOPIC_SIZE || strchr(topic, ' ') != NULL){
        printf("[BROKER] Tema invalido de %s:%d, ignorando.\n", sender_ip, port);
        return;
    }

    if(already_registered(sender_addr, topic)){
        printf("[BROKER] %s:%d ya estaba suscrito a '%s'.\n", sender_ip, port, topic);
    } else if(num_subscriptions < MAX_SUBSCRIPTIONS){
        subscriptions[num_subscriptions].addr = *sender_addr;
        strcpy(subscriptions[num_subscriptions].topic, topic);
        num_subscriptions++;
        printf("[BROKER] Nuevo subscriber en '%s': %s:%d  (total suscripciones: %d)\n", topic, sender_ip, port, num_subscriptions);
    } else {
        printf("[BROKER] Lista de suscripciones llena, ignorando %s:%d\n", sender_ip, port);
    }
}

// handle_publish procesa un mensaje "PUBLISH <tema> <mensaje>" y lo reenvia
// unicamente a los subscribers que estan suscritos a ese tema
void handle_publish(int udp_socket, const char *sender_ip, int sender_port, char *payload){
    // separamos el tema del mensaje: el tema llega hasta el primer espacio
    char *space = strchr(payload, ' ');
    if(space == NULL){
        printf("[BROKER] PUBLISH mal formado de %s:%d (falta mensaje), ignorando.\n", sender_ip, sender_port);
        return;
    }
    *space = '\0';
    const char *topic = payload;
    const char *message = space + 1;

    if(topic[0] == '\0' || strlen(topic) >= TOPIC_SIZE){
        printf("[BROKER] Tema invalido de %s:%d, ignorando.\n", sender_ip, sender_port);
        return;
    }

    printf("[BROKER] Publicacion de %s:%d en '%s' -> \"%s\"\n", sender_ip, sender_port, topic, message);

    // armamos el mensaje que recibira el subscriber con el formato "[tema] mensaje"
    char out[BUFFER_SIZE];
    int out_len = snprintf(out, sizeof(out), "[%s] %s", topic, message);
    if(out_len >= (int)sizeof(out)) out_len = sizeof(out) - 1;

    //for que recorre la lista de suscripciones y envia el mensaje solo a las que coinciden con el tema usando sendto
    int delivered = 0;
    for(int i = 0; i < num_subscriptions; i++){
        if(strcmp(subscriptions[i].topic, topic) != 0) continue;

        int sent = sendto(udp_socket, out, out_len + 1, 0, (struct sockaddr*)&subscriptions[i].addr, sizeof(subscriptions[i].addr));
        if(sent < 0){
            perror("[BROKER] Error reenviando mensaje");
        } else {
            delivered++;
        }
    }
    printf("[BROKER] Mensaje de '%s' entregado a %d subscriber(s).\n", topic, delivered);
}

int main(int argc, char *argv[]){

    setbuf(stdout, NULL);

    if(argc != 2){
        fprintf(stderr, "Usage: %s <broker_port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int broker_port = atoi(argv[1]);


    // socket que sigue la misma logica que el del publisher pero que sera en este caso de uso para el broker 
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    check(udp_socket, "Error creando socket");

   
    struct sockaddr_in broker_addr = {.sin_family = AF_INET,.sin_addr.s_addr = INADDR_ANY,.sin_port = htons(broker_port)};
    // bind se encarga de asociar el socket a la direccion y puerto del broker, en este caso INADDR_ANY significa que el broker aceptara conexiones en cualquier interfaz de red
    // htons se encarga de convertir el puerto en orden de red
    check(bind(udp_socket, (struct sockaddr*)&broker_addr, sizeof(broker_addr)),"Error en bind");

    printf("Broker escuchando en puerto %d...\n", broker_port);

    while(1){
        char buffer[BUFFER_SIZE];
        struct sockaddr_in sender_addr;
        socklen_t sender_len = sizeof(sender_addr);

        // recvfrom se encarga de recibir un datagrama UDP y capturar la direccion del remitente, en este caso el buffer se encarga de almacenar el mensaje recibido
    
        int bytes = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,(struct sockaddr*)&sender_addr, &sender_len);
        check(bytes, "Error en recvfrom");
        buffer[bytes] = '\0';

        char sender_ip[INET_ADDRSTRLEN];
        //inet_ntop se encarga de convertir la direccion IP binaria del remitente a cadena de texto para mostrar quien envio mensajes y quien se suscribio
        inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, sizeof(sender_ip));
        int sender_port = ntohs(sender_addr.sin_port);

        // strncmp compara el inicio del buffer con los prefijos del protocolo:
        //   "SUBSCRIBE <tema>"          -> un subscriber se registra en un tema
        //   "PUBLISH <tema> <mensaje>"  -> un publisher envia un mensaje a un tema
        if(strncmp(buffer, SUBSCRIBE_PREFIX, strlen(SUBSCRIBE_PREFIX)) == 0){
            handle_subscribe(&sender_addr, sender_ip, buffer + strlen(SUBSCRIBE_PREFIX));
        } else if(strncmp(buffer, PUBLISH_PREFIX, strlen(PUBLISH_PREFIX)) == 0){
            handle_publish(udp_socket, sender_ip, sender_port, buffer + strlen(PUBLISH_PREFIX));
        } else {
            printf("[BROKER] Mensaje desconocido de %s:%d -> \"%s\", ignorando.\n", sender_ip, sender_port, buffer);
        }
    }

    // close cierra el socket y libera los recursos que estaba usando
    close(udp_socket);
    return EXIT_SUCCESS;
}
