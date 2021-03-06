#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "memory.h"
#include "io_regs.h"
#include "screen.h"
#include "keypad.h"
#include "rom.h"
#include "timer.h"

// temporary
#include <assert.h>
#include "lr35902.h"

static uint8_t mm[MEM_SIZE];

static uint8_t *bios;
static uint8_t bios_enabled;
static unsigned int bios_size;
// Maybe add mem_init to set the pointers to NULL???

void mem_init() {
	bios = NULL;
	bios_enabled = 0;
	bios_size = 0;
}

void mem_load(uint16_t addr, uint8_t *buf, unsigned int size) {
	memcpy(&mm[addr], buf, size);
}

/*
void mem_set_rom(uint8_t *r, unsigned int size) {
	rom = r;
	rom_size = size;
	//printf("ROM JUST SET size: %d\n", rom_size);
	//printf("%02x %02x %02x %02x\n", rom[0], rom[1], rom[2], rom[3]);
}

void mem_unset_rom() {
	if (rom != NULL) {
		free(rom);
	} else {
		printf("Error, no set rom to unset!!!\n");
	}
}

void mem_load_rom() {
	//mem_load(0, rom, rom_size);
	mem_load(0, rom, 0x8000);
}
*/
void mem_set_bios(uint8_t * b, unsigned int size) {
	bios = b;
	bios_size = size;
}

void mem_enable_bios() {
	//mem_load(0, bios, bios_size);
	//printf("BIOS JUST ENABLED size: %d\n", bios_size);
	//mem_dump(0x0, 0x200);
	bios_enabled = 1;
}

void mem_disable_bios() {
	//mem_load(0, rom, bios_size);
	bios_enabled = 0;
}

uint8_t *mem_get_mem() {
	return mm;
}

void mem_bit_set(uint16_t addr, uint8_t bit) {
	mm[addr] |= bit;
}

void mem_bit_unset(uint16_t addr, uint8_t bit) {
	mm[addr] &= ~bit;
}

uint8_t mem_bit_test(uint16_t addr, uint8_t bit) {
	return ((mm[addr] & bit) > 0);
}

//typedef uint8_t (*fptr_mem_read_8)(uint16_t addr);
//typedef void (*fptr_mem_write_8)(uint16_t addr, uint8_t v);

mem_map_t mem_map(uint16_t addr) {
	if (addr < 0x100 && bios_enabled) {
		return MEM_MAP_BIOS;
	}
	if (addr < 0x8000) {
		return MEM_MAP_ROM;
	}
	if (addr < 0xA000) {
		return MEM_MAP_VRAM;
	}
	if (addr < 0xC000) {
		return MEM_MAP_ROM_RAM;
	}
	if (addr < 0xE000) {
		return MEM_MAP_WRAM;
	}
	if (addr < 0xFE00) {
		return MEM_MAP_RAM_ECHO;
	}
	if (addr < 0xFEA0) {
		return MEM_MAP_OAM;
	}
	if (addr < 0xFF00) {
		return MEM_MAP_NOT_USABLE;
	}
	if (addr < 0xFF80) {
		return MEM_MAP_IO;
	}
	if (addr < 0xFFFF) {
		return MEM_MAP_HRAM;
	}
	return MEM_MAP_IE_REG;
}

void dma(uint16_t a) {
	int i, j;
	//uint8_t tmp;
	for (i = 0; i < 40; i++) {
		for (j = 0; j < 4; j++) {
			mm[MEM_OAM + i * 4 + j] = mm[a + i * 4 + j];
		}
		mm[MEM_OAM + i * 4 + 3] &= 0xF0;
	}
}

uint8_t mem_read_io_8(uint16_t addr) {
	switch (addr) {
	case IO_CURLINE:
	case IO_CMPLINE:
	case IO_BGRDPAL:
	case IO_OBJ0PAL:
	case IO_OBJ1PAL:
	case IO_SCROLLY:
	case IO_SCROLLX:
	case IO_WNDPOSY:
	case IO_WNDPOSX:
	case IO_LCDSTAT:
		return screen_read_8(addr);
		break;
	case IO_JOYPAD:
		return keypad_read_8(addr);
		break;
	case IO_DIVIDER:
	case IO_TIMECNT:
	case IO_TIMEMOD:
	case IO_TIMCONT:
		return timer_read_8(addr);
		break;
	case IO_IFLAGS:
		return mm[addr] |= 0xE0;
	default:
		break;

	}
	return mm[addr];
}

