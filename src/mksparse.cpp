/*
 * Copyright (C) 2016 Tiago Gonçalves
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "common.hpp"

int main(int argc, char *argv[])
{
	// TODO argc, argv check

	int fd_in = open(argv[1], O_RDONLY);
	if ( fd_in < 0 )
	{
		perror("Could not open input file");
		exit(1);
	}

	auto table = sparse::get_fiemap_entries(fd_in);

	uint64_t sparsed_size = 0;
	for(const auto &i: table)
	{
		sparsed_size += i.logic_size;
	}

	auto unsparsed_size = lseek(fd_in, 0, SEEK_END);
	if ( unsparsed_size == (off_t)-1 )
	{
		fprintf(stderr, "Could not get file size\n");
		exit(1);
	}

	sparse::header header;

	header.version         = 1;
	header.unsparsed_size  = unsparsed_size;
	header.  sparsed_size  = sparsed_size;
	header.n_table_entries = table.size();

	int fd_out = open(argv[2], O_WRONLY | O_CREAT, 0644);
	if ( fd_out < 0 )
	{
		perror("Could not open output file");
		exit(1);
	}

	sparse::write_header(fd_out, header);
	sparse::write_table (fd_out, table);

	/*
	fprintf(stderr, "version         %u\n",  header.version        );
	fprintf(stderr, "n_table_entries %u\n",  header.n_table_entries);
	fprintf(stderr, "unsparsed_size  %lu\n", header.unsparsed_size );
	fprintf(stderr, "  sparsed_size  %lu\n", header.  sparsed_size );

	for(const auto &i: table)
	{
		fprintf(stderr, "%16lu %16lu\n", i.logic_offset, i.logic_size);
	}
	*/

	for(const auto &i: table)
	{
		if ( lseek(fd_in, i.logic_offset, SEEK_SET) == (off_t)-1 )
		{
			perror("Failed to lseek in the input file");
			exit(1);
		}

		auto size = i.logic_size;
		while( size > 0 )
		{
			char buff[4096];
			size_t buff_size = std::min(size, static_cast<uint64_t>(sizeof(buff)));
			int n = read(fd_in, buff, buff_size);
			if ( n < 0 )
			{
				perror("Could not read input file");
				exit(1);
			}
			size -= n;

			auto b = buff;
			for( ; n > 0; )
			{
				int a = write(fd_out, b, n);
				if ( a < 0 )
				{
					perror( "Could not write output file");
					exit(1);
				}

				b += a;
				n -= a;
			}
		}
	}

	return 0;
}

