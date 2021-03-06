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
 Foundation, Inc.,51 Franklin St,Fifth Floor, Boston, MA 02110-1301 USA

 ILI9341
*/

/*
DONE

2. font[8]->[6] - OK
3. fillScreen - for inc loop -> while dec lopp - OK
4. fillScreen() - just call generic fillscreen(0,0,max,max,0); - OK
5. straight lines - fill screen - OK
5. dashedLine for inc loops -> while dec loops - OK
2. setCol(poX0,poX0);setPage(poY0,poY0); - as one sendData with 4 bytes transfer - OK
3. init - use bulk send - OK

TOTEST
1. drawThickLine - loops replaced with drawlines - FIXED - TEST AGAIN
4. drwaChar - opaq optimization (?) - NO, much slower

TODO

6. dash line - optimize (rewrite like fullScreen)
7. draw lines - save on local vars

*/

#include "TFT_ILI9341.h"
#include <SPI.h>

void TFT::sendCMD(INT8U index)
{
    TFT_DC_LOW;
    TFT_CS_LOW;
    SPI.transfer(index);
    TFT_CS_HIGH;
}

void TFT::WRITE_DATA(INT8U data)
{
    TFT_DC_HIGH;
    TFT_CS_LOW;
    SPI.transfer(data);
    TFT_CS_HIGH;
}

void TFT::sendData(INT16U data)
{
    INT8U data1 = data>>8;
    INT8U data2 = data&0xff;
    TFT_DC_HIGH;
    TFT_CS_LOW;
    SPI.transfer(data1);
    SPI.transfer(data2);
    TFT_CS_HIGH;
}

/*
void TFT::Write_Bulk(const INT8U *data, INT8U count)
{
    TFT_DC_HIGH;
    TFT_CS_LOW;
    for(INT8U i=0; i<count; i++) SPI.transfer(data[i]);
    TFT_CS_HIGH;
}

*/

void TFT::WriteCmdSeq(const INT8U *data)
{
  while(*data) {
    INT8U data_len1=(*data++);
    sendCMD(*data++);
    while(--data_len1) WRITE_DATA(*data++);    
  }
}

/*
void TFT::WRITE_Package(INT16U *data, INT8U howmany)
{
    INT16U    data1 = 0;
    INT8U   data2 = 0;

    TFT_DC_HIGH;
    TFT_CS_LOW;
    INT8U count=0;
    for(count=0;count<howmany;count++)
    {
        data1 = data[count]>>8;
        data2 = data[count]&0xff;
        SPI.transfer(data1);
        SPI.transfer(data2);
    }
    TFT_CS_HIGH;
}
*/
/*
INT8U TFT::Read_Register(INT8U Addr, INT8U xParameter)
{
    INT8U data=0;
    sendCMD(0xd9);                                                      // ext command                  
    WRITE_DATA(0x10+xParameter);                                        // 0x11 is the first Parameter  
    TFT_DC_LOW;
    TFT_CS_LOW;
    SPI.transfer(Addr);
    TFT_DC_HIGH;
    data = SPI.transfer(0);
    TFT_CS_HIGH;
    return data;
}
*/
/*
INT8U TFT::readID(void)
{
    INT8U i=0;
    INT8U data[3] ;
    INT8U ID[3] = {0x00, 0x93, 0x41};
    INT8U ToF=1;
    for(i=0;i<3;i++)
    {
        data[i]=Read_Register(0xd3,i+1);
        if(data[i] != ID[i])
        {
            ToF=0;
        }
    }
    if(!ToF)                                                            // data!=ID                    
    {
	
        Serial.print("Read TFT ID failed, ID should be 0x09341, but read ID = 0x");
        for(i=0;i<3;i++)
        {
            Serial.print(data[i],HEX);
        }
        Serial.println();
	
    }
    return ToF;
}
*/

