#include <pic16f877a.h>
#include <xc.h>
#include <stdio.h>
#include <string.h>
#include "ADC.h"
#include "UART.h"
#include "I2C.h"


#define _XTAL_FREQ 4000000
#define TMR2PRESCALE 4

#define TL1 0x01
#define TL2 0x02
#define TL3 0x04

#define TC1 PORTDbits.RD3
#define TC2 PORTDbits.RD2
#define TC3 PORTDbits.RD1
#define TC4 PORTDbits.RD0

#define PASS 1234

typedef struct
{
    int temperatura;
    int huminade;
    int velocidade;
    int alarm;
}Data_struct;

typedef struct
{
    int rajada_vento;
    int humidade_incendio; 
    int temperatuta_incendio; 
}Calibrar_struct;


#pragma config FOSC = HS // Oscillator Selection bits (HS oscillator)
#pragma config WDTE = OFF // Watchdog Timer Enable bit
#pragma config PWRTE = OFF // Power-up Timer Enable bit
#pragma config BOREN = OFF // Brown-out Reset Enable bit
#pragma config LVP = OFF // Low-Voltage (Single-Supply) In-Circuit Serial Programming Enable bit
#pragma config CPD = OFF // Data EEPROM Memory Code Protection bit
#pragma config WRT = OFF // Flash Program Memory Write Enable bits
#pragma config CP = OFF // Flash Program Memory Code Protection bit

long PWM_freq = 5000; //7025
int fan_rpm = 0;
int fan_count= 0;
int count_timer_fan_speed=0;
int count_minute = 0;
int running = 0;

Data_struct Data_atual;

int LAST_ALARM = 0;

char json[80];


void PWM_Initialize()
{
  PR2 = (_XTAL_FREQ/(PWM_freq*4*TMR2PRESCALE)) - 1; //Setting the PR2 formulae using Datasheet // Makes the PWM work in 5KHZ
  CCP1M3 = 1; 
  CCP1M2 = 1;  //Configure the CCP1 module 
  T2CKPS0 = 1;
  T2CKPS1 = 0;
  TMR2ON = 1; //Configure the Timer module
  TRISC2 = 0; // make port pin on C as output
}

void PWM_Duty(unsigned int duty)
{
  if(duty<1023)
  {
    duty = ((float)duty/1023)*(_XTAL_FREQ/(PWM_freq*TMR2PRESCALE)); // On reducing //duty = (((float)duty/1023)*(1/PWM_freq)) / ((1/_XTAL_FREQ) * TMR2PRESCALE);
    CCP1X = duty & 1; //Store the 1st bit
    CCP1Y = duty & 2; //Store the 0th bit
    CCPR1L = duty>>2;// Store the remining 8 bit
  }
}

int get_temperatura()
{
  int a;
  int b;

  a = ADC_Read(2);
  b = (a*27.5)/56;
  return b;  
}

int get_humidade()
{
  int a;
  a = ADC_Read(1);
  return a;  
}

void print_json()
{ 
  sprintf(json,"{'temperatura' : %d , 'humidade' : %d , 'velocidade' : %d, 'alarm' : %d}$", Data_atual.temperatura , Data_atual.huminade, Data_atual.velocidade, Data_atual.alarm);
  UART_Write_Text(json);
  UART_Write_Text("\n");
}

Calibrar_struct read_from_MR(Calibrar_struct calibracao)
{
    char output[8];   // recebe uma sring de um numero xxxxxx
    int int_input = 0;                   // se for  1xxxxx é um pedido de informacao
    UART_Read_Text(output,8);            // se for  00yyyy mudar o valor da calibracao do vento yyyy->valor
    output[7]='\0';
    output[6]='\n';
    UART_Write_Text(output);             // se for  01yyyy mudar o valor da calibracao da humidade yyyy->valor
                                         // se for  02yyyy mudar o valor da calibracao da temperatura yyyy->valor
    if(output[0]==49)
    {
        UART_Write_Text("1 Pedido de informacao \n");
        print_json();
        return calibracao;
    }
    int_input = (output[2]-48)*1000+(output[3]-48)*100+(output[4]-48)*10+(output[5]-48);
    UART_Write_Decimal(int_input);
    UART_Write_Text("\n");
    switch (output[1])
    {
        case 48:
            UART_Write_Text("0 0 vento \n");
            calibracao.rajada_vento = int_input;
            break;
        case 49:
            UART_Write_Text("0 1 humindade \n");
            calibracao.humidade_incendio = int_input;
            break;
        case 50:
            UART_Write_Text("0 2 temperatura \n");
            calibracao.temperatuta_incendio = int_input;
            break;
    }
    return calibracao;
}

