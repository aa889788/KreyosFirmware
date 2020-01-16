#include "contiki.h"
#include <stdio.h>
#include "sys/rtimer.h"
#include "isr_compat.h"
#include "spiflash.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#define hexdump(...)
#endif

/*
* Exchanges data on SPI connection
* - Busy waits until entire shift is complete
* - This function is safe to use to control hardware lines that rely on shifting being finalised
*/

/******************************************************************************************
*��������SPI_FLASH_SendByte()
* ������u8 byte        д�������
* ����ֵ��u8 8λ����
* ���ܣ�SPIFLASH��дһ���ֽں������ⲿ����
*********************************************************************************************/
static inline uint8_t SPI_FLASH_SendByte( uint8_t data )
{
  UCA1TXBUF = data;
  while(( UCA1IFG & UCRXIFG ) == 0x00 ); // Wait for Rx completion (implies Tx is also complete)
  return( UCA1RXBUF );
}

static void SPI_FLASH_SendCommandAddress(uint8_t opcode, uint32_t address)
{
  SPI_FLASH_SendByte(opcode);
  SPI_FLASH_SendByte((address >> 16) & 0xFF);
  SPI_FLASH_SendByte((address >> 8) & 0xFF);
  SPI_FLASH_SendByte(address & 0xFF);
}

static inline void SPI_FLASH_CS_LOW()
{
  UCA1CTL1 &= ~UCSWRST;
  SPIOUT &= ~CSPIN;
}

static inline void SPI_FLASH_CS_HIGH()
{
  while(UCA1STAT & UCBUSY);
  UCA1CTL1 |= UCSWRST;
  SPIOUT |= CSPIN;
  __delay_cycles(10);
}


/******************************************************************************************
*��������SPI_FLASH_SectorErase()
* ������u32 SectorAddr   ���ַ
* ����ֵ��void
* ���ܣ�SPIFLASH���������������ⲿ����
*********************************************************************************************/
void SPI_FLASH_SectorErase(u32 SectorAddr, u32 size)
{
  uint8_t opcode;
  if (size == 4 * 1024UL)
  {
    opcode = W25X_SectorErase;
  }
  else if (size == 32 * 1024UL)
  {
    opcode = W25X_BlockErase32;
  }
  else if (size == 64 * 1024UL)
  {
    opcode = W25X_BlockErase64;
  }
  else
  {
    PRINTF("Error in erase size: %lx\n", size);
    return;
  }

  PRINTF("Erase offset: %lx\n", SectorAddr);
 
  /*����д����ʹ��ָ��*/
  SPI_FLASH_WriteEnable();
  /* ʹ��Ƭѡ */
  SPI_FLASH_CS_LOW();
  /*������������ָ��*/
  SPI_FLASH_SendCommandAddress(opcode, SectorAddr);
  /*ʧ��Ƭѡ*/
  SPI_FLASH_CS_HIGH();
  /* �ȴ�д���*/
  SPI_FLASH_WaitForWriteEnd();
}

/******************************************************************************************
*��������SPI_FLASH_BulkErase()
* ������void
* ����ֵ��void
* ���ܣ�SPIFLASH��Ƭ�����������ⲿ����
*********************************************************************************************/
void SPI_FLASH_BulkErase(void)
{
  /*ʹ��д��*/
  SPI_FLASH_WriteEnable();
   /* ʹ��Ƭѡ */
  SPI_FLASH_CS_LOW();
  /*������Ƭ����ָ��*/
  SPI_FLASH_SendByte(W25X_ChipErase);
  /*ʧ��Ƭѡ*/
  SPI_FLASH_CS_HIGH();
  /* �ȴ�д���*/
  SPI_FLASH_WaitForWriteEnd();
}

