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

	sparse::header header;
	std::vector<sparse::table_entry> table;

	sparse::read_header(fd_in, header       );
	sparse::read_table (fd_in, header, table);

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

	int fd_out = open(argv[2], O_WRONLY | O_CREAT, 0644);
	if ( fd_out < 0 )
	{
		perror("Could not open output file");
		exit(1);
	}

	bool disk_mode = false;
	int ftruncate_res = ftruncate(fd_out, 0);
	if ( ftruncate_res == -1 )
	{
		if ( errno == EINVAL )
		{
			disk_mode = true;
		}
		else
		{
			perror("Could not truncate output file");
			exit(1);
		}
	}

	size_t   buffer_size = 4096;
	uint8_t *buffer      = (uint8_t*)malloc(buffer_size);

	uint64_t accumulated_written_size = 0;
	uint64_t accumulated_fsync_size   = 0;

	for(const auto &i: table)
	{
		uint64_t size = i.logic_size;

		auto pos = lseek(fd_out, i.logic_offset, SEEK_SET);
		if ( pos == (off_t)-1 )
		{
			perror("Could not seek in the output file");
			exit(1);
		}

		while(size > 0)
		{
			auto read_size = std::min(size, buffer_size);

			for(size_t read_offset = 0; read_offset != read_size; )
			{
				int n = read(fd_in, buffer + read_offset, read_size - read_offset);

				if ( n < 0 )
				{
					perror("Could not read from input file\n");
					exit(1);
				}

				if ( n == 0 )
				{
					fprintf(stderr, "Invalid read size of 0x0\n");
					exit(1);
				}

				read_offset += n;
			}

			for(size_t write_offset = 0; write_offset != read_size; )
			{
				int n = write(fd_out, buffer + write_offset, read_size - write_offset);

				if ( n < 0 )
				{
					perror("Could not write to output file\n");
					exit(1);
				}

				if ( n == 0 )
				{
					fprintf(stderr, "Invalid write size of 0x0\n");
					exit(1);
				}

				write_offset             += n;
				accumulated_written_size += n;
				accumulated_fsync_size   += n;
			}

			size -= read_size;

			if ( accumulated_fsync_size >= (5*1024*1024 * 1) )
			{
				accumulated_fsync_size = 0;

				fprintf(stderr, "%16lu %16lu\n", accumulated_written_size, header.sparsed_size);

				if ( disk_mode )
				{
					fprintf(stderr, "fsync\n");
					fsync(fd_out);
				}
			}
		}

	}

	fprintf(stderr, "%16lu %16lu\n", accumulated_written_size, header.sparsed_size);

	if ( disk_mode )
	{
		fprintf(stderr, "fsync\n");
		fsync(fd_out);
	}
	else
	{
		if ( ftruncate(fd_out, header.unsparsed_size) == -1 )
		{
			perror("Could not truncate output file to the original file size");
			exit(1);
		}
	}

	return 0;
}

