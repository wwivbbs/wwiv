#include "fossil.h"

#include "pipe.h"
#include "util.h"
#include <conio.h>
#include <ctype.h>
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <malloc.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#pragma warning(disable : 4505)

#define LOBYTE(w) ((w) & 0xff)
#define HIBYTE(w) (((w) >> 8) & 0xff)

#define STATUS_BASE           0x0008
#define STATUS_CARRIER_DETECT 0x0080
#define STATUS_INPUT_AVAIL    0x0100
#define STATUS_INPUT_OVERRUN  0x0200
#define STATUS_OUTPUT_AVAIL   0x2000
#define STATUS_OUTPUT_EMPTY   0x4000

#define FOSSIL_HIGHEST_FUNCTION 0x1B
#define FOSSIL_SIGNATURE 0x1954

void (__interrupt __far *old_int14)();

static int fossil_enabled = 0;
static int carrier = 1;
static int pipe_handle = -1;
static int log_count_ = 0;
static int num_calls = 0;
static int fos_calls[32];
static int in_int14 = 0;
static char m[20];
static Pipe __far * pipe = NULL;
static int char_avail = 0;

char __far * fossil_info_ident = "WWIV OS/2 FOSSIL Driver";

#pragma pack(1)
struct fossil_info {
   unsigned int size;
   unsigned char majver;
   unsigned char minver;
   char __far *  ident;
   unsigned int  in_buffer_size;
   unsigned int  in_bytes_avail;
   unsigned int  out_buffer_size;
   unsigned int  out_buffer_avail;
   unsigned char width;
   unsigned char height;
   unsigned char baudmask;
};
#pragma pack()

static struct fossil_info info;

static unsigned status() {
  unsigned r = STATUS_BASE;
  if (!pipe->is_open() || !carrier) {
    carrier = 0;
    return r;
  }

  r |= STATUS_CARRIER_DETECT;
  if (char_avail <= 0) {
    char_avail = pipe->peek();
  }
  if (char_avail > 0) {
    r |= STATUS_INPUT_AVAIL;
  }
  // HACK: Claim were were done writing
  r |= STATUS_OUTPUT_AVAIL;
  r |= STATUS_OUTPUT_EMPTY;
  return r;
}

#pragma check_stack-
#pragma warning(disable : 4100)

void __interrupt __far int14_handler( unsigned _es, unsigned _ds,
	  unsigned _di, unsigned _si,
	  unsigned _bp, unsigned _sp,
	  unsigned _bx, unsigned _dx,
	  unsigned _cx, unsigned _ax,
	  unsigned _ip, unsigned _cs,
	  unsigned _flags ) { 

  if (in_int14) {
    return;
  }
  in_int14 = 1;
  
  int func = (int) HIBYTE(_ax);
  if (func < 32 && func >= 0) {
    ++fos_calls[func];
  }
  num_calls++;
  switch (func) {
  case 0x0: {
    // Set baud rate.  Nothing to set since we don't care about BPS
    _ax = status();
  } break;
  case 0x01: {
    /*
      AH = 01h    Transmit character with wait

            Parameters:
                Entry:  AL = Character
                        DX = Port number
                Exit:   AX = Port status (see function 03h)

     */
    unsigned char ch = (unsigned char) LOBYTE(_ax);
    //pipe->write("0x01: ", 6);
    ++log_count_;
    pipe->write(ch);
    _ax = status();
  } break;
  case 0x02: {
    //pipe->write("0x02: ", 6);
    // Receive characer with wait
    char_avail = 0;
    _ax = pipe->blocking_read();
  } break;
  case 0x03: {
    // pipe->write("0x03: ", 6);
    // Request status. 
    // TODO should we mask it?
    int cc = pipe->control_code();
    if (cc == 'D') {
      carrier = 0;
    }
    _ax = status();
  } break;
  case 0x04: {
    // AH = 04h    Initialize driver
    // TODO should we mask it?
    _ax = FOSSIL_SIGNATURE;
    _bx = 0x1000 | FOSSIL_HIGHEST_FUNCTION;
    //pipe->write("0x04: ", 6);
  } break;
  case 0x0B: {
    /*
      AH = 0Bh    Transmit no wait

            Parameters:
                Entry:  DX = Port number
                Exit:   AX = 0001h - Character was accepted
                           = 0000h - Character was not accepted

     */
    // Transmit character with wait (or no wait for 0b)
    unsigned char ch = (unsigned char) LOBYTE(_ax);
    ++log_count_;
    pipe->write(ch);
    _ax = 1;
    // See if this makes gwar work.
    os_yield();
  } break;
  case 0x0C: {
    // com peek
    int ch = pipe->peek();
    _ax = (ch == -1) ? 0xffff : ch;
  } break;
  case 0x0D: {
    // read keyboard without wait
  } break;
  case 0x0E: {
    // read keyboard with wait
  } break;
  case 0x19: {
    // block write
    /*
           Parameters:
               Entry:  CX = Maximum number of characters to transfer
                       DX = Port number
                       ES = Segment of user buffer
                       DI = Offset into ES of user buffer
               Exit:   AX = Number of characters actually transferred  */
    char __far * buf = (char __far *) MK_FP(_es, _di);
    _ax = pipe->write(buf, _cx);
  } break;
  case 0x1B: {
    /*
     AH = 1Bh    Return information about the driver

           Parameters:
               Entry:  CX = Size of user info buffer in bytes
                       DX = Port number
                       ES = Segment of user info buffer
                       DI = Offset into ES of user info buffer
               Exit:   AX = Number of bytes actually transferred
     */
    // Request status. 
    // TODO should we mask it?
    unsigned int siz = sizeof(info);
    if (siz < _cx) {
      siz = _cx;
    }
    info.size = sizeof(info);
    info.majver = 10;
    info.minver = 1;
    info.ident = fossil_info_ident;
    info.in_buffer_size = 100;
    info.in_bytes_avail = 100;
    info.out_buffer_size = 100;
    info.out_buffer_avail = 100;
    info.width = 80;
    info.height = 25;
    info.baudmask = 0x23; // 38400 N81
    void __far * p = MK_FP(_es, _di);
    _fmemcpy(p, &info, siz);
    _ax = siz;
  } break;
  }

  in_int14 = 0;
}

