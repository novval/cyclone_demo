/**
 * @file main.c
 * @brief Hash demo
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
 * @author Valery Novikov <novikov.val@gmail.com>
 * @version 2.3.4
 **/

//Platform-specific dependencies
#ifdef _WIN32
   #include <crtdbg.h>
#else
   #include <sys/types.h>
   #include <unistd.h>
#endif

//Dependencies
#include <stdlib.h>
#include "hash/hash_algorithms.h"
#include "debug.h"

#if defined _MSC_VER
#define ALIGN(x) __declspec(align(x))
#else
#define ALIGN(x) __attribute__ ((__aligned__(x)))
#endif

#define TEST_BLOCK_LEN		8192
#define TEST_BLOCK_COUNT	50000

unsigned char test_m1[63] = "012345678901234567890123456789012345678901234567890123456789012";
ALIGN(16) unsigned char block[TEST_BLOCK_LEN];
unsigned char digest[STREEBOG512_DIGEST_SIZE];

void print_digest(unsigned char *digest, unsigned int len)
{
	unsigned int i;
	if (len > 64) len = 64;

	///* eflag is set when little-endian output requested */
	//if (eflag) reverse_order(in, len);

	for (i = 0; i < len; i++)
		TRACE_INFO("%02x", (unsigned char) digest[i]);
}


/**
 * @brief Main entry point
 * @return 0
 **/

int_t main(void)
{
   //Start-up message
   TRACE_INFO("*******************************\r\n");
   TRACE_INFO("*** CycloneCRYPTO Hash Demo ***\r\n");
   TRACE_INFO("*******************************\r\n");
   TRACE_INFO("\r\n");

//   const HashAlgo *hashSha256Algo = SHA256_HASH_ALGO;
   const HashAlgo *hashAlgos[] = {
	   STREEBOG256_HASH_ALGO,
	   STREEBOG512_HASH_ALGO
   };

   // Test
   TRACE_INFO("M1: 012345678901234567890123456789012345678901234567890123456789012\n");

   for (int i = 0; i < arraysize(hashAlgos); ++i) {
	   const HashAlgo *hashAlgo = hashAlgos[i];

	   //Allocate a memory buffer to hold the hash context
	   HashContext *hashContext = cryptoAllocMem(hashAlgo->contextSize);
	   //Successful memory allocation?
	   if (!hashContext) {
		   TRACE_INFO("Error alocate context for %s algo\n", hashAlgo->name);
		   continue;
	   }

	   hashAlgo->init(hashContext);
	   hashAlgo->update(hashContext, test_m1, sizeof(test_m1));
	   hashAlgo->final(hashContext, digest);
	   TRACE_INFO("%s %d bit digest (M1): 0x", hashAlgo->name, hashAlgo->digestSize * 8);
	   print_digest(digest, hashAlgo->digestSize);
	   TRACE_INFO("\n");

	   // Benchmark
	   TRACE_INFO("\n%s timing benchmark.\n", hashAlgo->name);
	   TRACE_INFO("Digesting %d %d-byte blocks with %d bits digest...\n", TEST_BLOCK_COUNT, TEST_BLOCK_LEN, hashAlgo->digestSize * 8);

	   for (int i = 0; i < TEST_BLOCK_LEN; i++)
		   block[i] = (unsigned char)(i & 0xff);

	   uint32_t before = osGetSystemTime();

	   hashAlgo->init(hashContext);
	   for (int i = 0; i < TEST_BLOCK_COUNT; i++)
		   hashAlgo->update(hashContext, block, TEST_BLOCK_LEN);
	   hashAlgo->final(hashContext, NULL);

	   uint32_t after = osGetSystemTime();
	   float seconds = (float)(after - before) / 1000;

	   TRACE_INFO("Digest = ");
	   print_digest(hashContext->digest, hashAlgo->digestSize);
	   TRACE_INFO("\nTime = %f seconds\n", seconds);
	   TRACE_INFO("Speed = %.2f bytes/second\n", (float)TEST_BLOCK_LEN * (float)TEST_BLOCK_COUNT / seconds);

	   cryptoFreeMem(hashContext);
   }
   
#ifdef _WIN32
   //Dumps all the memory blocks in the heap when a memory leak has occurred
   _CrtDumpMemoryLeaks();
#endif

   //Return status code
   return 0;
}