void TFT::TFTinit (INT8U cs, INT8U dc, INT8U bl) 
{
    _cs=cs;
    _dc=dc; 
    _bl=bl;
    _flags = 0;
	
    pinMode(_cs,OUTPUT);
    pinMode(_dc,OUTPUT);
    pinMode(_bl,OUTPUT);
	
    SPI.begin();
    SPI.setClockDivider(2);
    TFT_CS_HIGH;
    TFT_DC_HIGH;
    /*
    INT8U i=0, TFTDriver=0;
    for(i=0;i<3;i++)
    {
        TFTDriver = readID();
    }
    */
    delay(500);
    sendCMD(0x01);
    delay(200);

    const INT8U seq[]={
      4, 0xCF,    0x00,0x8B,0x30,
      5, 0xED,    0x67,0x03,0x12,0x81,
      4, 0xE8,    0x85,0x10,0x7A,
      6, 0xCB,    0x39,0x2C,0x00,0x34,0x02,
      2, 0xF7,    0x20,
      3, 0xEA,    0x00, 0x00,
      2, 0xC0,    0x1B, /* Power control     VRH[5:0]              */
      2, 0xC1,    0x10, /* Power control    SAP[2:0];BT[3:0]            */
      3, 0xC5,    0x3F, 0x3C,  /* VCM control                  */
      2, 0xC7,    0xB7,       /* VCM control2                 */
      2, 0x36,    0x08,        /* Memory Access Control   portrait     */
      2, 0x3A,    0x55,
      3, 0xB1,    0x00, 0x1B,
      3, 0xB6,    0x0A, 0xA2,    /* Display Function Control     */
      2, 0xF2,    0x00,          /* 3Gamma Function Disable      */
      2, 0x26,    0x01,          /* Gamma curve selected         */
      16, 0xE0,   0x0F,0x2A,0x28,0x08,0x0E,0x08,0x54,0xA9,0x43,0x0A,0x0F,0x00,0x00,0x00,0x00, /* Set Gamma                    */
      16, 0xE1,   0x00,0x15,0x17,0x07,0x11,0x06,0x2B,0x56,0x3C,0x05,0x10,0x0F,0x3F,0x3F,0x0F,  /* Set Gamma                    */    
      0  
  };
    
    WriteCmdSeq(seq);

    sendCMD(0x11);                                                      /* Exit Sleep                   */
    delay(120);
    sendCMD(0x29);                                                      /* Display on                   */
    fillScreen();
}

void TFT::setOrientation(int flags) 
{
  INT8U madctl = 0x08;
  if(flags & LCD_FLIP_X) madctl &= ~(1 << 6);
  if(flags & LCD_FLIP_Y) madctl |= 1 << 7;
  if(flags & LCD_SWITCH_XY) madctl |= 1 << 5;
  sendCMD(0x36);
  WRITE_DATA(madctl);
  _flags=flags;
}

/*
void TFT::setCol(INT16U StartCol,INT16U EndCol)
{
    sendCMD(0x2A);                                                      // Column Command address       
    sendData(StartCol); // can we optimize to send 2 integers ?????????
    sendData(EndCol);
}

void TFT::setPage(INT16U StartPage,INT16U EndPage)
{
    sendCMD(0x2B);                                                      // Column Command address       
    sendData(StartPage);
    sendData(EndPage);
}
*/

void TFT::setWindow(INT16U StartCol, INT16U EndCol, INT16U StartPage,INT16U EndPage)
{
    sendCMD(0x2A);                                                      
    sendData(StartCol); 
    sendData(EndCol);
    sendCMD(0x2B);                                                      
    sendData(StartPage);
    sendData(EndPage);
    sendCMD(0x2c);
}

void TFT::fillScreen(INT16 XL, INT16 XR, INT16 YU, INT16 YD, INT16U color)
{
    unsigned long  XY=0;

    if(XL > XR) { XL = XL^XR; XR = XL^XR; XL = XL^XR;}
    if(YU > YD) { YU = YU^YD; YD = YU^YD; YU = YU^YD;}
	
    XL = constrain(XL,0,getMaxX());
    XR = constrain(XR,0,getMaxX());
    YU = constrain(YU,0,getMaxY());
    YD = constrain(YD,0,getMaxY());
		
    XY = (XR-XL+1);
    XY = XY*(YD-YU+1);
    setWindow(XL,XR,YU,YD);
    
    TFT_DC_HIGH;
    TFT_CS_LOW;
    INT8U Hcolor = color>>8;
    INT8U Lcolor = color&0xff;
    
    while(XY--) {
      SPI.transfer(Hcolor);
      SPI.transfer(Lcolor);
    }    
    TFT_CS_HIGH;
}

void TFT::fillScreen(void)
{
   fillScreen(0, getMaxX(), 0, getMaxY(), 0);
}

/*
void TFT::setXY(INT16U poX, INT16U poY)
{
    setCol(poX, poX);
    setPage(poY, poY);
    sendCMD(0x2c);
}
*/

void TFT::setPixel(INT16U poX, INT16U poY,INT16U color)
{
    //setXY(poX, poY);
    setWindow(poX,poX,poY,poY);
    sendData(color);
}

