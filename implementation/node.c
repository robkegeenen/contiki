#include "net/rime/polite.h"
#include "contiki.h"
#include "dev/serial-line.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "project-conf.h"

PROCESS(main_proc, "");
AUTOSTART_PROCESSES(&main_proc);

#define TYPE_PROD     0
#define TYPE_SERVER  -1
#define TYPE_INVALID -2
typedef struct{
  const char *name;
  uint32_t item_count;
  uint32_t seq;
  uint32_t ack_count;
}item_t;
item_t item_list[] = {
  {"apple", 0, 0, 0},
  {"banana", 0, 0, 0},
  {"pear", 0, 0, 0},
  {NULL, 0, 0, 0}
};
int32_t node_type = TYPE_INVALID;
uint16_t node_addr = 0;
uint32_t seq = 1;
bool picked = FALSE;

#define CHANNEL 136
#define SEND_TIME_MAX (CLOCK_SECOND / 2)

#define CMD_PRICE 0
#define CMD_ACK 1
#define CMD_PICK 2

#define SCMD_ARGC 4
typedef struct{
  uint32_t cmd;
  uint32_t seq;
  uint16_t addr;
  uint32_t argv[SCMD_ARGC];
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
  cmd->seq = seq;
  cmd->addr = (uint32_t)node_addr;
  packetbuf_copyfrom(cmd, sizeof(*cmd));
  return(seq++);
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
  if((node_type == TYPE_SERVER) &&
     (orig->cmd == CMD_ACK) &&
     (orig->argv[1] == CMD_PRICE)){
    item_t *it = &item_list[orig->argv[3]];
    if(orig->argv[2] == it->seq){
      it->ack_count++;
    }
    if(it->ack_count >= it->item_count){
      it->item_count = it->ack_count;
      printf("\nAll %lu products of type %s have the latest price\n", it->item_count, it->name);
      prompt();
    }
  }
  if((node_type >= TYPE_PROD) &&
     (orig->cmd == CMD_ACK) &&
     (orig->argv[0] == (uint32_t)node_addr) &&
     (orig->argv[1] == CMD_PICK)){
    picked = TRUE;
    process_post(&main_proc, PROCESS_EVENT_CONTINUE, NULL);
    printf("\nReceived pick acknowledgement\n");
    prompt();
  }
  if((node_type >= TYPE_PROD) &&
     (orig->cmd == CMD_PRICE) &&
     (orig->argv[0] == node_type) &&
     (orig->seq > price_seq)){
    printf("\nPrice set to %lu\n", orig->argv[1]);
    prompt();
    send_ack = TRUE;
  }
  if((node_type == TYPE_SERVER) &&
     (orig->cmd == CMD_PICK)){
    item_t *it = &item_list[orig->argv[0]];
    if(it->item_count > 0){
      it->item_count--;
    }
    printf("\nProduct of type %s has been picked\n", it->name);
    prompt();
    send_ack = TRUE;
  }
  if(send_ack){
    command_t cmd;
    cmd.cmd = CMD_ACK;
    cmd.argv[0] = orig->addr;
    cmd.argv[1] = orig->cmd;
    cmd.argv[2] = orig->seq;
    cmd.argv[3] = node_type;
    packet_prep(&cmd);
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
  static struct etimer et;
  const uint8_t argc = 3;
  uint8_t argi;
  char *argv[argc], *pch;
  static char *msg;

  PROCESS_EXITHANDLER(polite_close(&c));
  PROCESS_BEGIN();

  node_addr = *(uint16_t*)(&linkaddr_node_addr);
  switch(node_addr){
    case 0x35DE:
      node_type = TYPE_SERVER;
      break;
    case 0xC8F9:
      node_type = str2prod("apple");
      break;
    //case 0xC?F9:
    //  node_type = str2prod("apple");
    //  break;
    case 0xEAF9:
      node_type = str2prod("apple");
      break;
    case 0xEEF9:
      node_type = str2prod("banana");
      break;
    case 0xF2F9:
      node_type = str2prod("banana");
      break;
    case 0xFEF9:
      node_type = str2prod("banana");
      break;
    case 0x0CFA:
      node_type = str2prod("pear");
      break;
    case 0x23FA:
      node_type = str2prod("pear");
      break;
    case 0x33FA:
      node_type = str2prod("pear");
      break;
    default:
      node_type = TYPE_INVALID;
      break;
  }
  polite_open(&c, CHANNEL, &callbacks);

  for(;;){
    if(node_type <= TYPE_INVALID){
      msg = "Invalid node settings";
      etimer_set(&et, CLOCK_SECOND);
    }
    else if(picked){
      msg = "Item has been picked";
      etimer_set(&et, CLOCK_SECOND);
    }
    else{
      msg = NULL;
    }
    if(msg){
      polite_close(&c);
      printf("\n");
      for(;;){
        printf("%s!\n", msg);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        etimer_reset(&et);
      }
    }
    PROCESS_WAIT_EVENT_UNTIL((ev == serial_line_event_message) || (ev == PROCESS_EVENT_CONTINUE));
    if(ev == PROCESS_EVENT_CONTINUE){
      continue;
    }
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
      if((node_type == TYPE_SERVER) &&
         (strcmp(argv[0], "price") == 0)){
        if(argi == 3){
          int32_t prod = str2prod(argv[1]);
          uint32_t price = atoll(argv[2]);
          if(prod < TYPE_PROD){
            printf("Invalid product!\n");
            continue;
          }
          printf("Setting price of products of type %s to %lu\n", item_list[prod].name, price);
          command_t cmd;
          cmd.cmd = CMD_PRICE;
          cmd.argv[0] = (uint32_t)prod;
          cmd.argv[1] = price;
          item_list[cmd.argv[0]].ack_count = 0;
          item_list[cmd.argv[0]].seq = packet_prep(&cmd);
          packet_send(&c, &cmd);
        }
      }
      else if((node_type >= TYPE_PROD) &&
              (strcmp(argv[0], "pick") == 0)){
        printf("Item has been picked\n");
        command_t cmd;
        cmd.cmd = CMD_PICK;
        cmd.argv[0] = (uint32_t)node_type;
        packet_prep(&cmd);
        packet_send(&c, &cmd);
      }
      else{
        printf("Invalid command!\n");
      }
    }
  prompt();
  }
  PROCESS_END();
}

