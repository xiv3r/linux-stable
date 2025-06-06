/* SPDX-License-Identifier: GPL-2.0 */

/***************************************************************************
 *    copyright            : (C) 2002, 2004 by Frank Mori Hess             *
 ***************************************************************************/

#include "gpibP.h"
#include "plx9050.h"
#include "tms9914.h"

enum pci_vendor_ids {
	PCI_VENDOR_ID_AGILENT = 0x15bc,
};

enum pci_device_ids {
	PCI_DEVICE_ID_82350B = 0x0b01,
	PCI_DEVICE_ID_82351A = 0x1218
};

enum pci_subdevice_ids {
	PCI_SUBDEVICE_ID_82350A = 0x10b0,
};

enum pci_regions_82350a {
	PLX_MEM_REGION  = 0,
	PLX_IO_REGION   = 1,
	GPIB_82350A_REGION = 2,
	SRAM_82350A_REGION = 3,
	BORG_82350A_REGION = 4
};

enum pci_regions_82350b {
	GPIB_REGION = 0,
	SRAM_REGION = 1,
	MISC_REGION = 2,
};

enum board_model {
	MODEL_82350A,
	MODEL_82350B,
	MODEL_82351A
};

// struct which defines private_data for board
struct agilent_82350b_priv {
	struct tms9914_priv tms9914_priv;
	struct pci_dev *pci_device;
	void __iomem *plx_base;	//82350a only
	void __iomem *gpib_base;
	void __iomem *sram_base;
	void __iomem *misc_base;
	void __iomem *borg_base;
	int irq;
	unsigned short card_mode_bits;
	unsigned short event_status_bits;
	enum board_model model;
	bool using_fifos;
};

//registers
enum agilent_82350b_gpib_registers

{
	CARD_MODE_REG = 0x1,
	CONFIG_DATA_REG = 0x2, // 82350A specific
	INTERRUPT_ENABLE_REG = 0x3,
	EVENT_STATUS_REG = 0x4,
	EVENT_ENABLE_REG = 0x5,
	STREAM_STATUS_REG = 0x7,
	DEBUG_RAM0_REG = 0x8,
	DEBUG_RAM1_REG = 0x9,
	DEBUG_RAM2_REG = 0xa,
	DEBUG_RAM3_REG = 0xb,
	XFER_COUNT_LO_REG = 0xc,
	XFER_COUNT_MID_REG = 0xd,
	XFER_COUNT_HI_REG = 0xe,
	TMS9914_BASE_REG = 0x10,
	INTERNAL_CONFIG_REG = 0x18,
	IMR0_READ_REG = 0x19, //read
	T1_DELAY_REG = 0x19, // write
	IMR1_READ_REG = 0x1a,
	ADR_READ_REG = 0x1b,
	SPMR_READ_REG = 0x1c,
	PPR_READ_REG = 0x1d,
	CDOR_READ_REG = 0x1e,
	SRAM_ACCESS_CONTROL_REG = 0x1f,
};

enum card_mode_bits

{
	ACTIVE_CONTROLLER_BIT = 0x2,	// read-only
	CM_SYSTEM_CONTROLLER_BIT = 0x8,
	ENABLE_BUS_MONITOR_BIT = 0x10,
	ENABLE_PCI_IRQ_BIT = 0x20,
};

enum interrupt_enable_bits

{
	ENABLE_TMS9914_INTERRUPTS_BIT = 0x1,
	ENABLE_BUFFER_END_INTERRUPT_BIT = 0x10,
	ENABLE_TERM_COUNT_INTERRUPT_BIT = 0x20,
};

enum event_enable_bits

{
	ENABLE_BUFFER_END_EVENTS_BIT = 0x10,
	ENABLE_TERM_COUNT_EVENTS_BIT = 0x20,
};

enum event_status_bits

{
	TMS9914_IRQ_STATUS_BIT = 0x1,
	IRQ_STATUS_BIT = 0x2,
	BUFFER_END_STATUS_BIT = 0x10,	// write-clear
	TERM_COUNT_STATUS_BIT = 0x20,	// write-clear
};

enum stream_status_bits

{
	HALTED_STATUS_BIT = 0x1,	//read
	RESTART_STREAM_BIT = 0x1,	//write
};

enum internal_config_bits

{
	IC_SYSTEM_CONTROLLER_BIT = 0x80,
};

enum sram_access_control_bits

{
	DIRECTION_GPIB_TO_HOST = 0x20,	// transfer direction
	ENABLE_TI_TO_SRAM = 0x40,	// enable fifo
	ENABLE_FAST_TALKER = 0x80	// added for 82350A (not used)
};

enum borg_bits

{
	BORG_READY_BIT = 0x40,
	BORG_DONE_BIT = 0x80
};

static const int agilent_82350b_fifo_size = 0x8000;

static inline int agilent_82350b_fifo_is_halted(struct agilent_82350b_priv *a_priv)

{
	return readb(a_priv->gpib_base + STREAM_STATUS_REG) & HALTED_STATUS_BIT;
}

