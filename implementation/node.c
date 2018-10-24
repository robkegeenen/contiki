#include "net/rime/polite.h"
#include "contiki.h"
#include <stdio.h>

PROCESS(example_polite_process, "");
AUTOSTART_PROCESSES(&example_polite_process);

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
