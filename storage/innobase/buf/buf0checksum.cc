/*****************************************************************************

Copyright (c) 1995, 2016, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2017, 2018, MariaDB Corporation.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file buf/buf0checksum.cc
Buffer pool checksum functions, also linked from /extra/innochecksum.cc

Created Aug 11, 2011 Vasil Dimov
*******************************************************/

#include "univ.i"
#include "fil0fil.h"
#include "ut0crc32.h"
#include "ut0rnd.h"
#include "buf0checksum.h"
#include "mtr0types.h"
#include "mach0data.h"

#ifndef UNIV_INNOCHECKSUM
#include "srv0srv.h"
#endif /* !UNIV_INNOCHECKSUM */

#include "buf0types.h"

/** the macro MYSQL_SYSVAR_ENUM() requires "long unsigned int" and if we
use srv_checksum_algorithm_t here then we get a compiler error:
ha_innodb.cc:12251: error: cannot convert 'srv_checksum_algorithm_t*' to
  'long unsigned int*' in initialization */
ulong	srv_checksum_algorithm = SRV_CHECKSUM_ALGORITHM_INNODB;

/** set if we have found pages matching legacy big endian checksum */
bool	legacy_big_endian_checksum = false;
/** Calculates the CRC32 checksum of a page. The value is stored to the page
when it is written to a file and also checked for a match when reading from
the file. When reading we allow both normal CRC32 and CRC-legacy-big-endian
variants. Note that we must be careful to calculate the same value on 32-bit
and 64-bit architectures.
@param[in]	page			buffer page (UNIV_PAGE_SIZE bytes)
@param[in]	use_legacy_big_endian	if true then use big endian
byteorder when converting byte strings to integers
@return checksum */
uint32_t
buf_calc_page_crc32(
	const byte*	page,
	bool		use_legacy_big_endian /* = false */)
{
	/* Since the field FIL_PAGE_FILE_FLUSH_LSN, and in versions <= 4.1.x
	FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, are written outside the buffer pool
	to the first pages of data files, we have to skip them in the page
	checksum calculation.
	We must also skip the field FIL_PAGE_SPACE_OR_CHKSUM where the
	checksum is stored, and also the last 8 bytes of page because
	there we store the old formula checksum. */

	ut_crc32_func_t	crc32_func = use_legacy_big_endian
		? ut_crc32_legacy_big_endian
		: ut_crc32;

	const uint32_t	c1 = crc32_func(
		page + FIL_PAGE_OFFSET,
		FIL_PAGE_FILE_FLUSH_LSN_OR_KEY_VERSION - FIL_PAGE_OFFSET);

	const uint32_t	c2 = crc32_func(
		page + FIL_PAGE_DATA,
		UNIV_PAGE_SIZE - FIL_PAGE_DATA - FIL_PAGE_END_LSN_OLD_CHKSUM);

	return(c1 ^ c2);
}

/** Calculate a checksum which is stored to the page when it is written
to a file. Note that we must be careful to calculate the same value on
32-bit and 64-bit architectures.
@param[in]	page	file page (srv_page_size bytes)
@return checksum */
uint32_t
buf_calc_page_new_checksum(const byte* page)
{
	ulint checksum;

	/* Since the field FIL_PAGE_FILE_FLUSH_LSN, and in versions <= 4.1.x
	FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, are written outside the buffer pool
	to the first pages of data files, we have to skip them in the page
	checksum calculation.
	We must also skip the field FIL_PAGE_SPACE_OR_CHKSUM where the
	checksum is stored, and also the last 8 bytes of page because
	there we store the old formula checksum. */

	checksum = ut_fold_binary(page + FIL_PAGE_OFFSET,
				  FIL_PAGE_FILE_FLUSH_LSN_OR_KEY_VERSION
				  - FIL_PAGE_OFFSET)
		+ ut_fold_binary(page + FIL_PAGE_DATA,
				 UNIV_PAGE_SIZE - FIL_PAGE_DATA
				 - FIL_PAGE_END_LSN_OLD_CHKSUM);
	return(static_cast<uint32_t>(checksum));
}

/** In MySQL before 4.0.14 or 4.1.1 there was an InnoDB bug that
the checksum only looked at the first few bytes of the page.
This calculates that old checksum.
NOTE: we must first store the new formula checksum to
FIL_PAGE_SPACE_OR_CHKSUM before calculating and storing this old checksum
because this takes that field as an input!
@param[in]	page	file page (srv_page_size bytes)
@return checksum */
uint32_t
buf_calc_page_old_checksum(const byte* page)
{
	return(static_cast<uint32_t>
	       (ut_fold_binary(page, FIL_PAGE_FILE_FLUSH_LSN_OR_KEY_VERSION)));
}

/** Return a printable string describing the checksum algorithm.
@param[in]	algo	algorithm
@return algorithm name */
const char*
buf_checksum_algorithm_name(srv_checksum_algorithm_t algo)
{
	switch (algo) {
	case SRV_CHECKSUM_ALGORITHM_CRC32:
		return("crc32");
	case SRV_CHECKSUM_ALGORITHM_STRICT_CRC32:
		return("strict_crc32");
	case SRV_CHECKSUM_ALGORITHM_INNODB:
		return("innodb");
	case SRV_CHECKSUM_ALGORITHM_STRICT_INNODB:
		return("strict_innodb");
	case SRV_CHECKSUM_ALGORITHM_NONE:
		return("none");
	case SRV_CHECKSUM_ALGORITHM_STRICT_NONE:
		return("strict_none");
	}

	ut_error;
	return(NULL);
}

/** Calculates the CRC32 checksum of a page compressed page. The value is
stored to the page when it is written to a file and also checked for
a match when reading from the file. Checksum is calculated from
actual payload of the compressed page and some header fields.

@param[in]	page			buffer page (UNIV_PAGE_SIZE bytes)
@return checksum */
uint32_t
buf_calc_compressed_crc32(
	const byte*	page)
{
	/* In page compressed pages compression method is stored to field
	FIL_PAGE_FILE_FLUSH_LSN_OR_KEY_VERSION, thus add it to checksum.
	In pages first compressed and then encrypted same field
	contains key version after compression. */

	ulint page_type = mach_read_from_2(page + FIL_PAGE_TYPE);

	ulint header_len =  page_type == FIL_PAGE_PAGE_COMPRESSED ?
		FIL_PAGE_SPACE_ID - FIL_PAGE_OFFSET :
		FIL_PAGE_FILE_FLUSH_LSN_OR_KEY_VERSION - FIL_PAGE_OFFSET;

	const uint32_t	c1 = ut_crc32(
		page + FIL_PAGE_OFFSET,
		header_len);

	/* Calculate checksum from actual payload including stored size
	field. In encrypted case add also compression method field. */
	ulint payload_len = mach_read_from_2(page+FIL_PAGE_DATA)+FIL_PAGE_COMPRESSED_SIZE;

	if (page_type == FIL_PAGE_PAGE_COMPRESSED_ENCRYPTED) {
		payload_len += FIL_PAGE_COMPRESSION_METHOD_SIZE;
	}

	const uint32_t	c2 = ut_crc32(
		page + FIL_PAGE_DATA,
		payload_len);

	return(c1 ^ c2);
}
