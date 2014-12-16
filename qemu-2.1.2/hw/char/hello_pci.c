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

#define HELLO_PCI_IOSIZE 128
#define HELLO_PCI_MEMSIZE 2048

#define PCI_VENDOR_ID_HELLO 0x7000
#define PCI_DEVICE_ID_HELLO 0x0001

#define BAR_MMIO 0
#define BAR_PIO  1

static const char *iotest_type[] = {
    "mmio",
    "portio"
};

typedef struct HelloPCIState {
    PCIDevice parent_obj;

		MemoryRegion mmio;
		MemoryRegion portio;
		int data;
} HelloPCIState;

#define TYPE_HELLO_PCI "hello_pci"

#define HELLO_PCI(obj) \
    OBJECT_CHECK(HelloPCIState, (obj), TYPE_HELLO_PCI)

#define PIO_HELLO_OFFSET 40

static uint64_t
hello_pci_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    HelloPCIState *d = opaque;
		printf("%s : addr %lld, size %d\n", __func__,   addr, size);

    return 0x12345678;
}

static uint64_t
hello_pci_pio_read(void *opaque, hwaddr addr, unsigned size)
{
    HelloPCIState *d = opaque;
		printf("%s : addr %lld, size %d\n", __func__ , addr, size);

		if(addr == PIO_HELLO_OFFSET){
			return d->data;
		}

    return 0x12345678;
}

static void
hello_pci_write(void *opaque, hwaddr addr, uint64_t val,
                  unsigned size, int type)
{
    HelloPCIState *d = opaque;

		printf("%s : addr %lld, size %d, type:%d\n", __func__ , addr, size, type);
		if(addr = PIO_HELLO_OFFSET){
			d->data = val;
		}
}

static void
hello_pci_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                       unsigned size)
{
    hello_pci_write(opaque, addr, val, size, 0);
}

static void
hello_pci_pio_write(void *opaque, hwaddr addr, uint64_t val,
                       unsigned size)
{
		printf("%s\n", __func__ );
    hello_pci_write(opaque, addr, val, size, 1);
}

static const MemoryRegionOps hello_pci_mmio_ops = {
    .read = hello_pci_mmio_read,
    .write = hello_pci_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
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
    pci_conf[PCI_INTERRUPT_PIN] = 0; /* no interrupt pin */

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
