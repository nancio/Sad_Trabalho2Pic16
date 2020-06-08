#include "I2C.h"
#include <pic16f877a.h>
#include <stdio.h>
#include <string.h>
#include <xc.h>

#define _XTAL_FREQ 4000000

void I2C_init(const unsigned long feq_K)
{
  TRISC3 = 1;  TRISC4 = 1;  //Set SDA and SCL pins as input pins

  SSPCON  = 0b00101000;    //pg84/234
  SSPCON2 = 0b00000000;    //pg85/234

  SSPADD = (_XTAL_FREQ/(4*feq_K*100))-1; //Setting Clock Speed pg99/234
  SSPSTAT = 0b00000000;    //pg83/234
}

void I2C_Hold()
{
    while (   (SSPCON2 & 0b00011111)    ||    (SSPSTAT & 0b00000100)   ) ; //check the this on registers to make sure the IIC is not in progress
}

void I2C_Begin()
{
  I2C_Hold();  //Hold the program is I2C is busy 
  SEN = 1;     //Begin IIC pg85/234
}

void I2C_End()
{
  I2C_Hold(); //Hold the program is I2C is busy 
  PEN = 1;    //End IIC pg85/234
}

void I2C_Write(unsigned data)
{
  I2C_Hold(); //Hold the program is I2C is busy
  SSPBUF = data;         //pg82/234
}

unsigned short I2C_Read(unsigned short ack)
{
  unsigned short incoming;
  I2C_Hold();
  RCEN = 1;

  I2C_Hold();
  incoming = SSPBUF;      //get the data saved in SSPBUF

  I2C_Hold();
  ACKDT = (ack)?0:1;    //check if ack bit received 
  ACKEN = 1;          //pg 85/234

  return incoming;
}