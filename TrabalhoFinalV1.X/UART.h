#ifndef UART_H_
#define UART_H_

char UART_Init(const long int baudrate);

void UART_Write(char data);

char UART_TX_Empty();

void UART_Write_Text(char *text);

char UART_Data_Ready();

char UART_Read();

void UART_Read_Text(char *Output, unsigned int length);

void  UART_Write_Decimal(int Dec);

#endif /* UART_H_ */