void __interrupt() Interrupt_Time()
{
  if (TMR0IF==1) //Check if Timer0 has caused the interrupt
  { 
    count_timer_fan_speed++;             //Increasing by 1
    count_minute++;
    TMR0=218; //timer //for 0.001
    T0IF=0; 
    if(count_timer_fan_speed==10)
    {  // 1s delay ->100  0.1->10
      count_timer_fan_speed=0;          //when count reaches 10 - Resetting to 0
      fan_count=TMR1L;
      TMR1L=0;
      TMR0IF=0;
      fan_rpm=(fan_count/7)*600; 
    }

    if(count_minute==500) //6000
    { 
      count_minute=0;
      if (running==1)
      {
        print_json();
      }
    }
  }
    
  if (INTF==1){
    INTF=0; // Reset the external interrupt flag
    if (running==1)
    {
      running = 0;
      UART_Write_Text("Sleep Mode.");
      UART_Write_Text("\n"); 
    }else{
      running = 1;
      UART_Write_Text("Waking Up!");
      UART_Write_Text("\n");
    }
  }
}

unsigned char teclado(unsigned int timeout)
{
  unsigned int to=0;
  unsigned char i;
  unsigned char ret=0;
  unsigned char tmp=PORTB;
  const unsigned char linha[4]= {TL1,TL2,TL3};


  while(((to < timeout)||(!timeout))&&(!ret))  
  {
    for(i=0;i<3;i++)
    {
      PORTB|=~linha[i];
      if(!TC1)
      {
          __delay_ms(20);
          if(!TC1)
          {
              while(!TC1);
              ret= 1+i;
              break;
          }
      }
      if(!TC2)
      {
          __delay_ms(20);
          if(!TC2)
          {
              while(!TC2);
              ret= 4+i;
              break;
          }
      }
      if(!TC3)
      {
          __delay_ms(20);
          if(!TC3)
          {
              while(!TC3);
              ret= 7+i;
              break;
          }
      }
      if(!TC4)
      {
          __delay_ms(20);
          if(!TC4)
          {
              while(!TC4);
              ret= 10+i;
              break;
          }
      }
      PORTB &=linha[i];
    }
    __delay_ms(5);
    to+=5;
  }
  
  if(!ret)ret=255;
  if(ret == 11)ret=0;
  PORTB=tmp;
  return ret;
}

void move_the_fan()
{
  int a= ADC_Read(0);
  PWM_Duty(a); 
}

Data_struct Aquisicao_Dados()
{
  Data_struct aqui;
  aqui.huminade=get_humidade();
  aqui.temperatura=get_temperatura();
  aqui.velocidade=fan_rpm;
  return aqui;
}

void write_to_EPPROM()
{
  char text1[30] =" Trabalho SAD \n \0";
  int count_string = strlen(text1);
  int epro_count=0;

  I2C_Begin(); 	
  I2C_Write(160); 	
  I2C_Write(0); 		

  while(epro_count<count_string)
  {
    I2C_Write(text1[epro_count]);
    epro_count++;
  }
  I2C_End(); 		// Sends I2C stop-condition
}

void read_from_EPPROM()
{
  char data[10];  // Array to store the received data

  I2C_Begin(); 	
  I2C_Write(160);

  I2C_Write(1); // word address location

  I2C_Begin(); 	
  I2C_Write(160);
   		
  I2C_Read(data[0]);  // Reads 1 byte from I2C and writes it to the array
  I2C_End();  // Sends I2C stop-condition
  UART_Write_Text(data);
}

void alarm()
{  
  if ((Data_atual.alarm==1 || Data_atual.alarm==2 || Data_atual.alarm==3) && LAST_ALARM!=Data_atual.alarm)
  {
    PORTCbits.RC1=1; //liga buzzer
    PORTBbits.RB7=1; //ligar LED
    print_json(); //imprimir json para uart
    LAST_ALARM=Data_atual.alarm;    
  }else if(Data_atual.alarm==0){
    PORTCbits.RC1=0; //desliga buzzer
    PORTBbits.RB7=0; //desliga LED
    LAST_ALARM=0;
  }
}

