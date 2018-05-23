/* main.c ---
*
* Filename: main.c
* Description:
* Author:
* Maintainer:
* Created: Tue Apr 17 15:08:43 2018
/* Code: */

#include <stm32f30x.h> // Pull in include files for F30x standard drivers
#include <f3d_led.h>
#include <f3d_uart.h>
#include <f3d_user_btn.h>
#include <f3d_lcd_sd.h>
#include <f3d_i2c.h>
#include <f3d_nunchuk.h>
#include <f3d_rtc.h>
#include <f3d_systick.h>
#include <stdio.h>
#include <ff.h>
#include <diskio.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aster3.h"
#include "aster4.h"
#include "aster5.h"


#define TIMER 20000
#define AUDIOBUFSIZE 128

extern uint8_t Audiobuf[AUDIOBUFSIZE];
extern int audioplayerHalf;
extern int audioplayerWhole;

FATFS Fatfs;		/* File system object */
FIL fid;		/* File object */
BYTE Buff[512];		/* File read buffer */
int ret;

struct ckhd {
  uint32_t ckID;
  uint32_t cksize;
};

struct fmtck {
  uint16_t wFormatTag;      
  uint16_t nChannels;
  uint32_t nSamplesPerSec;
  uint32_t nAvgBytesPerSec;
  uint16_t nBlockAlign;
  uint16_t wBitsPerSample;
};

void readckhd(FIL *fid, struct ckhd *hd, uint32_t ckID) {
  f_read(fid, hd, sizeof(struct ckhd), &ret);
  if (ret != sizeof(struct ckhd))
    exit(-1);
  if (ckID && (ckID != hd->ckID))
    exit(-1);
}

void die (FRESULT rc) {
  printf("Failed with rc=%u.\n", rc);
  while (1);
}

int play_wav(char *file_name) {
  FRESULT rc;			/* Result code */
  DIR dir;			/* Directory object */
  FILINFO fno;			/* File information object */
  UINT bw, br;
  unsigned int retval;
  int bytesread;


  printf("Reset\n");
  
  f_mount(0, &Fatfs);/* Register volume work area */

  printf("\nOpened %s\n",file_name);
  rc = f_open(&fid, file_name, FA_READ);
  printf("\nfile opened\n");
	
   if (!rc) {
    struct ckhd hd;
    uint32_t  waveid;
    struct fmtck fck;
    
    readckhd(&fid, &hd, 'FFIR');
    
    f_read(&fid, &waveid, sizeof(waveid), &ret);
    if ((ret != sizeof(waveid)) || (waveid != 'EVAW'))
      return -1;
    
    readckhd(&fid, &hd, ' tmf');
    
    f_read(&fid, &fck, sizeof(fck), &ret);
    
    // skip over extra info
    
    if (hd.cksize != 16) {
      printf("extra header info %d\n", hd.cksize - 16);
      f_lseek(&fid, hd.cksize - 16);
    }
    
    printf("audio format 0x%x\n", fck.wFormatTag);
    printf("channels %d\n", fck.nChannels);
    printf("sample rate %d\n", fck.nSamplesPerSec);
    printf("data rate %d\n", fck.nAvgBytesPerSec);
    printf("block alignment %d\n", fck.nBlockAlign);
    printf("bits per sample %d\n", fck.wBitsPerSample);
    
    // now skip all non-data chunks !
    
    while(1){
      readckhd(&fid, &hd, 0);
      if (hd.ckID == 'atad')
	break;
      f_lseek(&fid, hd.cksize);
    }
    
    printf("Samples %d\n", hd.cksize);
    
    // Play it !
    
    //      audioplayerInit(fck.nSamplesPerSec);
    
    f_read(&fid, Audiobuf, AUDIOBUFSIZE, &ret);
    hd.cksize -= ret;
    audioplayerStart();
    while (hd.cksize) {
      int next = hd.cksize > AUDIOBUFSIZE/2 ? AUDIOBUFSIZE/2 : hd.cksize;
      if (audioplayerHalf) {
	if (next < AUDIOBUFSIZE/2)
	  bzero(Audiobuf, AUDIOBUFSIZE/2);
	f_read(&fid, Audiobuf, next, &ret);
	hd.cksize -= ret;
	audioplayerHalf = 0;
      }
      if (audioplayerWhole) {
	if (next < AUDIOBUFSIZE/2)
	  bzero(&Audiobuf[AUDIOBUFSIZE/2], AUDIOBUFSIZE/2);
	f_read(&fid, &Audiobuf[AUDIOBUFSIZE/2], next, &ret);
	hd.cksize -= ret;
	audioplayerWhole = 0;
      }
    }
    audioplayerStop();
  }
  
  printf("\nClose the file.\n");
  rc = f_close(&fid);
  
  if (rc) die(rc);

}