/******************************************************************************************
*��������SPI_FLASH_PageWrite()
* ������u8* pBuffer, u32 WriteAddr, u16 NumByteToWrite ����ָ�룬д���ַ��д��ĸ���
* ����ֵ��void
* ���ܣ�SPIFLASHҳд�����ݺ������ⲿ����
*********************************************************************************************/
void SPI_FLASH_PageWrite(u8* pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
  PRINTF("Write to Disk: offset:%lx size:%d\n", WriteAddr, NumByteToWrite);
  hexdump(pBuffer, NumByteToWrite);

   /*ʹ��д��*/
  SPI_FLASH_WriteEnable();
  /*ʹ��Ƭѡ*/
  SPI_FLASH_CS_LOW();
  /* ����ҳд��ָ��*/
  SPI_FLASH_SendCommandAddress(W25X_PageProgram, WriteAddr);

  /*���д��������Ƿ񳬳�ҳ��������С*/
  if(NumByteToWrite > SPI_FLASH_PerWritePageSize)
  {
     NumByteToWrite = SPI_FLASH_PerWritePageSize;
  }
  /*ѭ��д������*/
  while (NumByteToWrite--)
  {
    /*��������*/
    SPI_FLASH_SendByte(~(*pBuffer));
    /* ָ���Ƶ���һ��д������ */
    pBuffer++;
  }
  /*ʧ��Ƭѡ*/
  SPI_FLASH_CS_HIGH();
  /* �ȴ�д���*/
  SPI_FLASH_WaitForWriteEnd();
}

/******************************************************************************************
*��������SPI_FLASH_BufferWrite()
* ������u8* pBuffer, u32 WriteAddr, u16 NumByteToWrite ����ָ�룬д���ַ��д��ĸ���
* ����ֵ��void
* ���ܣ�SPIFLASH������ݺ������ⲿ����
*********************************************************************************************/
void SPI_FLASH_BufferWrite(u8* pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
  u8 NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0, temp = 0;
  Addr = WriteAddr % SPI_FLASH_PageSize;                           //����д���ҳ�Ķ�Ӧ��ʼ��ַ
  count = SPI_FLASH_PageSize - Addr;
  NumOfPage =  NumByteToWrite / SPI_FLASH_PageSize;                //�����ܹ�Ҫд��ҳ��
  NumOfSingle = NumByteToWrite % SPI_FLASH_PageSize;               //����ʣ�൥��ҳд�����ݸ���
  if (Addr == 0) /* ���Ҫд���ҳ��ַΪ0��˵��������ҳд���ݣ�û��ƫ��*/
  {
    if (NumOfPage == 0) /* ��������д��ҳ��Ϊ0��˵����������һ��ҳ�ķ�Χ�ڣ���ֱ�ӽ���ҳ��д*/
    {
      SPI_FLASH_PageWrite(pBuffer, WriteAddr, NumByteToWrite);      //����ҳд����
    }
    else /* ���Ҫд��ҳ������0*/
    { 
      /*�Ƚ���ͷ���ݽ�����ҳд��*/
      while (NumOfPage--)
      { 
        //��ҳд��
        SPI_FLASH_PageWrite(pBuffer, WriteAddr, SPI_FLASH_PageSize);
        //��ַƫ��
        WriteAddr +=  SPI_FLASH_PageSize;
        //����ָ��ƫ��
        pBuffer += SPI_FLASH_PageSize;
      }
       //��ʣ�����ݸ���д��
      SPI_FLASH_PageWrite(pBuffer, WriteAddr, NumOfSingle);
    }
  }
  else /*���д��ĵ�ַ����ҳ�Ŀ�ͷλ��*/
  {
    if (NumOfPage == 0) /*���д������ҳ�ĸ���Ϊ0��������С��һҳ����*/
    {
      if (NumOfSingle > count) /*���ʣ�����ݴ��ڵ�ǰҳ��ʣ������*/
      {
        temp = NumOfSingle - count;     //���㳬�������ݸ���
        /*д����ǰҳ*/
        SPI_FLASH_PageWrite(pBuffer, WriteAddr, count);
        /*���õ�ַƫ��*/
        WriteAddr +=  count;
        /*��������ָ��ƫ��*/
        pBuffer += count;
        /*��ʣ����д���µ�ҳ*/
        SPI_FLASH_PageWrite(pBuffer, WriteAddr, temp);
      }
      else  /*���ʣ������С�ڵ�ǰҳ��ʣ������*/
      {
        /*ֱ��д�뵱ǰҳ*/
        SPI_FLASH_PageWrite(pBuffer, WriteAddr, NumByteToWrite);
      }
    }
    else /*���д������ҳ�ĸ�������0�������ݴ���һҳ����*/
    {
      NumByteToWrite -= count;         //�����ݼ�ȥ��ǰҳʣ�������
      NumOfPage =  NumByteToWrite / SPI_FLASH_PageSize;  //����Ҫд����ҳ����
      NumOfSingle = NumByteToWrite % SPI_FLASH_PageSize; //����ʣ�����ݸ���
      /*����ͷ����д�뵱ǰҳʣ���ֽڸ���*/
      SPI_FLASH_PageWrite(pBuffer, WriteAddr, count);
      /*���õ�ַƫ��*/
      WriteAddr +=  count;
      /*��������ָ��ƫ��*/
      pBuffer += count;
       /*��ʼʣ�����ݵ���ҳд��*/
      while (NumOfPage--)
      {
        /*д��һ��ҳ���ֽ���*/
        SPI_FLASH_PageWrite(pBuffer, WriteAddr, SPI_FLASH_PageSize);
        /*���õ�ַƫ��*/
        WriteAddr +=  SPI_FLASH_PageSize;
        /*����ָ��ƫ��*/
        pBuffer += SPI_FLASH_PageSize;
      }
      /*���ʣ�����ݴ���0����ʣ��ĸ���д����һ��ҳ*/
      if (NumOfSingle != 0)
      {
        SPI_FLASH_PageWrite(pBuffer, WriteAddr, NumOfSingle);
      }
    }
  }
}