void mem_write_io_8(uint16_t addr, uint8_t v) {
	switch (addr) {
	case 0xFF50:
		//printf("SOMETHING HERE!!!\n");
		if (v == 1) {
			// Unmap ROM
			mem_disable_bios();
		}
		break;
	case IO_SIOCONT:
		if (v & MASK_IO_SIOCONT_Start_Flag) {
			// Set bit for transfer complete
			mm[addr] = v;
			//printf("%c", mem_read_8(IO_SIODATA));
			fprintf(stderr, "%c", mem_read_8(IO_SIODATA));
			//printf("\n");
			//assert(1 == 2);
			mem_bit_unset(IO_SIOCONT, MASK_IO_SIOCONT_Start_Flag);
			return;
		}
		break;
	case IO_DMACONT:
		//printf("DMA %02x\n", v);
		dma((uint16_t)v << 8);
		mm[addr] = v;
		break;
	case IO_CURLINE:
	case IO_CMPLINE:
	case IO_BGRDPAL:
	case IO_OBJ0PAL:
	case IO_OBJ1PAL:
	case IO_SCROLLY:
	case IO_SCROLLX:
	case IO_WNDPOSY:
	case IO_WNDPOSX:
	case IO_LCDSTAT:
		screen_write_8(addr, v);
		break;
	case IO_JOYPAD:
		keypad_write_8(addr, v);
		break;
	case IO_DIVIDER:
	case IO_TIMECNT:
	case IO_TIMEMOD:
	case IO_TIMCONT:
		timer_write_8(addr, v);
		break;
	default:
		mm[addr] = v;
	}
}

uint8_t mem_read_8(uint16_t addr) {
	if(addr == 0xFF43 && mm[addr] != 0) {
		printf("Writing SCROLLX: %d\n", mm[addr]);
	}
	switch (mem_map(addr)) {
	case MEM_MAP_BIOS:
		return bios[addr];
		break;
	case MEM_MAP_ROM:
	case MEM_MAP_ROM_RAM:
		return rom_read_8(addr);
		break;
	case MEM_MAP_RAM_ECHO:
		return mm[addr - 0x2000];
		break;
	case MEM_MAP_NOT_USABLE:
		break;
	case MEM_MAP_IO:
		return mem_read_io_8(addr);
		//printf("read io at  %4X (%2X)\n", addr, tmp);
	case MEM_MAP_WRAM:
	case MEM_MAP_VRAM:
	case MEM_MAP_OAM:
	case MEM_MAP_HRAM:
	case MEM_MAP_IE_REG:
		break;
	}
	return mm[addr];
}

uint16_t mem_read_16(uint16_t addr) {
	//return (uint16_t)mm[addr] + ((uint16_t)mm[addr + 1] << 8);
	return (uint16_t) mem_read_8(addr) +
	    ((uint16_t) mem_read_8(addr + 1) << 8);
}

void mem_write_8(uint16_t addr, uint8_t v) {
	/*if (addr == 0xFF43) {
		printf("Writing SCROLLX: %d\n", v);
	}*/
	switch (mem_map(addr)) {
	case MEM_MAP_BIOS:
		break;
	case MEM_MAP_ROM:
	case MEM_MAP_ROM_RAM:
		rom_write_8(addr, v);
		break;
	case MEM_MAP_RAM_ECHO:
		mm[addr - 2000] = v;
		break;
	case MEM_MAP_NOT_USABLE:
		break;
	case MEM_MAP_IO:
		//printf("write io at %4X (%2X)\n", addr, v);
		mem_write_io_8(addr, v);
		break;
	case MEM_MAP_WRAM:
	case MEM_MAP_VRAM:
	case MEM_MAP_OAM:
	case MEM_MAP_HRAM:
	case MEM_MAP_IE_REG:
		mm[addr] = v;
		break;
	}
	return;
}

void mem_write_16(uint16_t addr, uint16_t v) {
	//mm[addr] = (uint8_t)(v & 0x00FF);
	//mm[addr + 1] = (uint8_t)((v & 0xFF00) >> 8);
	mem_write_8(addr, (uint8_t) (v & 0x00FF));
	mem_write_8(addr + 1, (uint8_t) ((v & 0xFF00) >> 8));
}

void mem_dump(uint16_t start, uint16_t end) {
	//unsigned char *buf = (unsigned char*)&memory;
	int i, j;
	for (i = start; i <= end; i += 16) {
		printf("%04X  ", i);
		for (j = 0; j < 16; j++) {
			if (i + j <= end) {
				printf("%02x ", mem_read_8(i + j));
			} else {
				printf("   ");
			}
			if (j == 7) {
				printf(" ");
			}
		}
		printf(" |");
		for (j = 0; j < 16; j++) {
			if (i + j <= end) {
				printf("%c",
				       isprint(mem_read_8(i + j)) ?
					       mem_read_8(i + j) : '.');
			}
		}
		printf("|\n");
	}
}

