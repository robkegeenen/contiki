#include "net/rime/polite.h"
#include "contiki.h"
#include "dev/serial-line.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "project-conf.h"

PROCESS(main_proc, "");
AUTOSTART_PROCESSES(&main_proc);

#define TYPE_SERVER -1
#define TYPE_INVALID -2
typedef struct{
  const char *name;
  uint32_t count;
  uint32_t seq;
  uint32_t ack_count;
}item_t;
item_t item_list[] = {
  {"banana", 1, 0, 0},
  {"apple", 0, 0, 0},
  {"pear", 1, 0, 0},
  {NULL, 0, 0, 0}
};
int32_t node_type = TYPE_INVALID;
uint16_t addr_u16 = 0;
uint32_t seq = 1;

#define CMD_PRICE 0
#define CMD_ACK 1
#define CMD_PICK 2

#define CHANNEL 136
#define SEND_TIME_MAX (CLOCK_SECOND / 2)

const size_t command_argc = 2;
typedef struct{
  uint32_t cmd;
  uint32_t seq;
  uint16_t addr;
  uint32_t argv[2];
}command_t;

static int32_t str2prod(const char *str){
  item_t *cur = item_list;
  while(cur->name){
    if(strcmp(cur->name, str) == 0){
      return(cur - item_list);
    }
    cur++;
  }
  return(TYPE_INVALID);
}

static void prompt(void){
  const char *str1, *str2;
  if(node_type == TYPE_SERVER){
    str1 = "Server";
    str2 = "";
  }
  else{
    str1 = "Product ";
    str2 = item_list[node_type].name;
  }
  printf("%s%s ready> ", str1, str2);
}

static uint32_t packet_prep(command_t *cmd){
  cmd->seq = seq++;
  cmd->addr = (uint32_t)addr_u16;
  packetbuf_copyfrom(cmd, sizeof(*cmd));
  return(seq);
}

static int packet_send(struct polite_conn *c, command_t *cmd){
  return(polite_send(c, SEND_TIME_MAX, sizeof(*cmd)));
}

static void recv(struct polite_conn *c){
  static uint32_t price_seq = 0;
  bool send_ack;
  char data[PACKETBUF_SIZE];
  packetbuf_copyto(data);
  command_t *orig = (command_t*)data;
  send_ack = FALSE;
  //TODO: Update ack count in list if orig->cmd == CMD_ACK (check seq)
  if((orig->cmd == CMD_PRICE) && (orig->argv[0] == node_type) && (orig->seq > price_seq)){
    printf("\nPrice set to %lu!\n", orig->argv[1]);
    prompt();
    send_ack = TRUE;
  }
  if(send_ack){
    command_t cmd;
    cmd.cmd = CMD_ACK;
    cmd.argv[0] = orig->seq;
    packet_send(c, &cmd);
  }
}

static void sent(struct polite_conn *c){
}

static void dropped(struct polite_conn *c){
}

static const struct polite_callbacks callbacks = {recv, sent, dropped};

PROCESS_THREAD(main_proc, ev, data){
  static struct polite_conn c;
  const uint8_t argc = 3;
  uint8_t argi;
  char *argv[argc], *pch;

  PROCESS_EXITHANDLER(polite_close(&c));
  PROCESS_BEGIN();

  addr_u16 = *(uint16_t*)(&linkaddr_node_addr);
  switch(addr_u16){
    case 0x23FA:
      node_type = TYPE_SERVER;
      break;
    case 0x33FA:
      node_type = str2prod("banana");
      break;
    case 0x0CFA:
      node_type = str2prod("pear");
      break;
    default:
      node_type = TYPE_INVALID;
      break;
  }
  polite_open(&c, CHANNEL, &callbacks);

  for(;;){
    if(node_type == TYPE_INVALID){
      static struct etimer et;
      etimer_set(&et, CLOCK_SECOND);
      for(;;){
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        printf("Settings invalid!\n");
        etimer_reset(&et);
      }
    }
    PROCESS_WAIT_EVENT_UNTIL(ev == serial_line_event_message);
    argi = 0;
    pch = strtok((char*)data, " ");
    while(pch && argi < argc){
      argv[argi] = pch;
      argi++;
      pch = strtok(NULL, " ");
    }

    //Formats:
    //price <product type> <new price>
    //pick
    if(argi > 0){
      if((strcmp(argv[0], "price") == 0) && (node_type == TYPE_SERVER)){
        if(argi == 3){
          command_t cmd;
          cmd.cmd = CMD_PRICE;
          cmd.argv[0] = str2prod(argv[1]);
          cmd.argv[1] = atoll(argv[2]);
          //Do error checking?
          item_list[cmd.argv[0]].ack_count = 0;
          item_list[cmd.argv[0]].seq = packet_prep(&cmd);
          packet_send(&c, &cmd);
        }
      }
      else if((strcmp(argv[0], "pick") == 0) && (node_type != TYPE_SERVER)){
        //
        printf("Pick event!\n");
        //zoek uit
      }
      else{
        printf("Invalid command!\n");
      }
    }
    prompt();
  }
  PROCESS_END();
}

