/////////////////////////////////////////////////////
//////////////////// EEPROM /////////////////////////
/////////////////////////////////////////////////////
//void I2C0_Init(void)
//{
//    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);
//    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
//
//    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_I2C0));
//
//    GPIOPinConfigure(GPIO_PB2_I2C0SCL);
//    GPIOPinConfigure(GPIO_PB3_I2C0SDA);
//
//    GPIOPinTypeI2CSCL(GPIO_PORTB_BASE, GPIO_PIN_2);
//    GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_3);
//
//    I2CMasterInitExpClk(I2C0_BASE, SysCtlClockGet(), false); // 100kHz
//}
//
//void EEPROM_WriteByte(uint16_t memAddr, uint8_t data)
//{
//    while(I2CMasterBusy(I2C0_BASE));
//
//    I2CMasterSlaveAddrSet(I2C0_BASE, EEPROM_ADDR, false);
//
//    // Send High Address
//    I2CMasterDataPut(I2C0_BASE, (memAddr >> 8) & 0xFF);
//    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_START);
//    while(I2CMasterBusy(I2C0_BASE));
//
//    // Send Low Address
//    I2CMasterDataPut(I2C0_BASE, memAddr & 0xFF);
//    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_CONT);
//    while(I2CMasterBusy(I2C0_BASE));
//
//    // Send Data
//    I2CMasterDataPut(I2C0_BASE, data);
//    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
//    while(I2CMasterBusy(I2C0_BASE));
//}
//
//uint8_t EEPROM_ReadByte(uint16_t memAddr)
//{
//    uint8_t data;
//
//    while(I2CMasterBusy(I2C0_BASE));
//
//    I2CMasterSlaveAddrSet(I2C0_BASE, EEPROM_ADDR, false);
//
//    // Send High Address
//    I2CMasterDataPut(I2C0_BASE, (memAddr >> 8) & 0xFF);
//    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_START);
//    while(I2CMasterBusy(I2C0_BASE));
//
//    // Send Low Address
//    I2CMasterDataPut(I2C0_BASE, memAddr & 0xFF);
//    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
//    while(I2CMasterBusy(I2C0_BASE));
//
//    // Switch to Read Mode
//    I2CMasterSlaveAddrSet(I2C0_BASE, EEPROM_ADDR, true);
//
//    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_SINGLE_RECEIVE);
//    while(I2CMasterBusy(I2C0_BASE));
//
//    data = I2CMasterDataGet(I2C0_BASE);
//
//    return data;
//}
//
//void EEPROM_WritePage(uint16_t memAddr, volatile uint8_t *data, uint8_t len)
//{
//    uint8_t i;
//
//    while(I2CMasterBusy(I2C0_BASE));
//
//    I2CMasterSlaveAddrSet(I2C0_BASE, EEPROM_ADDR, false);
//
//    I2CMasterDataPut(I2C0_BASE, (memAddr >> 8) & 0xFF);
//    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_START);
//    while(I2CMasterBusy(I2C0_BASE));
//
//    I2CMasterDataPut(I2C0_BASE, memAddr & 0xFF);
//    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_CONT);
//    while(I2CMasterBusy(I2C0_BASE));
//
//    for(i = 0; i < len; i++)
//    {
//        I2CMasterDataPut(I2C0_BASE, data[i]);
//
//        if(i == (len - 1))
//            I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
//        else
//            I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_CONT);
//
//        while(I2CMasterBusy(I2C0_BASE));
//    }
//}
//void EEPROM_ReadPage(uint16_t pageStartAddr, volatile uint8_t *buffer, uint8_t len)
//{
//    uint8_t i;
//
//    for(i = 0; i < len; i++)
//    {
//        buffer[i] = EEPROM_ReadByte(pageStartAddr + i);
//    }
//}
//
//void EEPROM_Write_Data(){
//// -------- Byte Write --------
//    EEPROM_WriteByte(0x0000, 0x54); delay_ms(6);
//    EEPROM_WriteByte(0x0001, 0x41); delay_ms(6);
//    EEPROM_WriteByte(0x0002, 0x4E); delay_ms(6);
//    EEPROM_WriteByte(0x0003, 0x55); delay_ms(6);
//    EEPROM_WriteByte(0x0004, 0x4A); delay_ms(6);
//    EEPROM_WriteByte(0x0005, 0x20); delay_ms(6);
//    EEPROM_WriteByte(0x0006, 0x53); delay_ms(6);
//    EEPROM_WriteByte(0x0007, 0x4F); delay_ms(6);
//    EEPROM_WriteByte(0x0008, 0x4E); delay_ms(6);
//    EEPROM_WriteByte(0x0009, 0x49); delay_ms(6);
//    EEPROM_WriteByte(0x000A, 0x00); delay_ms(6);
//    delay_ms(100);
//}
//void EEPROM_Read_Data(){
//    // -------- Byte Read --------
//    EEPROM_ReadByte(0x0000); delay_ms(6);
//    EEPROM_ReadByte(0x0001); delay_ms(6);
//    EEPROM_ReadByte(0x0002); delay_ms(6);
//    EEPROM_ReadByte(0x0003); delay_ms(6);
//    EEPROM_ReadByte(0x0004); delay_ms(6);
//    EEPROM_ReadByte(0x0005); delay_ms(6);
//    EEPROM_ReadByte(0x0006); delay_ms(6);
//    EEPROM_ReadByte(0x0007); delay_ms(6);
//    EEPROM_ReadByte(0x0008); delay_ms(6);
//    EEPROM_ReadByte(0x0009); delay_ms(6);
//    EEPROM_ReadByte(0x000A); delay_ms(6);
//}
