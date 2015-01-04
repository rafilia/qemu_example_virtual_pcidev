// qemu test PCI virtual device

#include "hw/hw.h"
#include "hw/pci/pci.h"

#include <stdlib.h>
#include "../../../custom_device/test_pci/test_pci.h"

#define TEST_PCI_DEVICE_DEBUG

#ifdef  TEST_PCI_DEVICE_DEBUG
#define tprintf(fmt, ...) printf("## (%3d) %-20s: " fmt, __LINE__, __func__, ## __VA_ARGS__)
#else
#define tprintf(fmt, ...)
#endif

typedef struct TestPCIState {
    PCIDevice parent_obj;

		MemoryRegion mmio;
		MemoryRegion portio;

		char piodata[TEST_PIO_DATASIZE]; 
		char mmiodata[TEST_MMIO_DATASIZE]; 

		// for DMA operation
		dma_addr_t cdma_addr, sdma_addr;
		int cdma_len, sdma_len;
		int cdma_buf[TEST_CDMA_BUFFER_NUM], sdma_buf[TEST_SDMA_BUFFER_NUM];

		char intmask;
} TestPCIState;

#define TYPE_TEST_PCI "test_pci"

#define TEST_PCI(obj) \
    OBJECT_CHECK(TestPCIState, (obj), TYPE_TEST_PCI)

// when processing complete, raise irq line
void test_do_something(TestPCIState *s);
void test_do_something(TestPCIState *s) 
{
	PCIDevice *pdev = PCI_DEVICE(s);
	
	tprintf("called\n");

	s->intmask |= INT_DO;
	// raise irq line
	pci_irq_assert(pdev);
}

void test_show_cdmabuf(TestPCIState *s);
void test_show_cdmabuf(TestPCIState *s) 
{
	PCIDevice *pci_dev = PCI_DEVICE(s);
	int i;
	
	for (i = 0; i < TEST_CDMA_BUFFER_NUM; i++) {
		printf("%3d ", s->cdma_buf[i]++);
	}
	printf("\n");

	pci_dma_write(pci_dev,  s->cdma_addr, s->cdma_buf, s->cdma_len);

	s->intmask |= INT_CDMA;
	pci_irq_assert(pci_dev);
}

int comp_int(const void* a, const void*b);
int comp_int(const void* a, const void*b)
{
	return *((int *)a) - *((int *)b);
}

void test_show_sdmabuf(TestPCIState *s);
void test_show_sdmabuf(TestPCIState *s) 
{
	PCIDevice *pci_dev = PCI_DEVICE(s);
	int i;
	
	for (i = 0; i < TEST_SDMA_BUFFER_NUM; i++) {
		printf("%3d ", s->sdma_buf[i]);
	}
	printf("\n");
	qsort(s->sdma_buf, TEST_SDMA_BUFFER_NUM, sizeof(int), comp_int);
	pci_dma_write(pci_dev,  s->sdma_addr, s->sdma_buf, s->sdma_len);

	s->intmask |= INT_SDMA;
	pci_irq_assert(pci_dev);
}

void test_down_irq(TestPCIState *s);
void test_down_irq(TestPCIState *s) 
{
	PCIDevice *pdev = PCI_DEVICE(s);
	
	// down irq line
	pci_irq_deassert(pdev);
}

static uint64_t
test_pci_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    TestPCIState *s = opaque;
		tprintf("addr %ld, size %d\n", addr, size);

		if(addr >= TEST_MEM && addr < TEST_MEM + TEST_MMIO_DATASIZE){
			return s->mmiodata[addr>>2]; // 4byte
		}

    return 0xaabbccdd;
}

static uint64_t
test_pci_pio_read(void *opaque, hwaddr addr, unsigned size)
{
    TestPCIState *s = opaque;
		tprintf("addr %ld, size %d\n", addr, size);

		if(addr < TEST_PIO_DATASIZE) {
			return s->piodata[addr];
		} else {
			switch(addr){
				case TEST_GET_INTMASK:
					return s->intmask;

				default:
					return 0x12345678;
			}
		}
}

static void
test_pci_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                       unsigned size)
{
    TestPCIState *s = opaque;

		tprintf("addr %ld, size %d\n", addr, size);
		
		if(addr >= TEST_MEM && addr < TEST_MEM + TEST_MMIO_DATASIZE){
			s->mmiodata[addr>>2] = val; // 4byte
		}
}