void alarm_check(Calibrar_struct calibracao) 
{
  int magic = Data_atual.velocidade;
  if(Data_atual.huminade<calibracao.humidade_incendio && Data_atual.temperatura>calibracao.temperatuta_incendio && Data_atual.velocidade>calibracao.rajada_vento)
  {
    Data_atual.alarm=1;
    alarm(); 
  }else if(Data_atual.velocidade>magic){
    Data_atual.alarm=2;
    alarm();
  }else{
    Data_atual.alarm=0;
    alarm();
  }
}


int main(void)
{
  UART_Init(9600);
  I2C_init(250); //250
  ADC_Init();
  PWM_Initialize();  //This sets the PWM frequency of PWM1


  GIE=1; //Enable Global Interrupt
  PEIE=1; //Enable the Peripheral Interrupt
  INTE = 1; //Enable RB0 as external Interrupt pin

/*****Port Configuration for Timer2 PWM ******/
  CCP1CON = 0X0F;       // Select the PWM Mode
  PR2 = 0x21;           //31Set the Cycle time for varying the duty cycle
  CCPR1L = 50;          // By default set the dutyCycle to 50
  TMR2ON = 1;           //Start the Timer for PWM generatio
/***********______***********/ 

/*****Port Configuration for Timer1 Counter******/
  T1CON=0;
  TMR1CS=1;
  T1SYNC=0;
  T1OSCEN=1;
  TMR1ON=1;
/***********______***********/ 

/*****Port Configuration for Timer0 Timer******/
  OPTION_REG =0x07;
  TMR0IE=1;
  TMR0=217;  
  TMR0IE=1;
/*********______***********/
 
  PORTB = 0XFF;                       // PORTB as FF
  TRISB = 0;                          // Configure PORTB as output
  TRISD = 0XFF;                       // TRISD as FF
  PORTD = 0XFF;                       // PORTD as FF
  TRISD = 0XFF;                       // Configure PORTD as input

  TRISA = 0xFF; //Analog pins as Input
    
  TRISCbits.TRISC0 = 1;//input velocidade fan
  TRISCbits.TRISC1 = 0;//pin buzzer output
  TRISCbits.TRISC2 = 0;//PWM/FAN output
  TRISCbits.TRISC5 = 0;//pin Resistencia output

  TRISBbits.TRISB0 = 1;//pin INT input
  TRISBbits.TRISB7 = 0; //LED ALARM

  char password[4]= {0, 0, 0, 0}; 
  int password_int = 0;
  int pass_count = 0;

  Data_atual.alarm=0;

  Calibrar_struct calibracao;
  calibracao.rajada_vento = 2400; //calibracao dos sensores
  calibracao.humidade_incendio = 500;  //calibracao dos sensores
  calibracao.temperatuta_incendio = 40;  //calibracao dos sensores
  
  
  UART_Write_Text("Inserir Password");
  UART_Write_Text("\n");

  while(password_int!=PASS) //fica a espera da palavra pass correta
  {
    pass_count=0;
    while(pass_count<4)
    {
      TRISBbits.TRISB0 = 0;
      TRISD=0x0F;
      unsigned char abc = teclado(1500)+0x30; //timeout retirado de codigo fonte
      TRISD=0x00;
      TRISBbits.TRISB0 = 1;
      if (abc!=47)
      {
        UART_Write(abc);
        password[pass_count]=abc;
        pass_count++;
      }
    }
    password_int = (password[0]-48)*1000+(password[1]-48)*100+(password[2]-48)*10+(password[3]-48);

    if(password_int==PASS)
    {
      UART_Write_Text("\n");
      UART_Write_Text("Password Correta");
      UART_Write_Text("\n");
    }else{
      UART_Write_Text("\n");
      UART_Write_Text("Password Incorreta");
      UART_Write_Text("\n");
    }
  }
    
  write_to_EPPROM();
  read_from_EPPROM();
    
  running=1;

  TRISBbits.TRISB3 = 1;
  TRISDbits.TRISD0 = 0;
  PORTDbits.RD0 = 0;

  while(1)
  {
    while (running==1)
    {
      Data_atual = Aquisicao_Dados();
      move_the_fan();
      
      if(UART_Data_Ready())
          calibracao=read_from_MR(calibracao);

      alarm_check(calibracao);
      
      if (!PORTBbits.RB3)
      {  
        PORTCbits.RC5=1; //liga resistencia
      }else{
        PORTCbits.RC5=0; //desliga resistencia
      }        

    __delay_ms(100); 
    }   
  }
}
