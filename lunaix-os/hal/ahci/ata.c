#include <hal/ahci/ahci.h>
#include <hal/ahci/hba.h>
#include <hal/ahci/sata.h>

#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>

void
sata_read_error(struct hba_port* port)
{
    uint32_t tfd = port->regs[HBA_RPxTFD];
    port->device->last_result.sense_key = (tfd & 0xf000) >> 12;
    port->device->last_result.error = (tfd & 0x0f00) >> 8;
    port->device->last_result.status = tfd & 0x00ff;
}

int
__sata_buffer_io(struct hba_device* dev,
                 uint64_t lba,
                 void* buffer,
                 uint32_t size,
                 int write)
{
    assert_msg(((uintptr_t)buffer & 0x3) == 0, "HBA: Bad buffer alignment");

    struct hba_port* port = dev->port;
    struct hba_cmdh* header;
    struct hba_cmdt* table;
    int slot = hba_prepare_cmd(port, &table, &header, buffer, size);

    header->options |= HBA_CMDH_WRITE * (write == 1);

    uint16_t count = ICEIL(size, port->device->block_size);
    struct sata_reg_fis* fis = (struct sata_reg_fis*)table->command_fis;

    if ((port->device->flags & HBA_DEV_FEXTLBA)) {
        // 如果该设备支持48位LBA寻址
        sata_create_fis(
          fis, write ? ATA_WRITE_DMA_EXT : ATA_READ_DMA_EXT, lba, count);
    } else {
        sata_create_fis(fis, write ? ATA_WRITE_DMA : ATA_READ_DMA, lba, count);
    }
    /*
          确保我们使用的是LBA寻址模式
          注意：在ACS-3中（甚至在ACS-4），只有在(READ/WRITE)_DMA_EXT指令中明确注明了需要将这一位置位
        而并没有在(READ/WRITE)_DMA注明。
          但是这在ACS-2中是有的！于是这也就导致了先前的测试中，LBA=0根本无法访问，因为此时
        的访问模式是在CHS下，也就是说LBA=0 => Sector=0，是非法的。
          所以，我猜测，这要么是QEMU/VirtualBox根据ACS-2来编写的AHCI模拟，
        要么是标准出错了（毕竟是working draft）
    */
    fis->dev = (1 << 6);

    int is_ok = ahci_try_send(port, slot);

    vfree_dma(table);
    return is_ok;
}

int
sata_read_buffer(struct hba_device* dev,
                 uint64_t lba,
                 void* buffer,
                 uint32_t size)
{
    return __sata_buffer_io(dev, lba, buffer, size, 0);
}

int
sata_write_buffer(struct hba_device* dev,
                  uint64_t lba,
                  void* buffer,
                  uint32_t size)
{
    return __sata_buffer_io(dev, lba, buffer, size, 1);
}