/******************************************************************************************
*��������SPI_FLASH_BufferRead()
* ������u8* pBuffer, u32 ReadAddr, u16 NumByteToRead ����ָ�룬�����ĵ�ַ�������ĸ���
* ����ֵ��void
* ���ܣ�SPIFLASH������ݺ������ⲿ����
*********************************************************************************************/
void SPI_FLASH_BufferRead(u8* pBuffer, u32 ReadAddr, u16 NumByteToRead)
{
  PRINTF("Read from Disk: offset:%lx size:%d\n", ReadAddr, NumByteToRead);
  //u16 n = NumByteToRead;

   /* ʹ��Ƭѡ */
  SPI_FLASH_CS_LOW();
  /*���Ͷ�����ָ��*/
  SPI_FLASH_SendCommandAddress(W25X_ReadData, ReadAddr);
  while (NumByteToRead--) /* ѭ����ȡ����*/
  {
    /*��ȡһ���ֽ�����*/
    *pBuffer = ~SPI_FLASH_SendByte(Dummy_Byte);
    /*����ָ���1*/
    pBuffer++;
  }
  /*ʧ��Ƭѡ*/
  SPI_FLASH_CS_HIGH();

  hexdump(pBuffer - n, n);
}

/******************************************************************************************
*��������SPI_FLASH_BufferRead()
* ������u8* pBuffer, u32 ReadAddr, u16 NumByteToRead ����ָ�룬�����ĵ�ַ�������ĸ���
* ����ֵ��void
* ���ܣ�SPIFLASH������ݺ������ⲿ����
*********************************************************************************************/
void SPI_FLASH_BufferRead_Raw(u8* pBuffer, u32 ReadAddr, u16 NumByteToRead)
{
  PRINTF("Read from Disk: offset:%lx size:%d\n", ReadAddr, NumByteToRead);
  //u16 n = NumByteToRead;

   /* ʹ��Ƭѡ */
  SPI_FLASH_CS_LOW();
  /*���Ͷ�����ָ��*/
  SPI_FLASH_SendCommandAddress(W25X_ReadData, ReadAddr);
  while (NumByteToRead--) /* ѭ����ȡ����*/
  {
    /*��ȡһ���ֽ�����*/
    *pBuffer = SPI_FLASH_SendByte(Dummy_Byte);
    /*����ָ���1*/
    pBuffer++;
  }
  /*ʧ��Ƭѡ*/
  SPI_FLASH_CS_HIGH();

  hexdump(pBuffer - n, n);
}