char *byte2bin_str(uint8_t b, char *s) {
	int i, bi;
	for (i = 0; i < 8; i++) {
		bi = 7 - i;
		s[i] = ((b & (1 << bi)) >> bi) + 48;
	}
	s[i] = '\0';
	return s;
}

void mem_dump_io_regs() {
	char s[16];
	printf("%02X [JOYPAD]:    %02x (%s)\n",
	       IO_JOYPAD, mem_read_8(IO_JOYPAD),
		byte2bin_str(mem_read_8(IO_JOYPAD), s));
	printf("%02X [SIODATA]:   %02x (%s)\n",
	       IO_SIODATA, mem_read_8(IO_SIODATA),
		byte2bin_str(mem_read_8(IO_SIODATA), s));
	printf("%02X [SIOCONT]:   %02x (%s)\n",
	       IO_SIOCONT, mem_read_8(IO_SIOCONT),
		byte2bin_str(mem_read_8(IO_SIOCONT), s));
	printf("%02X [DIVIDER]:   %02x (%s)\n",
	       IO_DIVIDER, mem_read_8(IO_DIVIDER),
		byte2bin_str(mem_read_8(IO_DIVIDER), s));
	printf("%02X [TIMECNT]:   %02x (%s)\n",
	       IO_TIMECNT, mem_read_8(IO_TIMECNT),
		byte2bin_str(mem_read_8(IO_TIMECNT), s));
	printf("%02X [TIMEMOD]:   %02x (%s)\n",
	       IO_TIMEMOD, mem_read_8(IO_TIMEMOD),
		byte2bin_str(mem_read_8(IO_TIMEMOD), s));
	printf("%02X [TIMCONT]:   %02x (%s)\n",
	       IO_TIMCONT, mem_read_8(IO_TIMCONT),
		byte2bin_str(mem_read_8(IO_TIMCONT), s));
	printf("%02X [IFLAGS]:    %02x (%s)\n",
	       IO_IFLAGS, mem_read_8(IO_IFLAGS),
		byte2bin_str(mem_read_8(IO_IFLAGS), s));
	printf("%02X [SNDREG10]:  %02x (%s)\n",
	       IO_SNDREG10, mem_read_8(IO_SNDREG10),
		byte2bin_str(mem_read_8(IO_SNDREG10), s));
	printf("%02X [SNDREG11]:  %02x (%s)\n",
	       IO_SNDREG11, mem_read_8(IO_SNDREG11),
		byte2bin_str(mem_read_8(IO_SNDREG11), s));
	printf("%02X [SNDREG12]:  %02x (%s)\n",
	       IO_SNDREG12, mem_read_8(IO_SNDREG12),
		byte2bin_str(mem_read_8(IO_SNDREG12), s));
	printf("%02X [SNDREG13]:  %02x (%s)\n",
	       IO_SNDREG13, mem_read_8(IO_SNDREG13),
		byte2bin_str(mem_read_8(IO_SNDREG13), s));
	printf("%02X [SNDREG14]:  %02x (%s)\n",
	       IO_SNDREG14, mem_read_8(IO_SNDREG14),
		byte2bin_str(mem_read_8(IO_SNDREG14), s));
	printf("%02X [SNDREG21]:  %02x (%s)\n",
	       IO_SNDREG21, mem_read_8(IO_SNDREG21),
		byte2bin_str(mem_read_8(IO_SNDREG21), s));
	printf("%02X [SNDREG22]:  %02x (%s)\n",
	       IO_SNDREG22, mem_read_8(IO_SNDREG22),
		byte2bin_str(mem_read_8(IO_SNDREG22), s));
	printf("%02X [SNDREG23]:  %02x (%s)\n",
	       IO_SNDREG23, mem_read_8(IO_SNDREG23),
		byte2bin_str(mem_read_8(IO_SNDREG23), s));
	printf("%02X [SNDREG24]:  %02x (%s)\n",
	       IO_SNDREG24, mem_read_8(IO_SNDREG24),
		byte2bin_str(mem_read_8(IO_SNDREG24), s));
	printf("%02X [SNDREG30]:  %02x (%s)\n",
	       IO_SNDREG30, mem_read_8(IO_SNDREG30),
		byte2bin_str(mem_read_8(IO_SNDREG30), s));
	printf("%02X [SNDREG31]:  %02x (%s)\n",
	       IO_SNDREG31, mem_read_8(IO_SNDREG31),
		byte2bin_str(mem_read_8(IO_SNDREG31), s));
	printf("%02X [SNDREG32]:  %02x (%s)\n",
	       IO_SNDREG32, mem_read_8(IO_SNDREG32),
		byte2bin_str(mem_read_8(IO_SNDREG32), s));
	printf("%02X [SNDREG33]:  %02x (%s)\n",
	       IO_SNDREG33, mem_read_8(IO_SNDREG33),
		byte2bin_str(mem_read_8(IO_SNDREG33), s));
	printf("%02X [SNDREG34]:  %02x (%s)\n",
	       IO_SNDREG34, mem_read_8(IO_SNDREG34),
		byte2bin_str(mem_read_8(IO_SNDREG34), s));
	printf("%02X [SNDREG41]:  %02x (%s)\n",
	       IO_SNDREG41, mem_read_8(IO_SNDREG41),
		byte2bin_str(mem_read_8(IO_SNDREG41), s));
	printf("%02X [SNDREG42]:  %02x (%s)\n",
	       IO_SNDREG42, mem_read_8(IO_SNDREG42),
		byte2bin_str(mem_read_8(IO_SNDREG42), s));
	printf("%02X [SNDREG43]:  %02x (%s)\n",
	       IO_SNDREG43, mem_read_8(IO_SNDREG43),
		byte2bin_str(mem_read_8(IO_SNDREG43), s));
	printf("%02X [SNDREG44]:  %02x (%s)\n",
	       IO_SNDREG44, mem_read_8(IO_SNDREG44),
		byte2bin_str(mem_read_8(IO_SNDREG44), s));
	printf("%02X [SNDREG50]:  %02x (%s)\n",
	       IO_SNDREG50, mem_read_8(IO_SNDREG50),
		byte2bin_str(mem_read_8(IO_SNDREG50), s));
	printf("%02X [SNDREG51]:  %02x (%s)\n",
	       IO_SNDREG51, mem_read_8(IO_SNDREG51),
		byte2bin_str(mem_read_8(IO_SNDREG51), s));
	printf("%02X [SNDREG52]:  %02x (%s)\n",
	       IO_SNDREG52, mem_read_8(IO_SNDREG52),
		byte2bin_str(mem_read_8(IO_SNDREG52), s));
	printf("%02X [LCDCONT]:   %02x (%s)\n",
	       IO_LCDCONT, mem_read_8(IO_LCDCONT),
		byte2bin_str(mem_read_8(IO_LCDCONT), s));
	printf("%02X [LCDSTAT]:   %02x (%s)\n",
	       IO_LCDSTAT, mem_read_8(IO_LCDSTAT),
		byte2bin_str(mem_read_8(IO_LCDSTAT), s));
	printf("%02X [SCROLLY]:   %02x (%s)\n",
	       IO_SCROLLY, mem_read_8(IO_SCROLLY),
		byte2bin_str(mem_read_8(IO_SCROLLY), s));
	printf("%02X [SCROLLX]:   %02x (%s)\n",
	       IO_SCROLLX, mem_read_8(IO_SCROLLX),
		byte2bin_str(mem_read_8(IO_SCROLLX), s));
	printf("%02X [CURLINE]:   %02x (%s)\n",
	       IO_CURLINE, mem_read_8(IO_CURLINE),
		byte2bin_str(mem_read_8(IO_CURLINE), s));
	printf("%02X [CMPLINE]:   %02x (%s)\n",
	       IO_CMPLINE, mem_read_8(IO_CMPLINE),
		byte2bin_str(mem_read_8(IO_CMPLINE), s));
	printf("%02X [BGRDPAL]:   %02x (%s)\n",
	       IO_BGRDPAL, mem_read_8(IO_BGRDPAL),
		byte2bin_str(mem_read_8(IO_BGRDPAL), s));
	printf("%02X [OBJ0PAL]:   %02x (%s)\n",
	       IO_OBJ0PAL, mem_read_8(IO_OBJ0PAL),
		byte2bin_str(mem_read_8(IO_OBJ0PAL), s));
	printf("%02X [OBJ1PAL]:   %02x (%s)\n",
	       IO_OBJ1PAL, mem_read_8(IO_OBJ1PAL),
		byte2bin_str(mem_read_8(IO_OBJ1PAL), s));
	printf("%02X [WNDPOSY]:   %02x (%s)\n",
	       IO_WNDPOSY, mem_read_8(IO_WNDPOSY),
		byte2bin_str(mem_read_8(IO_WNDPOSY), s));
	printf("%02X [WNDPOSX]:   %02x (%s)\n",
	       IO_WNDPOSX, mem_read_8(IO_WNDPOSX),
		byte2bin_str(mem_read_8(IO_WNDPOSX), s));
	printf("%02X [DMACONT]:   %02x (%s)\n",
	       IO_DMACONT, mem_read_8(IO_DMACONT),
		byte2bin_str(mem_read_8(IO_DMACONT), s));
	printf("%02X [IENABLE]:   %02x (%s)\n",
	       IO_IENABLE, mem_read_8(IO_IENABLE),
		byte2bin_str(mem_read_8(IO_IENABLE), s));
}