void TFT::drawChar( INT8U ascii, INT16 poX, INT16 poY, INT16U size, INT16U fgcolor, INT16U bgcolor, bool opaq)
{   
    if(opaq) fillRectangle(poX, poY, FONT_SPACE*size, FONT_Y*size, bgcolor);	
    if((ascii<32)||(ascii>129)) ascii = '?';
    INT16U x=poX;
    for (INT8U i=0; i<FONT_SZ; i++, x+=size ) {
        INT8U temp = simpleFont[ascii-0x20][i];
        INT16U y=poY;
        for(INT8U f=0;f<8;f++, y+=size)
        {
            if((temp>>f)&0x01) // if bit is set in the font mask
            {
                fillScreen(x, x+size, y, y+size, fgcolor);
            }
        }
    }
}

INT16U TFT::drawString(const char *string,INT16 poX, INT16 poY, INT16U size,INT16U fgcolor, INT16U bgcolor, bool opaq)
{
    while(*string)
    {
        drawChar(*string, poX, poY, size, fgcolor, bgcolor, opaq);
        //*string++;
        string++;
        if(poX < getMaxX()) poX += FONT_SPACE*size;   /* Move cursor right */
        else break;
    }
    return poX;
}

void TFT::drawStraightDashedLine(INT8U dir, INT16 poX, INT16 poY, INT16U length,INT16U color, INT16U bkcolor, INT8U mask)
{
    if(dir==LCD_HORIZONTAL) setWindow(poX,poX + length,poY,poY);
    else setWindow(poX,poX,poY,poY + length);
    while(length--) sendData((mask>>(length&0x07))&0x01 ? color : bkcolor);
}

/*
void TFT::drawLine( INT16 x0,INT16 y0,INT16 x1, INT16 y1,INT16U color)
{    
    int x = x1-x0;
    int y = y1-y0;
    int dx = abs(x), sx = x0<x1 ? 1 : -1;
    int dy = -abs(y), sy = y0<y1 ? 1 : -1;
    int err = dx+dy, e2;                                                // error value e_xy             
    for (;;){                                                           // loop                         
        setPixel(x0,y0,color);
        e2 = 2*err;
        if (e2 >= dy) {                                                 // e_xy+e_x > 0                 
            if (x0 == x1) break;
            err += dy; x0 += sx;
        }
        if (e2 <= dx) {                                                 // e_xy+e_y < 0                 
            if (y0 == y1) break;
            err += dx; y0 += sy;
        }
    }
}
*/

void TFT::drawLineThick(INT16 x0,INT16 y0,INT16 x1,INT16 y1,INT16U color,INT8U th)
{
  /*
    int x = x1-x0;
    int y = y1-y0;
    int dx = abs(x), sx = x0<x1 ? 1 : -1;
    int dy = -abs(y), sy = y0<y1 ? 1 : -1;
    */
    
    int16_t dx, dy;
    int16_t sx, sy;
    
    if(x0<x1) {dx=x1-x0; sx=1;} else {dx=x0-x1; sx=-1; }
    if(y0<y1) {dy=y0-y1; sy=1;} else {dy=y1-y0; sy=-1; }
    
    int err = dx+dy, e2;                                                /* error value e_xy             */
    INT8U th2=th/2;
    for (;;){                                                           /* loop                         */
        e2 = 2*err;
        if (e2 >= dy) {                   /* e_xy+e_x > 0                 */
            //for(int it=-th2; it<=th2; it++) setPixel(x0,y0+it,color); // replace with fillScreen?
            drawVerticalLine(x0, y0-th2, th, color);
            if (x0 == x1) break;
            err += dy; x0 += sx;
        }
        if (e2 <= dx) {                   /* e_xy+e_y < 0                 */
            //for(int it=-th2; it<=th2; it++) setPixel(x0+it,y0,color); // replace with fillScreen?
            drawHorizontalLine(x0-th2, y0, th, color);
            if (y0 == y1) break;
            err += dx; y0 += sy;
        }
    }
}
        
void TFT::drawRectangle(INT16 poX, INT16 poY, INT16U length, INT16U width,INT16U color)
{
    drawHorizontalLine(poX, poY, length, color);
    drawHorizontalLine(poX, poY+width, length, color);
    drawVerticalLine(poX, poY, width,color);
    drawVerticalLine(poX + length, poY, width,color);

}

TFT Tft=TFT();
/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
