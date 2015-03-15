/*  MudPRO: An advanced client for the online game MajorMUD
 *  Copyright (C) 2002, 2003, 2004  David Slusky
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __KEYS_H__
#define __KEYS_H__

enum
{
	/* CTRL_XXX */
	KEY_NONE,     KEY_CTRL_A,   KEY_CTRL_B,
	KEY_CTRL_C,   KEY_CTRL_D,   KEY_CTRL_E,
	KEY_CTRL_F,   KEY_CTRL_G,   KEY_CTRL_H,
	KEY_TAB,      KEY_LF,       KEY_CTRL_K,
	KEY_CTRL_L,   KEY_CR,       KEY_CTRL_N,
	KEY_CTRL_O,   KEY_CTRL_P,   KEY_CTRL_Q,
	KEY_CTRL_R,   KEY_CTRL_S,   KEY_CTRL_T,
	KEY_CTRL_U,   KEY_CTRL_V,   KEY_CTRL_W,
	KEY_CTRL_X,   KEY_CTRL_Y,   KEY_CTRL_Z,

	KEY_ALT = 0x1B,
	KEY_BKSP = 0x7F,

	/* ALT_XXX */
	KEY_ALT_A = 0x1B + 0x61,  KEY_ALT_B = 0x1B + 0x62,
	KEY_ALT_C = 0x1B + 0x63,  KEY_ALT_D = 0x1B + 0x64,
	KEY_ALT_E = 0x1B + 0x65,  KEY_ALT_F = 0x1B + 0x66,
	KEY_ALT_G = 0x1B + 0x67,  KEY_ALT_H = 0x1B + 0x68,
	KEY_ALT_I = 0x1B + 0x69,  KEY_ALT_J = 0x1B + 0x6A,
	KEY_ALT_K = 0x1B + 0x6B, _KEY_ALT_L = 0x1B + 0x6C,
	KEY_ALT_M = 0x1B + 0x6D,  KEY_ALT_N = 0x1B + 0x6E,
	KEY_ALT_O = 0x1B + 0x6F,  KEY_ALT_P = 0x1B + 0x70,
	KEY_ALT_Q = 0x1B + 0x71, _KEY_ALT_R = 0x1B + 0x72,
	KEY_ALT_S = 0x1B + 0x73,  KEY_ALT_T = 0x1B + 0x74,
	KEY_ALT_U = 0x1B + 0x75,  KEY_ALT_V = 0x1B + 0x76,
	KEY_ALT_W = 0x1B + 0x77,  KEY_ALT_X = 0x1B + 0x78,
	KEY_ALT_Y = 0x1B + 0x79,  KEY_ALT_Z = 0x1B + 0x7A,

	/* KP_XXX */
	KEY_KP_ENT = 0x1B + 0x4F + 0x4D,
	KEY_KP_MUL = 0x1B + 0x4F + 0x6A,
	KEY_KP_PLS = 0x1B + 0x4F + 0x6B,
	KEY_KP_MIN = 0x1B + 0x4F + 0x6D,
	KEY_KP_DEL = 0x1B + 0x4F + 0x6E,
	KEY_KP_DIV = 0x1B + 0x4F + 0x6F,
	KEY_KP0    = 0x1B + 0x4F + 0x70,
	KEY_KP1    = 0x1B + 0x4F + 0x71,
	KEY_KP2    = 0x1B + 0x4F + 0x72,
	KEY_KP3    = 0x1B + 0x4F + 0x73,
	KEY_KP4    = 0x1B + 0x4F + 0x74,
	KEY_KP5    = 0x1B + 0x4F + 0x75,
	KEY_KP6    = 0x1B + 0x4F + 0x76,
	KEY_KP7    = 0x1B + 0x4F + 0x77,
	KEY_KP8    = 0x1B + 0x4F + 0x78,
	KEY_KP9    = 0x1B + 0x4F + 0x79,
};

#endif /* __KEYS_H__ */
