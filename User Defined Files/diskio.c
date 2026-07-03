#include "diskio.h"
#include "sd_spi.h"
#include "ff.h"
//#include "usblib/usbmsc.h"
#include "usblib/host/usbhmsc.h"

extern tUSBHMSCInstance *g_psMSCInstance;

static DSTATUS Stat = STA_NOINIT;

static DSTATUS USBStat = STA_NOINIT;

DSTATUS disk_initialize(BYTE drv)
{
    switch(drv)
    {
        case 0:     /* SD */
        {
            if(SD_Init() == 0)
                Stat &= ~STA_NOINIT;
            else
                Stat = STA_NOINIT;

            return Stat;
        }

        case 1:     /* USB */
        {
            USBStat |= STA_NOINIT;

            if(USBHMSCDriveReady(g_psMSCInstance))
                return USBStat;

            USBStat &= ~STA_NOINIT;

            return USBStat;
        }
    }

    return STA_NOINIT;
}

DSTATUS disk_status(BYTE drv)
{
    switch(drv)
    {
        case 0:
            return Stat;

        case 1:
            return USBStat;
    }

    return STA_NOINIT;
}

DRESULT disk_read(BYTE drv,
                  BYTE *buff,
                  LBA_t sector,
                  UINT count)
{
    switch(drv)
    {
        case 0:     /* SD */
        {
            for(UINT i=0;i<count;i++)
            {
                if(SD_ReadBlock(sector+i,
                                buff+(512*i)) != 0)
                    return RES_ERROR;
            }

            return RES_OK;
        }

        case 1:     /* USB */
        {
            if(USBStat & STA_NOINIT)
                return RES_NOTRDY;

            if(USBHMSCBlockRead(g_psMSCInstance,
                                sector,
                                buff,
                                count) == 0)
                return RES_OK;

            return RES_ERROR;
        }
    }

    return RES_PARERR;
}

DRESULT disk_write(BYTE drv,
                   const BYTE *buff,
                   LBA_t sector,
                   UINT count)
{
    switch(drv)
    {
        case 0:
        {
            for(UINT i=0;i<count;i++)
            {
                if(SD_WriteBlock(sector+i,
                                 buff+(512*i)) != 0)
                    return RES_ERROR;
            }

            return RES_OK;
        }

        case 1:
        {
            if(USBStat & STA_NOINIT)
                return RES_NOTRDY;

            if(USBStat & STA_PROTECT)
                return RES_WRPRT;

            if(USBHMSCBlockWrite(g_psMSCInstance,
                                 sector,
                                 (BYTE*)buff,
                                 count) == 0)
                return RES_OK;

            return RES_ERROR;
        }
    }

    return RES_PARERR;
}

DRESULT disk_ioctl(BYTE drv, BYTE cmd, void *buff)
{
    switch(drv)
    {
        case 0:     /* SD Card */
        {
            switch(cmd)
            {
                case GET_SECTOR_SIZE:
                    *(WORD*)buff = 512;
                    return RES_OK;

                case GET_BLOCK_SIZE:
                    *(DWORD*)buff = 1;
                    return RES_OK;

                case CTRL_SYNC:
                    return RES_OK;
            }

            return RES_PARERR;
        }

        case 1:     /* USB MSC */
        {
            if(USBStat & STA_NOINIT)
                return RES_NOTRDY;

            switch(cmd)
            {
                case CTRL_SYNC:
                    return RES_OK;

                case GET_SECTOR_SIZE:
                    *(WORD*)buff = 512;
                    return RES_OK;

                case GET_BLOCK_SIZE:
                    *(DWORD*)buff = 1;
                    return RES_OK;
            }

            return RES_PARERR;
        }
    }

    return RES_PARERR;
}
