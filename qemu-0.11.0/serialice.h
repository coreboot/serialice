#ifndef HW_SERIALICE_H
#define HW_SERIALICE_H

#include "config-host.h"

extern const char *serialice_device;
extern int serialice_active;

void serialice_init(void);
void serialice_exit(void);

uint8_t serialice_inb(uint16_t port);
uint16_t serialice_inw(uint16_t port);
uint32_t serialice_inl(uint16_t port);

void serialice_outb(uint8_t data, uint16_t port);
void serialice_outw(uint16_t data, uint16_t port);
void serialice_outl(uint32_t data, uint16_t port);

uint8_t serialice_readb(uint32_t addr);
uint16_t serialice_readw(uint32_t addr);
uint32_t serialice_readl(uint32_t addr);

void serialice_writeb(uint8_t data, uint32_t addr);
void serialice_writew(uint16_t data, uint32_t addr);
void serialice_writel(uint32_t data, uint32_t addr);

uint64_t serialice_rdmsr(uint32_t addr);
void serialice_wrmsr(uint64_t data, uint32_t addr);

typedef struct {
	uint32_t eax, ebx, ecx, edx;
} cpuid_regs_t;


cpuid_regs_t serialice_cpuid(uint32_t eax, uint32_t ecx);

int serialice_handle_load(uint32_t addr, uint32_t *result, unsigned int data_size);
void serialice_log_load(int caught, uint32_t addr, uint32_t result, unsigned int data_size);
int serialice_handle_store(uint32_t addr, uint32_t val, unsigned int data_size);

#endif
