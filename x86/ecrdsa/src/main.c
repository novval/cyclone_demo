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
 * @author Valery Novikov <novikov.val@gmail.com>
 * @version 2.3.4
 **/

//Platform-specific dependencies
#ifdef _WIN32
   #define _CRTDBG_MAP_ALLOC
   #define _WINERROR_
   #include <crtdbg.h>
   #include <conio.h>
   #include <winsock2.h>
   #include <ws2tcpip.h>
#else
   #include <sys/random.h>
   #include <sys/types.h>
   #include <sys/socket.h>
   #include <netinet/in.h>
   #include <arpa/inet.h>
   #include <netdb.h>
   #include <unistd.h>
   #include <termios.h>
   #include <errno.h>
#endif

//Dependencies
#include <stdlib.h>
#include "hash/hash_algorithms.h"
#include "ecc/ec_curves.h"
#include "ecc/ecrdsa.h"
#include "rng/yarrow.h"
#include "debug.h"


#define TEST_SIGN_COUNT 100//0

//Pseudo-random number generator
YarrowContext yarrowContext;


/**
 * @brief Main entry point
 * @return Status code
 **/

int_t main(void)
{
	error_t error;
	int_t ret;
#ifdef _WIN32
	//   WSADATA wsaData;
	HCRYPTPROV hProvider;
#endif
	uint8_t seed[32];

	//Start-up message
	TRACE_INFO("*********************************\r\n");
	TRACE_INFO("*** CycloneCRYPTO ECRDSA Demo ***\r\n");
	TRACE_INFO("*********************************\r\n");

#ifdef _WIN32
	//Acquire cryptographic context
	ret = CryptAcquireContext(&hProvider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
	//Any error to report?
	if (!ret)
	{
		//Debug message
		TRACE_ERROR("Error: Cannot acquire cryptographic context (%d)\r\n", GetLastError());
		//Exit immediately
		return ERROR_FAILURE;
	}

	//Generate a random seed
	ret = CryptGenRandom(hProvider, sizeof(seed), seed);
	//Any error to report?
	if (!ret)
	{
		//Debug message
		TRACE_ERROR("Error: Failed to generate random data (%d)\r\n", GetLastError());
		//Exit immediately
		return ERROR_FAILURE;
	}

	//Release cryptographic context
	CryptReleaseContext(hProvider, 0);
#else
	//Generate a random seed
	ret = getrandom(seed, sizeof(seed), GRND_RANDOM);
	//Any error to report?
	if (ret < 0)
	{
		//Debug message
		TRACE_ERROR("Error: Failed to generate random data (%d)\r\n", errno);
		//Exit immediately
		return ERROR_FAILURE;
	}
#endif

	//PRNG initialization
	error = yarrowInit(&yarrowContext);
	//Any error to report?
	if (error)
	{
		//Debug message
		TRACE_ERROR("Error: PRNG initialization failed (%d)\r\n", error);
		//Exit immediately
		return ERROR_FAILURE;
	}

	//Properly seed the PRNG
	error = yarrowSeed(&yarrowContext, seed, sizeof(seed));
	//Any error to report?
	if (error)
	{
		//Debug message
		TRACE_ERROR("Error: Failed to seed PRNG (%d)\r\n", error);
		//Exit immediately
		return error;
	}

	/**
	* ECRDSA tests from NIST
	**/
	error = ecrdsaTest();
	TRACE_INFO("\r\nECRDSA all test: %s\r\n", error == NO_ERROR ? "OK" : "ERROR");


	/**
	* ECRDSA bencmark
	**/
	struct Digest {
		uint8_t *digest;
		uint_t size;
	} digests[2] = {
		{ "\x53\x8c\xa0\x69\x15\xc1\x7b\x66\x0b\x70\x62\xac\x8f\x5e\xfc\x23\x1b\xe1\xaf\x8d\xf2\x46\xd9\x6b\xc3\xb3\x54\x02\xae\x9f\x0a\x33", 256 },
		{ "\x80\xd3\x32\xde\xe3\xdb\x39\xbd\xa4\x5b\xee\x55\x74\x31\xfa\xe7\x2b\xd7\xa0\xbb\x43\x84\xe1\x90\x68\xfb\x60\xdc\xa0\x01\xfb\x25\xa0\x32\x6e\x99\xfc\xc7\xa2\x6a\x1c\x47\xf0\x71\xb7\x92\x0c\xc4\xcd\x03\x5e\x8f\xd3\x9f\xc5\x9c\x0b\x2e\x8c\xa6\xb9\x83\x6c\x06", 512 }
	};

	EcDomainParameters params;
	EcPrivateKey privateKey;
	EcPublicKey publicKey;
	EcrdsaSignature ecrdsaSignature;

	//Initialize ECDSA signature
	ecrdsaInitSignature(&ecrdsaSignature);

	//Initialize EC domain parameters
	ecInitDomainParameters(&params);
	//Load EC domain parameters
	error = ecLoadDomainParameters(&params, SECP224R1_CURVE);
	params.h = 1;
	params.mod = NULL;

	//Initialize EC private key
	ecInitPrivateKey(&privateKey);
	//Initialize EC public key
	ecInitPublicKey(&publicKey);

	// Generate public key & pair public key
	error = ecGenerateKeyPair(&yarrowPrngAlgo, &yarrowContext, &params, &privateKey, &publicKey);

	//Check status code
	if (!error)
	{
		//Generate ECDSA signature
		for (int n = 0; n < 2; ++n) {
			// Benchmark
			TRACE_INFO("\nECRDSA timing benchmark.\n");
			TRACE_INFO("Signing %d signatures...\n", TEST_SIGN_COUNT);

			error = NO_ERROR;

			uint32_t before = osGetSystemTime();

			for (int i = 0; i < TEST_SIGN_COUNT; i++)
				error |= ecrdsaGenerateSignature(&yarrowPrngAlgo, &yarrowContext, &params, &privateKey, digests[n].digest, digests[n].size, &ecrdsaSignature);

			uint32_t after = osGetSystemTime();
			float seconds = (float)(after - before) / 1000;

			if (error == NO_ERROR) {
				TRACE_INFO("Time = %f seconds\n", seconds);
				TRACE_INFO("Speed = %.2f signatures/second\n", (float)TEST_SIGN_COUNT / seconds);
			}
			else {
				TRACE_INFO("Error occured\n");
			}

			TRACE_INFO("Verifying %d signatures...\n", TEST_SIGN_COUNT);

			before = osGetSystemTime();

			for (int i = 0; i < TEST_SIGN_COUNT; i++)
				error |= ecrdsaVerifySignature(&params, &publicKey, digests[n].digest, digests[n].size, &ecrdsaSignature);

			after = osGetSystemTime();
			seconds = (float)(after - before) / 1000;

			if (error == NO_ERROR) {
				TRACE_INFO("Time = %f seconds\n", seconds);
				TRACE_INFO("Speed = %.2f signatures/second\n", (float)TEST_SIGN_COUNT / seconds);
			}
			else {
				TRACE_INFO("Error occured\n");
			}

		}
	}

	//Release previously allocated resources
	ecFreeDomainParameters(&params);
	ecFreePrivateKey(&privateKey);

	////Check status code
	//if (!error)
	//{
	//	//Encode the resulting (R, S) integer pair using ASN.1
	//	error = ecrdsaWriteSignature(&ecrdsaSignature, signature, signatureLen);
	//}

	//Release previously allocated resources
	ecrdsaFreeSignature(&ecrdsaSignature);

	//*******************************************************************

   //Release PRNG context
   yarrowRelease(&yarrowContext);

#ifdef _WIN32
   //Dumps all the memory blocks in the heap when a memory leak has occurred
   _CrtDumpMemoryLeaks();
#endif

   //Return status code
   return error;
}
