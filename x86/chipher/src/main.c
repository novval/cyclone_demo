/**
 * @file main.c
 * @brief DTLS client demo
 *
 * @section License
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2010-2023 Oryx Embedded SARL. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @author Oryx Embedded SARL (www.oryx-embedded.com)
 * @version 2.3.4
 **/

//Platform-specific dependencies
#ifdef _WIN32
   #define _CRTDBG_MAP_ALLOC
   #define _WINERROR_
   #include <crtdbg.h>
#else
   #include <sys/types.h>
   #include <unistd.h>
#endif

//Dependencies
#include <stdlib.h>
#include "cipher/cipher_algorithms.h"
#include "debug.h"

#define TEST_BLOCK_COUNT	50000

const uint8_t testvec_pt[16] = {
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x00,
	0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88
};
const uint8_t testvec_ct[16] = {
	0x7F, 0x67, 0x9D, 0x90, 0xBE, 0xBC, 0x24, 0x30,
	0x5A, 0x46, 0x8D, 0x42, 0xB9, 0xD4, 0xED, 0xCD
};

static uint8_t test_key[32] = {
	0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
	0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
};

static uint8_t test_iv[16] = {
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
};

// debug print state
void print_w128(w128_t *x)
{
	int i;

	for (i = 0; i < 16; i++)
		TRACE_INFO(" %02X", x->b[i]);
	TRACE_INFO("\n");
}

/**
 * @brief Main entry point
 * @return Status code
 **/

int_t main(void)
{
   error_t error;

   //Start-up message
   TRACE_INFO("**********************************\r\n");
   TRACE_INFO("*** CycloneCRYPTO Chipher Demo ***\r\n");
   TRACE_INFO("**********************************\r\n");
   TRACE_INFO("\r\n");

   KuznyechikContext ctx;
   const CipherAlgo *cipherAlgo = KUZNYECHIK_CIPHER_ALGO;
   
   /* Set key */
   TRACE_INFO("%s self-test:\n", cipherAlgo->name);

   error = cipherAlgo->init(&ctx, test_key, cipherAlgo->blockSize);
   if (error) {
	   TRACE_ERROR("Error: Error init cipher\n");
	   return -1;
   }

   for (int i = 0; i < 10; i++) {
	   TRACE_INFO("K_%d\t=", i + 1);
	   print_w128(&ctx.ek[i]);
   }

   w128_t x;
   for (int i = 0; i < 16; i++)
	   x.b[i] = testvec_pt[i];
   TRACE_INFO("PT\t=");
   print_w128(&x);

   cipherAlgo->encryptBlock(&ctx, (const uint8_t *)&x, (uint8_t *)&x);

   TRACE_INFO("CT\t=");
   print_w128(&x);

   for (int i = 0; i < 16; i++) {
	   if (testvec_ct[i] != x.b[i]) {
		   TRACE_ERROR("Error: Encryption self-test failure.\n");
		   return -1;
	   }
   }

   cipherAlgo->decryptBlock(&ctx, (const uint8_t *)&x, (uint8_t *)&x);

   TRACE_INFO("PT\t=");
   print_w128(&x);

   for (int i = 0; i < 16; i++) {
	   if (testvec_pt[i] != x.b[i]) {
		   TRACE_ERROR("Error: Decryption self-test failure.\n");
		   return -2;
	   }
   }

   TRACE_INFO("Self-test OK!\n");


   // Benchmark
   TRACE_INFO("\n%s timing benchmark.\n", cipherAlgo->name);
   TRACE_INFO("Crypting %d %d-byte blocks...\n", TEST_BLOCK_COUNT, cipherAlgo->blockSize);

   uint32_t before = osGetSystemTime();

   for (int i = 0; i < TEST_BLOCK_COUNT; i++)
	   cipherAlgo->encryptBlock(&ctx, (const uint8_t *)&x, (uint8_t *)&x);

   uint32_t after = osGetSystemTime();
   float seconds = (float)(after - before) / 1000;
   TRACE_INFO("\nTime = %f seconds\n", seconds);
   TRACE_INFO("Speed = %.2f bytes/second\n", (float)cipherAlgo->blockSize * (float)TEST_BLOCK_COUNT / seconds);


   TRACE_INFO("\nDecrypting %d %d-byte blocks...\n", TEST_BLOCK_COUNT, cipherAlgo->blockSize);

   before = osGetSystemTime();

   for (int i = 0; i < TEST_BLOCK_COUNT; i++)
	   cipherAlgo->decryptBlock(&ctx, (const uint8_t *)&x, (uint8_t *)&x);

   after = osGetSystemTime();
   seconds = (float)(after - before) / 1000;
   TRACE_INFO("\nTime = %f seconds\n", seconds);
   TRACE_INFO("Speed = %.2f bytes/second\n", (float)cipherAlgo->blockSize * (float)TEST_BLOCK_COUNT / seconds);

   cipherAlgo->deinit(&ctx);

#ifdef _WIN32
   //Dumps all the memory blocks in the heap when a memory leak has occurred
   _CrtDumpMemoryLeaks();
#endif

   //Return status code
   return error;
}