/******************************************************************************************
*��������SPI_FLASH_ReadID()
* ������void
* ����ֵ��u32 ����ID
* ���ܣ�SPIFLASH��ȡID�������ⲿ����
*********************************************************************************************/
u32 SPI_FLASH_ReadID(void)
{
  u32 Temp = 0, Temp0 = 0, Temp1 = 0, Temp2 = 0;

  /* ʹ��Ƭѡ */
  SPI_FLASH_CS_LOW();

  /*����ʶ������ID��*/
  SPI_FLASH_SendByte(W25X_JedecDeviceID);
  /* ��ȡһ���ֽ�*/
  Temp0 = SPI_FLASH_SendByte(Dummy_Byte);
  /* ��ȡһ���ֽ�*/
  Temp1 = SPI_FLASH_SendByte(Dummy_Byte);
   /* ��ȡһ���ֽ�*/
  Temp2 = SPI_FLASH_SendByte(Dummy_Byte);
  /*ʧ��Ƭѡ*/
  SPI_FLASH_CS_HIGH();
  Temp = (Temp0 << 16) | (Temp1 << 8) | Temp2;
  return Temp;
}
/******************************************************************************************
*��������SPI_FLASH_ReadDeviceID()
* ������void
* ����ֵ��u32 �豸ID
* ���ܣ�SPIFLASH��ȡ�豸ID�������ⲿ����
*********************************************************************************************/
u32 SPI_FLASH_ReadDeviceID(void)
{
  u32 Temp = 0;
   /* ʹ��Ƭѡ */
  SPI_FLASH_CS_LOW();
  /*���Ͷ�ȡIDָ��*/
  SPI_FLASH_SendCommandAddress(W25X_DeviceID, 0UL);
  /*��ȡ8λ����*/
  Temp = SPI_FLASH_SendByte(Dummy_Byte);
  /*ʧ��Ƭѡ*/
  SPI_FLASH_CS_HIGH();
  return Temp;
}

static void SPI_FLASH_WaitForFlag(uint8_t flag)
{
  u16 tick = 0;
  u8 FLASH_Status = 0;
  do
  {
     /* ʹ��Ƭѡ */
    SPI_FLASH_CS_LOW();
    /*���Ͷ�״ָ̬�� */
    SPI_FLASH_SendByte(W25X_ReadStatusReg);
    /*ѭ�����Ϳ�����ֱ��FLASHоƬ����*/
    /* ���Ϳ��ֽ� */
    FLASH_Status = SPI_FLASH_SendByte(Dummy_Byte);
    tick++;
    /*ʧ��Ƭѡ*/
    SPI_FLASH_CS_HIGH();
  }while ((FLASH_Status & flag) == flag); /* ����Ƿ����*/

  PRINTF("operation takes %d ticks\n", tick);
}

/******************************************************************************************
*��������SPI_FLASH_WriteEnable()
* ������void
* ����ֵ��void
* ���ܣ�SPIFLASHдʹ�ܺ������ⲿ����
*********************************************************************************************/
void SPI_FLASH_WriteEnable(void)
{
   /* ʹ��Ƭѡ */
  SPI_FLASH_CS_LOW();
  /*����дʹ��ָ��*/
  SPI_FLASH_SendByte(W25X_WriteEnable);
  /*ʧ��Ƭѡ*/
  SPI_FLASH_CS_HIGH();

  /*check WEL */
  //SPI_FLASH_WaitForFlag(WEL_Flag);
}

/******************************************************************************************
*��������SPI_FLASH_WaitForWriteEnd()
* ������void
* ����ֵ��void
* ���ܣ�SPIFLASH�ȴ�д��Ϻ������ⲿ����
*********************************************************************************************/
void SPI_FLASH_WaitForWriteEnd(void)
{
  SPI_FLASH_WaitForFlag(WIP_Flag);
}