static void
test_pci_pio_write(void *opaque, hwaddr addr, uint64_t val,
                       unsigned size)
{
    TestPCIState *s = opaque;
		PCIDevice *pdev = PCI_DEVICE(s);

		// printf("%s : addr %ld, size %d\n", __func__ , addr, size);
	
		tprintf("addr %ld, size %d\n", addr, size);

		if(addr >= TEST_PIO && addr < TEST_PIO_DATASIZE) {
			s->piodata[addr] = (char)val;
		} else {
			switch (addr) {
				case TEST_DO:
					test_do_something(s);
					break;

				case TEST_SET_INTMASK:
					s->intmask = val;
					break;

				case TEST_DOWN_IRQ:
					test_down_irq(s);
					break;

				// coherent DMA
				case TEST_SET_CDMA_ADDR:
					s->cdma_addr = val;
					break;
				case TEST_SET_CDMA_LEN:
					s->cdma_len = val;
					break;
				case TEST_CDMA_START:
					pci_dma_read(pdev, s->cdma_addr, s->cdma_buf, s->cdma_len);
					test_show_cdmabuf(s);
					break;

				// streaming DMA
				case TEST_SET_SDMA_ADDR:
					s->sdma_addr = val;
					break;
				case TEST_SET_SDMA_LEN:
					s->sdma_len = val;
					break;
				case TEST_SDMA_START:
					pci_dma_read(pdev, s->sdma_addr, s->sdma_buf, s->sdma_len);
					test_show_sdmabuf(s);
					break;

				default:
					;
			}
		}
}

static const MemoryRegionOps test_pci_mmio_ops = {
    .read = test_pci_mmio_read,
    .write = test_pci_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static const MemoryRegionOps test_pci_pio_ops = {
    .read = test_pci_pio_read,
    .write = test_pci_pio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static int test_pci_init(PCIDevice *pdev)
{
    TestPCIState *s = TEST_PCI(pdev);
    uint8_t *pci_conf;
		int i;
  
    pci_conf = pdev->config;
    pci_conf[PCI_INTERRUPT_PIN] = 1; /* if 0 no interrupt pin */

		// register memory mapped io / port mapped io
    memory_region_init_io(&s->mmio, OBJECT(s), &test_pci_mmio_ops,  s,
                          "test_pci_mmio", TEST_PCI_MEMSIZE * 2);
    memory_region_init_io(&s->portio, OBJECT(s), &test_pci_pio_ops, s,
                          "test_pci_portio", TEST_PCI_IOSIZE * 2);

		// set base address register 
    pci_register_bar(pdev, BAR_MMIO, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio);
    pci_register_bar(pdev, BAR_PIO , PCI_BASE_ADDRESS_SPACE_IO    , &s->portio);

		// init local parameter
		for(i=0; i<TEST_PIO_DATASIZE; i++){
			s->piodata[i] = i;
		}
		for(i=0; i<TEST_MMIO_DATASIZE; i++){
			s->mmiodata[i] = i;
		}

    tprintf("loaded\n");
    return 0;
}

static void
test_pci_uninit(PCIDevice *pdev)
{
    TestPCIState *s = TEST_PCI(pdev);

		memory_region_destroy(&s->mmio);
		memory_region_destroy(&s->portio);

    tprintf("unloaded\n");
}

static void
test_pci_reset(TestPCIState *s)
{
    tprintf("done reset\n");
}

static void qdev_test_pci_reset(DeviceState *ds)
{
		TestPCIState *s = TEST_PCI(ds);
		test_pci_reset(s);
}

static void test_pci_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->init = test_pci_init;
    k->exit = test_pci_uninit;
    k->vendor_id = PCI_VENDOR_ID_TEST;
    k->device_id = PCI_DEVICE_ID_TEST;
    k->revision = 0x00;
    k->class_id = PCI_CLASS_OTHERS;
    dc->desc = "Test PCI Virtual Device";
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
    dc->reset = qdev_test_pci_reset;
}

static const TypeInfo test_pci_info = {
    .name          = TYPE_TEST_PCI,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(TestPCIState),
    .class_init    = test_pci_class_init,
};

static void test_pci_register_types(void)
{
    type_register_static(&test_pci_info);
}

type_init(test_pci_register_types)
