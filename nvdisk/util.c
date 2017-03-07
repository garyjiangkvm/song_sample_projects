#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/version.h>
//#include <linux/dma-direction.h>
#include <linux/list.h>

#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_eh.h>

#include "nvdisk.h"

int find_1st_zero_bit(uint32 bits_32)
{
  uint32 bits = bits_32;
  int num = 0;
#if 0
  /* 64 bits */
  if ((bits&0xffffffff00000000) == 0xffffffff00000000){
     num += 32;
     bits = bits&0xffffffff;
  } else {
     bits = bits>>32;
  }
#endif
  /* 32 bits */
  if ((bits&0xffff0000) == 0xffff0000){
     num += 16;
     bits = bits&0xffff;
  } else {
     bits = (bits&0xffff0000)>>16;
  }

  /* 16 bits */
  if ((bits&0xff00) == 0xff00){
     num += 8;
     bits = bits&0xff;
  } else {
     bits = (bits&0xff00)>>8;
  }

  /* 8 bits */
  if ((bits&0xf0) == 0xf0){
     num += 4;
     bits = bits&0xf;
  } else {
     bits = (bits&0xf0)>>4;
  }

  /* 4 bits */
  if ((bits&0xc) == 0xc){
     num += 2;
     bits = bits&0x3;
  } else {
     bits = (bits&0xc)>>2;
  }
  
  /* 2 bits */
  if ((bits&0x2) == 0x2){
     num += 1;
     bits = bits&0x1;
  } else {
    bits = (bits&0x2)>>1;
  }

 
  /* 1 bits */
  if (bits == 0x1){
    num += 1;
  }

  return num;
}


void set_one_bit(uint32 *p_bits_32, int index)
{
    *p_bits_32 = (*p_bits_32)|(1<<(32-1-index));   
}

void clr_one_bit(uint32 *p_bits_32, int index)
{
    *p_bits_32 = (*p_bits_32)&~(1<<(32-1-index));   
}

#define DEF_BYTES_PER_LINE 16

static int bytes_per_line = DEF_BYTES_PER_LINE;

#define CHARS_PER_HEX_BYTE 3
#define BINARY_START_COL 6
#define MAX_LINE_LENGTH 257

void dStrHex(void* str, int len, long start, int noAddr)
{
    char* p = (char *)str;
    unsigned char c;
    char buff[MAX_LINE_LENGTH];
    long a = start;
    int bpstart, cpstart;
    int j, k, line_length, nl, cpos, bpos, midline_space;

    if (noAddr) {
        bpstart = 0;
        cpstart = ((CHARS_PER_HEX_BYTE * bytes_per_line) + 1) + 5;
    } else {
        bpstart = BINARY_START_COL;
        cpstart = BINARY_START_COL +
                        ((CHARS_PER_HEX_BYTE * bytes_per_line) + 1) + 5;
    }
    cpos = cpstart;
    bpos = bpstart;
    midline_space = ((bytes_per_line + 1) / 2);

    if (len <= 0)
        return;
    line_length = BINARY_START_COL +
                  (bytes_per_line * (1 + CHARS_PER_HEX_BYTE)) + 7;
    if (line_length >= MAX_LINE_LENGTH) {
        printk("bytes_per_line causes maximum line length of %d "
                        "to be exceeded\n", MAX_LINE_LENGTH);
        return;
    }
    memset(buff, ' ', line_length);
    buff[line_length] = '\0';
    if (0 == noAddr) {
        k = sprintf(buff + 1, "%.2lx", a);
        buff[k + 1] = ' ';
    }

    for(j = 0; j < len; j++) {
        nl = (0 == (j % bytes_per_line));
        if ((j > 0) && nl) {
            printk("%s\n", buff);
            bpos = bpstart;
            cpos = cpstart;
            a += bytes_per_line;
            memset(buff,' ', line_length);
            if (0 == noAddr) {
                k = sprintf(buff + 1, "%.2lx", a);
                buff[k + 1] = ' ';
            }
        }
        c = *p++;
        bpos += (nl && noAddr) ?  0 : CHARS_PER_HEX_BYTE;
        if ((bytes_per_line > 4) && ((j % bytes_per_line) == midline_space))
            bpos++;
        sprintf(&buff[bpos], "%.2x", (int)(unsigned char)c);
        buff[bpos + 2] = ' ';
        if ((c < ' ') || (c >= 0x7f))
            c='.';
        buff[cpos++] = c;
    }
    if (cpos > cpstart)
        printk("%s\n", buff);
}