/******************************************************************************************
*��������SPI_Flash_PowerDown()
* ������void
* ����ֵ��void
* ���ܣ�SPIFLASH�������ģʽ�������ⲿ����
*********************************************************************************************/
void SPI_Flash_PowerDown(void)   
{ 
  /* ʹ��Ƭѡ */
  SPI_FLASH_CS_LOW();
  /*���͵���ָ�� */
  SPI_FLASH_SendByte(W25X_PowerDown);
  /*ʧ��Ƭѡ*/
  SPI_FLASH_CS_HIGH();
}   

/******************************************************************************************
*��������SPI_Flash_WAKEUP()
* ������void
* ����ֵ��void
* ���ܣ�SPIFLASH���ѵ���ģʽ�������ⲿ����
*********************************************************************************************/
void SPI_Flash_WAKEUP(void)   
{
  /* ʹ��Ƭѡ */
  SPI_FLASH_CS_LOW();
  /* �����˳�����ģʽָ�� */
  SPI_FLASH_SendByte(W25X_ReleasePowerDown);
  /*ʧ��Ƭѡ*/
  SPI_FLASH_CS_HIGH();              
}  

/******************************************************************************************
*��������SPI_Flash_Reset()
* ������void
* ����ֵ��void
* ���ܣ�SPIFLASH Reset�������ⲿ����
*********************************************************************************************/
void SPI_Flash_Reset(void)   
{
  /* ʹ��Ƭѡ */
  SPI_FLASH_CS_LOW();
  /* �����˳�����ģʽָ�� */
  SPI_FLASH_SendByte(W25X_EnableReset);
  /*ʧ��Ƭѡ*/
  SPI_FLASH_CS_HIGH();
  /* ʹ��Ƭѡ */
  SPI_FLASH_CS_LOW();
  /* �����˳�����ģʽָ�� */
  SPI_FLASH_SendByte(W25X_Reset);
  /*ʧ��Ƭѡ*/
  SPI_FLASH_CS_HIGH();
  
}  

void SPI_FLASH_Init(void)
{
  // init SPI
  UCA1CTL1 = UCSWRST;

  UCA1CTL0 |= UCMST + UCSYNC + UCMSB + UCCKPL; // master, 3-pin SPI mode, LSB //UCCKPH
  UCA1CTL1 |= UCSSEL__SMCLK; // SMCLK for now
  UCA1BR0 = 4; // 8MHZ / 4 = 2Mhz
  UCA1BR1 = 0;
  UCA1MCTL = 0;

  //Configure ports.
  CLKDIR |= CLKPIN;
  CLKSEL |= CLKPIN;
  CLKOUT &= ~CLKPIN;

  SPIDIR |= SIPIN | CSPIN;
  SPIDIR &= ~SOPIN; // SO is input
  SPISEL |= SIPIN | SOPIN;
  SPIOUT &= ~SIPIN;
  SPIOUT |= CSPIN; // pull CS high to disable chip

  //SPI_Flash_Reset();

  printf("\n[!][SPIFLASH] init OK\n");
#if 1
  uint8_t FLASH_Status;
  SPI_FLASH_CS_LOW();
  /*���Ͷ�״ָ̬�� */
  SPI_FLASH_SendByte(W25X_ReadStatusReg);
  /* ���Ϳ��ֽ� */
  FLASH_Status = SPI_FLASH_SendByte(Dummy_Byte);
  /*ʧ��Ƭѡ*/
  SPI_FLASH_CS_HIGH();
  printf("\n[*][SPIFLASH] status register 1 = %x \n", FLASH_Status);

  SPI_FLASH_CS_LOW();
  /*���Ͷ�״ָ̬�� */
  SPI_FLASH_SendByte(W25X_ReadStatusReg2);
  /* ���Ϳ��ֽ� */
  FLASH_Status = SPI_FLASH_SendByte(Dummy_Byte);
  /*ʧ��Ƭѡ*/
  SPI_FLASH_CS_HIGH();
  printf("\n[*][SPIFLASH] status register 2 = %x \n", FLASH_Status);  
#endif
  printf("\n[*][SPIFLASH] Found SPI Flash DeviceId = %x\n", (uint16_t)SPI_FLASH_ReadDeviceID());

//  SPI_FLASH_BulkErase();
}
