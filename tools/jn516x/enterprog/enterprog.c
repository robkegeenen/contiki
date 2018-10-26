#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ftdi.h>
#include <usb.h>

#define BM_RESET 0b11000000
#define BM_PROG  0b11000100
#define BM_RUN   0b11001100

int main(int argc, const char* argv[]){
  struct ftdi_context *ftdi_list, *ftdi;
  struct ftdi_device_list *devlist, *cur_dev;
  int i, res;
  unsigned char mode;
  char *mode_str;
  if((argc < 2) || (strcmp(argv[1], "run") != 0)){
    mode = BM_PROG;
    mode_str = "program";
  }
  else{
    mode = BM_RUN;
    mode_str = "run";
  }

  ftdi_list = ftdi_new();
  if(!ftdi_list){
    fprintf(stderr, "Error allocating FTDI context\n");
    return EXIT_FAILURE;
  }

  res = ftdi_usb_find_all(ftdi_list, &devlist, 0x0403, 0x6001);
  if(res < 0){
    fprintf(stderr, "Error finding FTDI devices\n");
    ftdi_free(ftdi_list);
    return EXIT_FAILURE;
  }

  cur_dev = devlist;
  i = 0;
  while(cur_dev){
    printf("Entering %s mode on device: %d\n", mode_str, i);

    ftdi = ftdi_new();
    if(!ftdi){
      fprintf(stderr, "Error allocating FTDI context\n");
      continue;
    }

    res = ftdi_usb_open_dev(ftdi, cur_dev->dev);
    if(res < 0){
      fprintf(stderr, "Error opening FTDI device: [%d] %d (%s)\n", i, res, ftdi_get_error_string(ftdi));
      ftdi_free(ftdi);
      continue;
    }

    res = ftdi_set_bitmode(ftdi, BM_RESET, BITMODE_CBUS);
    if(res < 0){
      fprintf(stderr, "Error forcing dongle in reset mode: [%d] %d (%s)\n", i, res, ftdi_get_error_string(ftdi));
      ftdi_usb_close(ftdi);
      ftdi_free(ftdi);
      continue;
    }

    res = ftdi_set_bitmode(ftdi, mode, BITMODE_CBUS);
    if(res < 0){
      fprintf(stderr, "Error forcing dongle in %s mode: [%d] %d (%s)\n", mode_str, i, res, ftdi_get_error_string(ftdi));
      ftdi_usb_close(ftdi);
      ftdi_free(ftdi);
      continue;
    }

    res = ftdi_disable_bitbang(ftdi);
    if(res < 0){
      fprintf(stderr, "Error disabling bitbang mode: [%d] %d (%s)\n", i, res, ftdi_get_error_string(ftdi));
      ftdi_usb_close(ftdi);
      ftdi_free(ftdi);
      continue;
    }

    res = usb_reset(ftdi->usb_dev);
    if(res < 0){
      fprintf(stderr, "Error resetting USB device: [%d]\n", i);
      ftdi_usb_close(ftdi);
      ftdi_free(ftdi);
      continue;
    }

    res = ftdi_usb_close(ftdi);
    if(res < -1){
      fprintf(stderr, "Error closing FTDI device: [%d] %d (%s)\n", i, res, ftdi_get_error_string(ftdi));
      ftdi_usb_close(ftdi);
      ftdi_free(ftdi);
      continue;
    }

    ftdi_free(ftdi);
    cur_dev = cur_dev->next;
    i++;
  }

  ftdi_list_free(&devlist);
  ftdi_free(ftdi_list);
  return EXIT_SUCCESS;
}
