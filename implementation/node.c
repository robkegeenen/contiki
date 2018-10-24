#include "net/rime/polite.h"
#include "contiki.h"
#include "dev/serial-line.h"
#include <stdio.h>
#include <stdlib.h>

PROCESS(example_polite_process, "");
PROCESS(test_serial, "Serial line test process");

//AUTOSTART_PROCESSES(&example_polite_process, &test_serial);
AUTOSTART_PROCESSES(&test_serial);

static void recv(struct polite_conn *c){
  printf("recv '%s'\n", (char*)packetbuf_dataptr());
}

static void sent(struct polite_conn *c){
  printf("sent\n");
}

static void dropped(struct polite_conn *c){
  printf("dropped\n");
}

static const struct polite_callbacks callbacks = {recv, sent, dropped};

PROCESS_THREAD(example_polite_process, ev, data){
  static struct polite_conn c;
  static unsigned int i;

  const size_t len = 28;
  char str[len];

  PROCESS_EXITHANDLER(polite_close(&c));

  PROCESS_BEGIN();

  polite_open(&c, 136, &callbacks);

  for(i = 0;; i++){
    static struct etimer et;
    etimer_set(&et, CLOCK_SECOND * 1);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    sprintf(str, "Integer 0x%04x from 0x%02x%02x!", i,
      linkaddr_node_addr.u8[1], linkaddr_node_addr.u8[0]);
    if(i >= 0xFFFF){
      i = 0;
    }

    packetbuf_copyfrom(str, len);
    polite_send(&c, CLOCK_SECOND * 1, len);
  }
  PROCESS_END();
}

PROCESS_THREAD(test_serial, ev, data){
  #define CMD_INVALID 0
  #define CMD_PRICE 1
  #define CMD_PICK 2

  const unsigned char argc = 3;
  unsigned char argi;
  unsigned int argv[argc];
  char *pch;

  PROCESS_BEGIN();

  for(;;){
    PROCESS_WAIT_EVENT_UNTIL(ev == serial_line_event_message);
    //Formats:
    //price <product type> <new price>
    //pick
    argi = 0;
    pch = strtok((char*)data, " ");
    argv[argi] = CMD_INVALID;
    if(strcmp(pch, "price") == 0){
      argv[argi] = CMD_PRICE;
    }
    if(strcmp(pch, "pick") == 0){
      argv[argi] = CMD_PICK;
    }
    argi++;
    pch = strtok(NULL, " ");
    while(pch && argi < argc){
      argv[argi] = atoi(pch);
      argi++;
      pch = strtok(NULL, " ");
    }

    if(argv[0] == CMD_PRICE){
      printf("Product type %u set to price %u!\n", argv[1], argv[2]);
    }
    if(argv[0] == CMD_PICK){
      printf("Pick event!\n");
    }
  }
  PROCESS_END();
}

