/*
 * ***************************************************************************
 * Copyright (C) 2015 Marvell International Ltd.
 * ***************************************************************************
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 2 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ***************************************************************************
 */


/* #define DEBUG */
#include <common.h>
#include <asm/io.h>
#include <asm/arch-mvebu/mvebu.h>
#include <asm/arch-mvebu/thermal.h>

DECLARE_GLOBAL_DATA_PTR;

#define THERMAL_TIMEOUT					1200

#define THERMAL_SEN_CTRL_LSB				0x0
#define THERMAL_SEN_CTRL_LSB_STRT_OFFSET		0
#define THERMAL_SEN_CTRL_LSB_STRT_MASK			(0x1 << THERMAL_SEN_CTRL_LSB_STRT_OFFSET)
#define THERMAL_SEN_CTRL_LSB_RST_OFFSET			1
#define THERMAL_SEN_CTRL_LSB_RST_MASK			(0x1 << THERMAL_SEN_CTRL_LSB_RST_OFFSET)
#define THERMAL_SEN_CTRL_LSB_EN_OFFSET			2
#define THERMAL_SEN_CTRL_LSB_EN_MASK			(0x1 << THERMAL_SEN_CTRL_LSB_EN_OFFSET)

#define THERMAL_SEN_CTRL_STATS				0x8
#define THERMAL_SEN_CTRL_STATS_VALID_OFFSET		16
#define THERMAL_SEN_CTRL_STATS_VALID_MASK		(0x1 << THERMAL_SEN_CTRL_STATS_VALID_OFFSET)
#define THERMAL_SEN_CTRL_STATS_TEMP_OUT_OFFSET		0
#define THERMAL_SEN_CTRL_STATS_TEMP_OUT_MASK		(0x3FF << THERMAL_SEN_CTRL_STATS_TEMP_OUT_OFFSET)

#define THERMAL_SEN_OUTPUT_MSB				512
#define THERMAL_SEN_OUTPUT_COMP				1024

s32 mvebu_thermal_ext_sensor_read(struct thermal_unit_config *tsen)
{
	u32 reg;

	if (!tsen->tsen_ready) {
		printf("External Thermal Sensor was not initialized\n");
		return 0;
	}

	reg = readl(tsen->regs_base + THERMAL_SEN_CTRL_STATS);
	reg = ((reg & THERMAL_SEN_CTRL_STATS_TEMP_OUT_MASK) >> THERMAL_SEN_CTRL_STATS_TEMP_OUT_OFFSET);

	/* TSEN output format is signed as a 2s complement number
	   ranging from-512 to +511. when MSB is set, need to
	   calculate the complement number */
	if (reg >= THERMAL_SEN_OUTPUT_MSB)
		reg -= THERMAL_SEN_OUTPUT_COMP;

	return ((tsen->tsen_gain * ((s32)reg)) + tsen->tsen_offset) / tsen->tsen_divisor;
}

u32 mvebu_thermal_ext_sensor_probe(struct thermal_unit_config *tsen)
{
	u32 reg, timeout = 0;

	debug_enter();
	debug("thermal.%lx Initializing sensor unit\n", (uintptr_t)tsen->regs_base);

	/* Initialize thermal sensor hardware reset once */
	reg = readl(tsen->regs_base + THERMAL_SEN_CTRL_LSB);
	reg &= ~THERMAL_SEN_CTRL_LSB_RST_OFFSET; /* De-assert TSEN_RESET */
	reg |= THERMAL_SEN_CTRL_LSB_EN_MASK; /* Set TSEN_EN to 1 */
	reg |= THERMAL_SEN_CTRL_LSB_STRT_MASK; /* Set TSEN_START to 1 */
	writel(reg, tsen->regs_base + THERMAL_SEN_CTRL_LSB);

	reg = readl(tsen->regs_base + THERMAL_SEN_CTRL_STATS);
	while ((reg & THERMAL_SEN_CTRL_STATS_VALID_MASK) == 0 && timeout < THERMAL_TIMEOUT) {
		udelay(10);
		reg = readl(tsen->regs_base + THERMAL_SEN_CTRL_STATS);
		timeout++;
	}
	if ((reg & THERMAL_SEN_CTRL_STATS_VALID_MASK) == 0) {
		error("%s: thermal.%lx: external sensor is not ready\n", __func__, (uintptr_t)tsen->regs_base);
		return -1;
	}

	debug("thermal.%lx: Initialization done\n", (uintptr_t)tsen->regs_base);
	debug_exit();

	return 0;
}