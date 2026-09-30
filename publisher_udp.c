#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// codigo hecho usando como guia https://www.youtube.com/watch?v=5PPfy-nUWIM (How to Send and Receive UDP packets (in C))

int main(int argc, char *argv[]){

    if(argc != 4){
        fprintf(stderr, "Usage: %s <peer_ip> <peer_port> <message>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char * peer_ip = argv[1];
    const char * peer_port_str = argv[2];
    const char * message = argv[3];


    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM};
    struct addrinfo *res;

    //getaddrinfo resuelve la direccion del broker a una estructura para usar luego el sendto
    if(getaddrinfo(peer_ip, peer_port_str, &hints, &res) != 0){
        fprintf(stderr, "Error resolviendo host '%s'\n", peer_ip);
        return EXIT_FAILURE;
    }


    // sokcet lo que hace como tal es que encargarse de crearlo utilizando en este caso como parametros las direcciones, el tipo que en este caso son datagramas
    // dado que trabajamos con udp y el protocolo por defecto
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if(udp_socket < 0){
        perror("Algo anda mal con el socket");
        // freeaddrinfo libera la memoria reservada por getaddrinfo
        freeaddrinfo(res);
        return EXIT_FAILURE;
    }


    // sendto se encarga de enviar el mensaje al broker en un datagrama, destacando el envio utilizando las diferentes variables que se 
    // utilizaron anteriormente para poder realizar el programa. En este caso la variable message se encarga de almacenar el mensaje que queremos enviar
    // strlen(message) + 1 se encarga de enviar el mensaje con el tamano adecuado. Ademas de tener el socket y la direccion del broker para el envio.

    if(sendto(udp_socket, message, strlen(message) + 1, 0, res->ai_addr, res->ai_addrlen) < 0){
        perror("Algo anda mal con el envio");
        freeaddrinfo(res);
        close(udp_socket);
        return EXIT_FAILURE;
    }
    freeaddrinfo(res);

    // close se encarga de cerrar el socket y es necesario que lo hagamos 
    close(udp_socket);
    return EXIT_SUCCESS;
}
