/*
 * Test PCI Virtual Device
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#include "hw/hw.h"
#include "hw/pci/pci.h"
#include "qemu/event_notifier.h"
#include "qemu/osdep.h"

#include "../../../custom_device/hello_pci/hello_pci_device.h"

typedef struct HelloPCIState {
    PCIDevice parent_obj;

		MemoryRegion mmio;
		MemoryRegion portio;

		int data;

		// for DMA operation
		dma_addr_t dma_addr, sdma_addr;
		int dma_len, sdma_len;
		int dma_buf[HELLO_DMA_BUFFER_SIZE/sizeof(int)];
} HelloPCIState;

#define TYPE_HELLO_PCI "hello_pci"

#define HELLO_PCI(obj) \
    OBJECT_CHECK(HelloPCIState, (obj), TYPE_HELLO_PCI)


static uint64_t
hello_pci_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    HelloPCIState *d = opaque;
		printf("%s : addr %ld, size %d\n", __func__,   addr, size);

		if(addr >= HELLO_MEMOFFSET && addr < HELLO_MEMOFFSET + DATANUM*sizeof(int)){
			return d->data + addr-HELLO_MEMOFFSET;
		}

    return 0xaabbccdd;
}

static uint64_t
hello_pci_pio_read(void *opaque, hwaddr addr, unsigned size)
{
    HelloPCIState *d = opaque;
		printf("%s : addr %ld, size %d\n", __func__ , addr, size);

		if(addr == HELLO_PIOOFFSET){
			return d->data;
		}

    return 0x12345678;
}

/* static void */
/* hello_pci_write(void *opaque, hwaddr addr, uint64_t val, */
/*                   unsigned size, int type) */
/* { */
/*     HelloPCIState *d = opaque; */
/*  */
/* 		printf("%s : addr %lld, size %d, type:%d\n", __func__ , addr, size, type); */
/* 		if(addr = HELLO_PIOOFFSET){ */
/* 			d->data = val; */
/* 		} */
/* } */

// when processing complete, raise irq line
void hello_do_something(HelloPCIState *d);
void hello_do_something(HelloPCIState *d) 
{
	PCIDevice *pci_dev = PCI_DEVICE(d);
	
	// raise irq line
	pci_irq_assert(pci_dev);
}

void hello_show_dmabuf(HelloPCIState *d);
void hello_show_dmabuf(HelloPCIState *d) 
{
	PCIDevice *pci_dev = PCI_DEVICE(d);
	int i;
	
	for (i = 0; i < HELLO_DMA_BUFFER_SIZE/sizeof(int); i++) {
		printf("%4d ", d->dma_buf[i]++);
	}
	printf("\n");

	pci_dma_write(pci_dev,  d->dma_addr, d->dma_buf, d->dma_len);

	pci_irq_assert(pci_dev);
}

void hello_show_sdmabuf(HelloPCIState *d);
void hello_show_sdmabuf(HelloPCIState *d) 
{
	PCIDevice *pci_dev = PCI_DEVICE(d);
	int i;
	
	for (i = 0; i < HELLO_DMA_BUFFER_SIZE/sizeof(int); i++) {
		printf("%4d ", d->dma_buf[i]++);
	}
	printf("\n");

	pci_dma_write(pci_dev,  d->sdma_addr, d->dma_buf, d->sdma_len);

	pci_irq_assert(pci_dev);
}

void hello_down_irq(HelloPCIState *d);
void hello_down_irq(HelloPCIState *d) 
{
	PCIDevice *pci_dev = PCI_DEVICE(d);
	
	// down irq line
	pci_irq_deassert(pci_dev);
}

static void
hello_pci_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                       unsigned size)
{
    HelloPCIState *d = opaque;

		printf("%s : addr %ld, size %d\n", __func__ , addr, size);
		
		if(addr == HELLO_MEMOFFSET){
			d->data = val;
		}
}

