
# Publish/Subscribe por temas sobre UDP

Sistema con tres programas en C que se ejecutan cada uno en su propia terminal:

- **broker**: recibe suscripciones y publicaciones, y reenvía cada mensaje solo a los subscribers de ese tema.
- **publisher**: envía un mensaje a un tema.
- **subscriber**: se suscribe a uno o más temas y muestra los mensajes que le llegan.

## Protocolo

| Quién               | Mensaje UDP                  |
| -------------------- | ---------------------------- |
| Subscriber → Broker | `SUBSCRIBE <tema>`         |
| Publisher → Broker  | `PUBLISH <tema> <mensaje>` |
| Broker → Subscriber | `[tema] mensaje`           |

> El tema no puede tener espacios.

## Compilar (en la máquina virtual)

```bash
make
```

o sin `make`:

```bash
gcc -Wall -o broker broker_udp.c
gcc -Wall -o publisher publisher_udp.c
gcc -Wall -o subscriber subscriber_udp.c
```

## Ejecutar (una terminal por proceso)

**Terminal 1 – broker**

```bash
./broker 5000
```

**Terminal 2 – subscriber de "deportes"**

```bash
./subscriber 6001 127.0.0.1 5000 deportes
```

**Terminal 3 – subscriber de "clima"**

```bash
./subscriber 6002 127.0.0.1 5000 clima
```

**Terminal 4 – subscriber de ambos temas**

```bash
./subscriber 6003 127.0.0.1 5000 deportes clima
```

**Terminal 5 – publisher**

```bash
./publisher 127.0.0.1 5000 deportes "Gol de Colombia"
./publisher 127.0.0.1 5000 clima "Lluvia en la tarde"
```

Resultado esperado:

- Terminal 2 recibe solo `[deportes] Gol de Colombia`
- Terminal 3 recibe solo `[clima] Lluvia en la tarde`
- Terminal 4 recibe ambos

> Cada subscriber debe usar un puerto distinto si están en la misma máquina.
> Si los procesos están en máquinas virtuales distintas, cambia `127.0.0.1` por la IP de la VM del broker (`ip a` para verla).

Para detener cualquier proceso: `Ctrl + C`.

## Análisis en Wireshark (todo en la misma VM)

### Interfaz a capturar

Captura en la interfaz **`lo` (Loopback)**, no en `eth0`/`enp0s3`.
En Linux, el tráfico entre procesos de la misma máquina nunca sale por la tarjeta de red (aunque se use la IP real de la VM): siempre pasa por `lo`.

Filtro de visualización:

```
udp.port == 5000
```

### Orden recomendado

1. Abrir Wireshark en `lo` e **iniciar la captura** (`sudo wireshark`).
2. Iniciar el broker.
3. Iniciar los subscribers (así se capturan los `SUBSCRIBE`).
4. Ejecutar los publishers.
5. Detener la captura.

### Captura solo por terminal (tshark)

```bash
sudo tshark -i lo -f "udp port 5000" -w captura.pcap   # capturar (Ctrl + C para detener)
sudo tshark -r captura.pcap                            # ver resumen
sudo tshark -r captura.pcap -x                         # ver contenido en hexadecimal
```

El archivo `captura.pcap` se puede abrir luego en Wireshark.

### Cómo identificar cada proceso

En `lo` todos los paquetes tienen IP `127.0.0.1`, así que se distinguen por **puerto**:

| Puerto           | Proceso                                   |
| ---------------- | ----------------------------------------- |
| 5000             | broker                                    |
| 6001, 6002, 6003 | subscribers                               |
| efímero (≥32768) | publisher (cambia en cada ejecución)      |

### Paquetes esperados

| Paquete     | Origen → Destino               | Contenido                          |
| ----------- | ------------------------------ | ---------------------------------- |
| Suscripción | subscriber:600X → broker:5000  | `SUBSCRIBE deportes`               |
| Publicación | publisher:efímero → broker:5000 | `PUBLISH deportes Gol de Colombia` |
| Reenvío     | broker:5000 → subscriber:600X  | `[deportes] Gol de Colombia`       |

El broker envía un datagrama de reenvío **por cada subscriber** suscrito al tema.

### Vistas útiles

- **Statistics → Flow Graph** (marcar *Limit to display filter*): diagrama de flechas entre puertos.
- **Statistics → Conversations → UDP**: pares de puertos que se comunicaron.
- Panel de bytes: muestra el texto del mensaje y el byte `00` final.
- Filtros por contenido: `udp contains "SUBSCRIBE"`, `udp contains "PUBLISH"`.

### Características de UDP observables

- **Sin conexión**: no hay handshake (SYN/SYN-ACK/ACK) ni cierre (FIN); el primer paquete ya lleva datos.
- **Sin ACK ni retransmisión**: si un datagrama se pierde, nadie lo reenvía (por eso el broker debe iniciarse antes que los subscribers).
- **Cabecera de 8 bytes**: puerto origen, puerto destino, longitud y checksum (TCP usa ≥20 bytes).
- **Un `sendto` = un datagrama**: sin segmentación ni números de secuencia.
- **Byte `00` al final**: el código envía `strlen + 1`, así que el terminador nulo viaja en el paquete.
- **Puerto efímero del publisher**: no hace `bind`, el sistema le asigna uno distinto en cada ejecución.
- **Texto plano**: el contenido es legible directamente en la captura.
