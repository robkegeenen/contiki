#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ftdi.h>
#include <usb.h>

#define BM_RESET 0b11000000
#define BM_PROG  0b11000100

int main(void){
  struct ftdi_context *ftdi;
  int f;

  ftdi = ftdi_new();
  if(!ftdi){
    fprintf(stderr, "Error allocating FTDI context\n");
    return EXIT_FAILURE;
  }

  f = ftdi_usb_open(ftdi, 0x0403, 0x6001);
  if((f < 0) && (f != -5)){
    fprintf(stderr, "Error opening FTDI device: %d (%s)\n", f, ftdi_get_error_string(ftdi));
    ftdi_deinit(ftdi);
    return EXIT_FAILURE;
  }

  f = ftdi_set_bitmode(ftdi, BM_RESET, BITMODE_CBUS);
  if(f < 0){
    fprintf(stderr, "Error forcing dongle in reset mode: %d (%s)\n", f, ftdi_get_error_string(ftdi));
    ftdi_usb_close(ftdi);
    ftdi_deinit(ftdi);
    return EXIT_FAILURE;
  }

  f = ftdi_set_bitmode(ftdi, BM_PROG, BITMODE_CBUS);
  if(f < 0){
    fprintf(stderr, "Error forcing dongle in program mode: %d (%s)\n", f, ftdi_get_error_string(ftdi));
    ftdi_usb_close(ftdi);
    ftdi_deinit(ftdi);
    return EXIT_FAILURE;
  }

  ftdi_disable_bitbang(ftdi);
  usb_reset(ftdi->usb_dev);
  ftdi_usb_close(ftdi);
  ftdi_free(ftdi);
  return EXIT_SUCCESS;
}
