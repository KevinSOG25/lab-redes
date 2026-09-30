#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE     1024
#define MAX_SUBSCRIBERS 10
#define SUBSCRIBE_MSG   "SUBSCRIBE"

struct sockaddr_in subscribers[MAX_SUBSCRIBERS];
int num_subscribers = 0;

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

// already_registered se encarga de verificar si un subscriber ya esta registrado
int already_registered(struct sockaddr_in *addr){
    for(int i = 0; i < num_subscribers; i++){
        if(subscribers[i].sin_addr.s_addr == addr->sin_addr.s_addr && subscribers[i].sin_port == addr->sin_port){
            return 1;
        }
    }
    return 0;
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

        // recvfrom se encarga de recibir un datagrama UDP y capturar la direccion del remitente, en este caso el buffer se encarga de almacenar el mensaje recibido que envio el publisher
    
        int bytes = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,(struct sockaddr*)&sender_addr, &sender_len);
        check(bytes, "Error en recvfrom");
        buffer[bytes] = '\0';

        char sender_ip[INET_ADDRSTRLEN];
        //inet_ntop se encarga de convertir la direccion IP binaria del remitente a cadena de texto para mostrar quien envio mensajes y quien se suscribio
        inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, sizeof(sender_ip));
        
        //strcmp se encarga de comparar el buffer con SUBSCRIBE_MSG
        // en caso de ser iguales, significa que el usuario se suscribio
        // already_registered se encarga de verificar si un subscriber ya esta registrado
        // en caso de que no este registrado y haya espacio, se agrega a la lista de subscribers
        // en caso de que no haya espacio, se ignora el mensaje
        // en caso de que ya este registrado, se imprime un mensaje
        
        if(strcmp(buffer, SUBSCRIBE_MSG) == 0){
            if(already_registered(&sender_addr)){
                printf("[BROKER] %s:%d ya estaba suscrito.\n",sender_ip, ntohs(sender_addr.sin_port));
            } else if(num_subscribers < MAX_SUBSCRIBERS){
                subscribers[num_subscribers++] = sender_addr;
                printf("[BROKER] Nuevo subscriber: %s:%d  (total: %d)\n",sender_ip, ntohs(sender_addr.sin_port), num_subscribers);
            } else {
                printf("[BROKER] Lista de subscribers llena, ignorando %s:%d\n",sender_ip, ntohs(sender_addr.sin_port));
            }
            continue;
        }

        printf("[BROKER] Mensaje de %s:%d -> \"%s\"\n",sender_ip, ntohs(sender_addr.sin_port), buffer);


        //for que recorre la lista de subscribers y envia el mensaje a cada uno usando la funcion de sendto que ya vimos anteriormente
        for(int i = 0; i < num_subscribers; i++){
            // sendto envia el mensaje recibido del publisher a cada subscriber que esta registrado en la lista
            int sent = sendto(udp_socket, buffer, bytes, 0,(struct sockaddr*)&subscribers[i], sizeof(subscribers[i]));
            if(sent < 0){
                perror("[BROKER] Error reenviando mensaje");
            }
        }
    }

    // close cierra el socket y libera los recursos que estaba usando
    close(udp_socket);
    return EXIT_SUCCESS;
}
