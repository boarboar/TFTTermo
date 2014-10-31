/*
 2012 Copyright (c) Seeed Technology Inc.

 Authors: Albert.Miao & Loovee, 
 Visweswara R (with initializtion code from TFT vendor)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

 ILI9341
 
*/
#ifndef TFTv2_h
#define TFTv2_h

#if defined(ARDUINO) && ARDUINO >= 100
#define SEEEDUINO
#include <Arduino.h>
#else
#include <WProgram.h>
#endif
//#include <avr/pgmspace.h>

#include <SPI.h>

//Basic Colors
#define RED		0xf800
#define GREEN	0x07e0
#define BLUE	0x001f
#define BLACK	0x0000
#define YELLOW	0xffe0
#define WHITE	0xffff

//Other Colors
#define CYAN		0x07ff	
#define BRIGHT_RED	0xf810	
#define GRAY1		0x8410  
#define GRAY2		0x4208 

//TFT_RGB - take a traditional 8 bit per channel RGB colour, and turn it into 5/6/5 bit colour for the TFT
#define TFT_RGB(red, green, blue) (((((uint16_t)red>>3) & 0x001F)<<11)|((((uint16_t)green>>2) & 0x003F) << 5) | (((uint16_t)blue>>3) & 0x001F))

//TFT resolution 240*320
#define MIN_X	0
#define MIN_Y	0
#define MAX_X	239
#define MAX_Y	319

//!!!!!!!!!!! this should be fixed if flipXY

//SPI DEFS:
//MOSI P1_7
//MISO P1_6
//SCLK P1_5
//CS pin P2.0
//DC pin P2.1
//BL pin P2.2

/*
#define TFT_CS_LOW  {P2OUT &=~ 0x01;}
#define TFT_CS_HIGH {P2OUT |=  0x01;}
#define TFT_DC_LOW  {P2OUT &=~ 0x02;}
#define TFT_DC_HIGH {P2OUT |=  0x02;}
#define TFT_BL_OFF  {P2OUT &=~ 0x04;}
#define TFT_BL_ON   {P2OUT |=  0x04;}
*/

#define TFT_CS_LOW  {digitalWrite(_cs, LOW);}
#define TFT_CS_HIGH {digitalWrite(_cs, HIGH);}
#define TFT_DC_LOW  {digitalWrite(_dc, LOW);}
#define TFT_DC_HIGH {digitalWrite(_dc, HIGH);}
#define TFT_BL_OFF  {digitalWrite(_bl, LOW);}
#define TFT_BL_ON   {digitalWrite(_bl, HIGH);}

#define LCD_FLIP_X	0x01
#define LCD_FLIP_Y	0x02
#define LCD_SWITCH_XY 0x04
#define LCD_LANDSCAPE (LCD_SWITCH_XY+LCD_FLIP_Y)
#define LCD_PORTRAIT 0

#define LCD_HORIZONTAL 0
#define LCD_VERTICAL 1

#ifndef INT8U
#define INT8U unsigned char
#endif
#ifndef INT16U
#define INT16U unsigned int
#endif

#define FONT_SPACE 6
//#define FONT_X 8
#define FONT_Y 8

extern INT8U simpleFont[][6];

class TFT
{
public:
	void TFTinit (INT8U cs=P2_0, INT8U dc=P2_1, INT8U bl=P2_2);
	void setBl(bool on) {digitalWrite(_bl, on ? HIGH : LOW);}
	//void setCol(INT16U StartCol,INT16U EndCol);
	//void setPage(INT16U StartPage,INT16U EndPage);
	//void setXY(INT16U poX, INT16U poY);
        void setWindow(INT16U StartCol, INT16U EndCol, INT16U StartPage,INT16U EndPage);
        void setPixel(INT16U poX, INT16U poY,INT16U color);
	void sendCMD(INT8U index);
	//void WRITE_Package(INT16U *data,INT8U howmany);
	void WRITE_DATA(INT8U data);
        void Write_Bulk(const INT8U *data, INT8U count);
	void sendData(INT16U data);
	//INT8U Read_Register(INT8U Addr,INT8U xParameter);
	void fillScreen(INT16U XL,INT16U XR,INT16U YU,INT16U YD,INT16U color);
        void fillScreen(void);
	//INT8U readID(void);

	void setOrientation(int value);

	void drawChar(INT8U ascii,INT16U poX, INT16U poY,INT16U size, INT16U fgcolor, INT16U bgcolor=0, bool opaq=false);
        INT16U drawString(const char *string,INT16U poX, INT16U poY,INT16U size,INT16U fgcolor, INT16U bgcolor=0, bool opaq=false);
	void fillRectangle(INT16U poX, INT16U poY, INT16U length, INT16U width, INT16U color) { fillScreen(poX, poX+length, poY, poY+width, color); }
	
	void drawLine(INT16U x0,INT16U y0,INT16U x1,INT16U y1,INT16U color);
        void drawLineThick(INT16U x0,INT16U y0,INT16U x1,INT16U y1,INT16U color,INT8U th);
        void drawVerticalLine(INT16U poX, INT16U poY,INT16U length,INT16U color) {fillScreen(poX,poX,poY,poY+length,color);}  
        void drawHorizontalLine(INT16U poX, INT16U poY,INT16U length,INT16U color) {fillScreen(poX,poX+length,poY,poY,color);}   
        void drawStraightDashedLine(INT8U dir, INT16U poX, INT16U poY, INT16U length, INT16U color, INT16U bkolor, INT8U mask);

        void drawRectangle(INT16U poX, INT16U poY, INT16U length,INT16U width,INT16U color);
	
        INT16U  getMinX() { return 0; }
        INT16U  getMinY() { return 0; }
        INT16U  getMaxX() { return _flags&LCD_SWITCH_XY ? MAX_Y : MAX_X; }
        INT16U  getMaxY() { return _flags&LCD_SWITCH_XY ? MAX_X : MAX_Y; }
        	
protected:
	INT8U _flags;	
	INT8U _cs, _dc, _bl;	
	
};

extern TFT Tft;

#endif

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/


