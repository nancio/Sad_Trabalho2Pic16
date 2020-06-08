#ifndef I2C_H_
#define I2C_H_

void I2C_init(const unsigned long feq_K);

void I2C_Hold();

void I2C_Begin();

void I2C_End();

void I2C_Write(unsigned data);

unsigned short I2C_Read(unsigned short ack);

#endif /* I2C_H_ */