void __interrupt __far int14_sig() { 
  char pad[10] = {0};
}

void enable_fossil(int nodenum, int comport) {
  old_int14 = _dos_getvect(0x14);

  for (int i=0; i<32; i++) {
    fos_calls[i]=0;
  }

  _disable();
  unsigned char __far * p = (unsigned char __far*) ((void __far*) int14_sig);
  void __far * sig_addr = int14_sig;
  void __far * handler_addr = int14_handler;
  // We offset by 3 since JMP is one, and then two for the address for a near JMP
  int diff = FP_OFF(handler_addr) - FP_OFF(sig_addr) - 3;

  *p = 0xE9;
  *(p+1) = (unsigned char)(diff & 0xff);
  *(p+2) = (unsigned char)(((diff & 0xff00) >> 8) & 0xff);
  *(p+3) = 0x90;
  *(p+4) = 0x90;
  *(p+5) = 0x90;
  // 0x1954 (signature)
  *(p+6) = LOBYTE(FOSSIL_SIGNATURE);
  *(p+7) = HIBYTE(FOSSIL_SIGNATURE);
  // Highest supported FOSSIL function.
  *(p+8) = FOSSIL_HIGHEST_FUNCTION;
  _dos_setvect(0x14, (void (__interrupt __far *)()) int14_sig);
  _enable();

  log("sig_addr=%p:%p, handler_addr=%p:%p diff=%d/%x", FP_SEG(sig_addr), FP_OFF(sig_addr), 
      FP_SEG(handler_addr), FP_OFF(handler_addr), diff, diff);
  int __far *ip = (int __far*)(p+1); // +1 is where the near address for JMP is
  log("p:%d/%x", *ip, *ip);
  fossil_enabled = 1;

  char pipename[200];
  sprintf(pipename, "\\PIPE\\WWIV%d", nodenum);
  pipe = new __far Pipe(pipename);
  if (!pipe->is_open()) {
    log("Unable to open FOSSIL log: wwivfoss.log");
  } else {
    log("Pipe Opened");
  }
  // This seems to make it work consistently with the test app.
  os_yield();
  // hack:
  pipe_handle = pipe->handle();
  log("FOSSIL Enabled. Pipe [handle:%d]", pipe_handle);
}

void disable_fossil() {
  if (!fossil_enabled) {
    log("ERROR: disable_fossil called when not enabled.");
    return;
  }

  _disable();
  _dos_setvect(0x14, old_int14);
  _enable();

  log("FOSSIL Disabled: [%d calls][handle: %d]", num_calls, pipe->handle());

  for (int i=0; i<32; i++) {
    if (fos_calls[i]) {
      log("Fossil Call: 0x%02X: %d times", i, fos_calls[i]);
    }
  }
  log("Pipe: [writes: %d][errors: %d][bytes: %ld]", pipe->num_writes(),
      pipe->num_errors(), pipe->bytes_written());

  log("Closed pipe");
  delete pipe;
  pipe = NULL;
}