static void
hello_pci_pio_write(void *opaque, hwaddr addr, uint64_t val,
                       unsigned size)
{
    HelloPCIState *d = opaque;
		PCIDevice *pdev = PCI_DEVICE(d);

		printf("%s : addr %ld, size %d\n", __func__ , addr, size);

		switch (addr) {
			case HELLO_PIOOFFSET :
				d->data = val;
				break;
			case HELLO_DOOFFSET:
				hello_do_something(d);
				break;

			case HELLO_DOWN_IRQ_OFFSET:
				hello_down_irq(d);
				break;

			// coherent DMA
			case HELLO_SET_DMA_ADDR:
				d->dma_addr = val;
				break;
			case HELLO_SET_DMA_LEN:
				d->dma_len = val;
				break;
			case HELLO_DMA_START:
				pci_dma_read(pdev, d->dma_addr, d->dma_buf, d->dma_len);
				hello_show_dmabuf(d);

			// streaming DMA
			case HELLO_SET_SDMA_ADDR:
				d->sdma_addr = val;
				break;
			case HELLO_SET_SDMA_LEN:
				d->sdma_len = val;
				break;
			case HELLO_SDMA_START:
				pci_dma_read(pdev, d->sdma_addr, d->dma_buf, d->sdma_len);
				hello_show_sdmabuf(d);

			default:
				;
		}

		/* if(addr == HELLO_PIOOFFSET){ */
		/* 	d->data = val; */
		/* } */
}

static const MemoryRegionOps hello_pci_mmio_ops = {
    .read = hello_pci_mmio_read,
    .write = hello_pci_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static const MemoryRegionOps hello_pci_pio_ops = {
    .read = hello_pci_pio_read,
    .write = hello_pci_pio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static int hello_pci_init(PCIDevice *pci_dev)
{
    HelloPCIState *d = HELLO_PCI(pci_dev);
    uint8_t *pci_conf;
  
    pci_conf = pci_dev->config;
    pci_conf[PCI_INTERRUPT_PIN] = 1; /* if 0 no interrupt pin */

		// register io map/port
    memory_region_init_io(&d->mmio, OBJECT(d), &hello_pci_mmio_ops,  d,
                          "hello_pci_mmio", HELLO_PCI_MEMSIZE * 2);
    memory_region_init_io(&d->portio, OBJECT(d), &hello_pci_pio_ops, d,
                          "hello_pci_portio", HELLO_PCI_IOSIZE * 2);

		// set base address register 
    pci_register_bar(pci_dev, BAR_MMIO, PCI_BASE_ADDRESS_SPACE_MEMORY, &d->mmio);
    pci_register_bar(pci_dev, BAR_PIO , PCI_BASE_ADDRESS_SPACE_IO    , &d->portio);

		// init local parameter
		d->data = 0x89abcdef;

    printf("hello_pci: loaded\n");
    return 0;
}

static void
hello_pci_uninit(PCIDevice *dev)
{
    HelloPCIState *d = HELLO_PCI(dev);

		memory_region_destroy(&d->mmio);
		memory_region_destroy(&d->portio);

    printf("hello_pci: unloaded\n");
}

static void
hello_pci_reset(HelloPCIState *d)
{
    printf("hello_pci: reset\n");
}

static void qdev_pci_hello_reset(DeviceState *dev)
{
		HelloPCIState *d = HELLO_PCI(dev);
		hello_pci_reset(d);
}

static void hello_pci_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->init = hello_pci_init;
    k->exit = hello_pci_uninit;
    k->vendor_id = PCI_VENDOR_ID_HELLO;
    k->device_id = PCI_DEVICE_ID_HELLO;
    k->revision = 0x00;
    k->class_id = PCI_CLASS_OTHERS;
    dc->desc = "Hello PCI Virtual Device";
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
    dc->reset = qdev_pci_hello_reset;
}

static const TypeInfo hello_pci_info = {
    .name          = TYPE_HELLO_PCI,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(HelloPCIState),
    .class_init    = hello_pci_class_init,
};

static void hello_pci_register_types(void)
{
    type_register_static(&hello_pci_info);
}

type_init(hello_pci_register_types)