int drawAster(int xx, int yy, int size);
uint16_t convert(char* data, uint8_t pixel[]);


int main(void) {
  
  
  f3d_uart_init();
  delay(10);
  f3d_timer2_init();
  delay(10);
  f3d_dac_init();
  delay(10);
  f3d_delay_init();
  delay(10);
  f3d_user_btn_init();
  delay(10);
  f3d_rtc_init();
  delay(10);
  f3d_systick_init();
  delay(10);
  f3d_lcd_init();
  delay(10);
  f3d_i2c1_init();
  delay(10);
  f3d_nunchuk_init();
  delay(10);
  f3d_led_init();
  delay(10);
  
  
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  // Set up your inits before you go ahead
  
  f3d_lcd_fillScreen(BLACK);

  float px=20, py=20, vx=0, vy=0, power=0.02, accel=0.01;
  
  int pad[2] = {100, 20};
  
  float f[3];
  
  char str[20];
  char lvls[20];
  
  int lives = 3;
  int lvl = 1;//, seed;
  char fx[10], fy[10], fz[10];

  float heading = 90;
  
  int success = 0;
  int level = 1;
  
  nunchuk_t ndata;
  
  
  while (1) {
    if (lives == 0 ){
		f3d_lcd_fillScreen(RED);
		f3d_lcd_drawString(40, 50, "YOU LOST", BLACK, RED);
		f3d_lcd_drawString(70, 80, lvls, BLACK, RED);
        //play_wav("g.wav");
        delay(500);
		break;
     
	}
    
    if (lvl == 4)  {
		f3d_lcd_fillScreen(YELLOW);
		f3d_lcd_drawString(40, 60, "YOU WIN", RED, YELLOW);
		//play_wav("o.wav");
		f3d_led_all_on();
		delay(600);
		f3d_led_all_off();
		delay(600);
		f3d_led_all_on();
		delay(600);
		f3d_led_all_off();
		delay(600);
		f3d_led_all_on();
		delay(600);
		f3d_led_all_off();
		delay(600);
		break;
	}
	
	
	sprintf(lvls, "Level:%d", lvl);
    sprintf(str, "Live(s) remain:%d", lives);
    f3d_lcd_fillScreen(BLACK);
    success=0;


    

    
	while (1) {
      f3d_nunchuk_read(&ndata);
      //seed = (int)((ndata.ax+ndata.ay)/2);
      f3d_lcd_drawString(40, 60, lvls, WHITE, BLACK);
      f3d_lcd_drawString(20, 90, str, WHITE, BLACK);
      
      if (ndata.c) break;
    
    }
    
    switch(lvl){
		
		case 1: 
			accel=0.01;
			power=0.02;
			pad[1] = 20;
			f3d_lcd_fillScreen(BLACK);
	
			f3d_lcd_drawPad(pad[0], pad[1], RED);
    
			drawAster(30, 50, 40);
			drawAster(70, 40, 50);
			drawAster(20, 120, 30);
			break;
		
		case 2: 
			accel=0.02;
			power=0.04;
			pad[1] = 12;
			f3d_lcd_fillScreen(BLACK);
	
			f3d_lcd_drawPad(pad[0], pad[1], RED);
    
			drawAster(30, 50, 40);
			drawAster(70, 40, 50);
			drawAster(20, 120, 30);
			break;
			
		case 3: 
			accel=0.04;
			power=0.08;
			pad[1] = 6;
			f3d_lcd_fillScreen(BLACK);
	
			f3d_lcd_drawPad(pad[0], pad[1], RED);
    
			drawAster(30, 50, 40);
			drawAster(70, 40, 50);
			drawAster(20, 120, 30);
			break;
		}
    
  //==============================
  while (1) {

    f3d_nunchuk_read(&ndata);
    
    f3d_lcd_clearRoc(px, py, BLACK);
    
    if (pow(px-50, 2)+pow(py-70, 2)<460) break;
    if (pow(px-95, 2)+pow(py-65, 2)<770) break;
    if (pow(px-35, 2)+pow(py-135, 2)<270) break;
    
    if (py > 155) {
      if (px > pad[0]-pad[1] && px < pad[0]+pad[1] && vy < 0.5 && (int)heading%360 > 74 && (int)heading%360 < 106) success = 1;
      break;
    }
    
    if(ndata.jx > 200) {
      heading-=5;
    } else if (ndata.jx < 50) {
      heading+=5;
    }
	
    px+=vx;
    vy+=accel;
    py+=vy;
	
    f3d_lcd_drawRoc(px, py, WHITE);
    f3d_lcd_drawDot(px+8*cosf(heading*M_PI/180), py-8*sinf(heading*M_PI/180), WHITE);
    f3d_lcd_drawDot(px+6*cosf(heading*M_PI/180), py-6*sinf(heading*M_PI/180), WHITE);
    f3d_lcd_drawDot(px+4*cosf(heading*M_PI/180), py-4*sinf(heading*M_PI/180), WHITE);
    f3d_lcd_drawDot(px+4*cosf((heading+120)*M_PI/180), py-4*sinf((heading+120)*M_PI/180), WHITE);
    f3d_lcd_drawDot(px+4*cosf((heading-120)*M_PI/180), py-4*sinf((heading-120)*M_PI/180), WHITE);
    
    
    
    
    if(ndata.z) {
      vx+=power*cosf(heading*M_PI/180);
      vy-=power*sinf(heading*M_PI/180);
      f3d_lcd_drawDot(px+3*cosf((heading-180)*M_PI/180), py-3*sinf((heading-180)*M_PI/180), RED);
      f3d_lcd_drawDot(px+6*cosf((heading-180)*M_PI/180), py-6*sinf((heading-180)*M_PI/180), YELLOW);
      //play_wav("c.wav");
    }
    
    printf("heading: %f, v: %f\n", heading, cosf(heading*M_PI/180)+sinf(heading*M_PI/180));
    
    
    
  }
  //==============================
  
  if (success) {
	f3d_lcd_fillScreen(YELLOW);
	lvl++;
	px=20, py=20, vx=0, vy=0;
    delay(500);
  } else {
    f3d_lcd_fillScreen(RED);
    lives--;
    px=20, py=20, vx=0, vy=0;
    delay(500);
  }
  
  
  }
  
}


int drawAster(int xx, int yy, int size) {
  char* aster;
  if (size == 30) aster = aster3;
  if (size == 40) aster = aster4;
  if (size == 50) aster = aster5;
  
  uint8_t pixel[3];
  int x, y;
  for (y = 0; y < size; y++) {
    for (x = 0; x < size; x++){
      uint16_t color = convert(aster + (x*4) + (y*4*size), pixel);
      f3d_lcd_setAddrWindow(x+xx,y+yy,x+xx+1,y+yy+1,0x6);
      f3d_lcd_pushColor(&color,1);
    }
  }
}

uint16_t convert(char* data, uint8_t pixel[]) {
  HEADER_PIXEL(data, pixel);
  uint16_t color = ((pixel[2] >> 3) << 11) | ((pixel[1] >> 2) << 5) | (pixel[1] >> 3);
  return color;
  
}



#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line) {
/* Infinite loop */
/* Use GDB to find out why we're here */
  while (1);
}
#endif

/* main.c ends here */
