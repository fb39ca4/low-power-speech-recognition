/*
 * Copyright (c) 2013-2014, 2016, Kevin Läufer
 * Copyright (c) 2013-2018, Niklas Hauser
 * Copyright (c) 2016, Fabian Greif
 * Copyright (c) 2016, Sascha Schade
 * Copyright (c) 2017, Michael Thies
 *
 * This file is part of the modm project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
// ----------------------------------------------------------------------------

#include <stdint.h>
#include <modm/architecture/utils.hpp>
#include <modm/architecture/interface/assert.h>
#include "../device.hpp"

// ----------------------------------------------------------------------------
// Interrupt vectors
typedef void (* const FunctionPointer)(void);

// ----------------------------------------------------------------------------
// The following are constructs created by the linker, indicating where the
// the "data" and "bss" segments reside in memory.
extern uint32_t __stack_start[];
extern uint32_t __stack_end[];

extern uint32_t __table_copy_intern_start[];
extern uint32_t __table_copy_intern_end[];

extern uint32_t __table_zero_intern_start[];
extern uint32_t __table_zero_intern_end[];

extern uint32_t __table_copy_extern_start[];
extern uint32_t __table_copy_extern_end[];

extern uint32_t __table_zero_extern_start[];
extern uint32_t __table_zero_extern_end[];

extern uint32_t __vector_table_rom_start[];
extern uint32_t __vector_table_ram_start[];

extern FunctionPointer __hardware_init_start[];
extern FunctionPointer __hardware_init_end[];

// Application's main function
int __attribute__((noreturn))
main(void);

// calls CTORS of static objects
void
__libc_init_array(void);

extern void
__modm_initialize_memory(void);

extern void
__modm_initialize_platform(void);

static void
table_copy(uint32_t **table, uint32_t **end)
{
	while(table < end)
	{
		uint32_t *src  = table[0]; // load address
		uint32_t *dest = table[1]; // destination start
		while (dest < table[2])    // destination end
		{
			*(dest++) = *(src++);
		}
		table += 3;
	}
}

static void
table_zero(uint32_t **table, uint32_t **end)
{
	while(table < end)
	{
		uint32_t *dest = table[0]; // destination start
		while (dest < table[1])    // destination end
		{
			*(dest++) = 0;
		}
		table += 2;
	}
}

// ----------------------------------------------------------------------------
void
Reset_Handler(void)
{
	__modm_initialize_platform();

	// copy all internal memory declared in linkerscript table
	table_copy(	(uint32_t**)__table_copy_intern_start,
				(uint32_t**)__table_copy_intern_end);
	table_zero(	(uint32_t**)__table_zero_intern_start,
				(uint32_t**)__table_zero_intern_end);

	// Enable FPU in privileged and user mode
	SCB->CPACR |= ((3UL << 10*2) | (3UL << 11*2));  // set CP10 and CP11 Full Access
	// Setup NVIC
	// Set vector table
	SCB->VTOR = (uint32_t)(__vector_table_rom_start);
	// Enables handlers with priority -1 or -2 to ignore data BusFaults caused by load and store instructions.
	// This applies to the hard fault, NMI, and FAULTMASK escalated handlers.
	// We use this to opportunistically restore LR, PC and xPSR in the hard fault handler.
	// Also enables trapping of divide by zero. Otherwise it would just be ignored.
	SCB->CCR |= SCB_CCR_BFHFNMIGN_Msk | SCB_CCR_DIV_0_TRP_Msk;

	// Lower priority level for all peripheral interrupts to lowest possible
	for (uint32_t i = 0; i < 85; i++) {
		NVIC->IP[i] = 0xff;
	}

	// Set the PRIGROUP[10:8] bits to
	// - 4 bits for pre-emption priority,
	// - 0 bits for subpriority
	NVIC_SetPriorityGrouping(3);

	// Enable fault handlers
	/*SCB->SHCSR |=
			SCB_SHCSR_BUSFAULTENA_Msk |
			SCB_SHCSR_USGFAULTENA_Msk |
			SCB_SHCSR_MEMFAULTENA_Msk;*/
	// Call all hardware initialize hooks.
	for (FunctionPointer *init_hook = __hardware_init_start;
			init_hook < __hardware_init_end;
			init_hook++) {
		(*init_hook)();
	}

	// copy all external memory declared in linkerscript table
	table_copy(	(uint32_t**)__table_copy_extern_start,
				(uint32_t**)__table_copy_extern_end);
	table_zero(	(uint32_t**)__table_zero_extern_start,
				(uint32_t**)__table_zero_extern_end);

	// initialize heap
	// needs to be done before calling the CTORS of static objects
	__modm_initialize_memory();

	// Call CTORS of static objects
	__libc_init_array();

	// Call the application's entry point
	main();

	modm_assert_debug(0, "core", "main", "exit");
	while(1) ;
}