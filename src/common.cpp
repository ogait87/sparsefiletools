/*
 * Copyright (C) 2016 Tiago Gonçalves
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/fiemap.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <stdint.h>
#include <unistd.h>
#include <algorithm>

#include "common.hpp"

namespace
{
	template <typename T>
	void pack_be_uint(const T &v, void *data, size_t size)
	{
		assert(size == sizeof(T));

		auto buff = reinterpret_cast<uint8_t *>(data);

		for(size_t i = 0; i < sizeof(T); ++i)
		{
			buff[sizeof(T) - i - 1] = (v >> (8 * i)) & 0xff;
		}
	}

	template <typename T>
	T unpack_be_uint(void *data, size_t size)
	{
		assert(size == sizeof(T));

		auto buff = reinterpret_cast<uint8_t *>(data);

		T res = 0;

		for(size_t i = 0; i < sizeof(T); ++i)
		{
			res += buff[sizeof(T) - i - 1] << (8 * i);
		}

		return res;
	}
}

std::vector<sparse::table_entry> sparse::get_fiemap_entries(int fd)
{
        size_t extent_count = 0;

        {
                struct fiemap map;

                map.fm_start        = 0;
                map.fm_length       = FIEMAP_MAX_OFFSET;
                map.fm_flags        = FIEMAP_FLAG_SYNC;
                map.fm_extent_count = 0;

                int res = ioctl(fd, FS_IOC_FIEMAP, &map);

                if ( res == -1 )
                {
                        fprintf(stderr, "ioctl FS_IOC_FIEMAP error: %s\n", strerror(errno));
                        exit(1);
                }

                extent_count = map.fm_mapped_extents;
        }

	std::vector<sparse::table_entry> table;

	if ( extent_count > 0 )
	{
                auto map = (struct fiemap *)malloc(sizeof(struct fiemap) + extent_count * sizeof(struct fiemap_extent));
                if( map == nullptr )
		{
			fprintf(stderr, "insufficient memory available\n");
		       	exit(1);
		}

                map->fm_start        = 0;
                map->fm_length       = FIEMAP_MAX_OFFSET;
                map->fm_flags        = FIEMAP_FLAG_SYNC;
                map->fm_extent_count = extent_count;

                int res = ioctl(fd, FS_IOC_FIEMAP, map);

                if ( res == -1 )
                {
                        fprintf(stderr, "ioctl FS_IOC_FIEMAP error: %s\n", strerror(errno));
                        exit(1);
                }

                if ( map->fm_mapped_extents != extent_count )
                {
                        fprintf(stderr, "file was modified while reading\n");
                        exit(1);
                }

                for(size_t i = 0; i < extent_count; ++i)
                {
                        auto extent = &map->fm_extents[i];

                        if ( extent->fe_flags & FIEMAP_EXTENT_UNKNOWN        ) { fprintf(stderr, "FIEMAP_EXTENT not Implemented\n"); exit(1); }
                        if ( extent->fe_flags & FIEMAP_EXTENT_DELALLOC       ) { fprintf(stderr, "FIEMAP_EXTENT not Implemented\n"); exit(1); }
                        if ( extent->fe_flags & FIEMAP_EXTENT_ENCODED        ) { fprintf(stderr, "FIEMAP_EXTENT not Implemented\n"); exit(1); }
                        if ( extent->fe_flags & FIEMAP_EXTENT_DATA_ENCRYPTED ) { fprintf(stderr, "FIEMAP_EXTENT not Implemented\n"); exit(1); }
                        if ( extent->fe_flags & FIEMAP_EXTENT_NOT_ALIGNED    ) { fprintf(stderr, "FIEMAP_EXTENT not Implemented\n"); exit(1); }
                        if ( extent->fe_flags & FIEMAP_EXTENT_DATA_INLINE    ) { fprintf(stderr, "FIEMAP_EXTENT not Implemented\n"); exit(1); }
                        if ( extent->fe_flags & FIEMAP_EXTENT_DATA_TAIL      ) { fprintf(stderr, "FIEMAP_EXTENT not Implemented\n"); exit(1); }
                        if ( extent->fe_flags & FIEMAP_EXTENT_UNWRITTEN      ) { fprintf(stderr, "FIEMAP_EXTENT not Implemented\n"); exit(1); }
                        if ( extent->fe_flags & FIEMAP_EXTENT_DATA_ENCRYPTED ) { fprintf(stderr, "FIEMAP_EXTENT not Implemented\n"); exit(1); }

			table.push_back({extent->fe_logical, extent->fe_length});

                        if (  (extent->fe_flags & FIEMAP_EXTENT_LAST) and (i+1 != extent_count) )
			{
				fprintf(stderr, "logic error\n");
				exit(1);
			}

                        if ( !(extent->fe_flags & FIEMAP_EXTENT_LAST) and (i+1 == extent_count) )
			{
				fprintf(stderr, "logic error\n");
				exit(1);
			}
		}
	}

	return table;
}

void sparse::write_header(int fd, const header &header)
{
	uint8_t buffer[24 + 8 + 8 + 8];

	memset(buffer, 0, sizeof(buffer));

	memcpy(buffer, "packed sparse file v1\0\0\0", 24);

	pack_be_uint(header.n_table_entries, buffer + 24 +  0, 4);
	pack_be_uint(header. unsparsed_size, buffer + 24 +  8, 8);
	pack_be_uint(header.   sparsed_size, buffer + 24 + 16, 8);

	int n = write(fd, buffer, sizeof(buffer));

	if ( n < 0 )
	{
		perror("Could not write header");
		exit(1);
	}

	if ( n != sizeof(buffer) )
	{
		fprintf(stderr, "Could not write complete header\n");
		exit(1);
	}
}

void sparse::write_table (int fd, const std::vector<table_entry> &table)
{
	size_t   buffer_size = table.size() * 2 * 8;
	uint8_t *buffer      = (uint8_t*)malloc(buffer_size);

	size_t idx = 0;
	for(const auto &i: table)
	{
		pack_be_uint(i.logic_offset, buffer + idx * 2 * 8 + 0, 8);
		pack_be_uint(i.logic_size  , buffer + idx * 2 * 8 + 8, 8);

		idx += 1;
	}

	int n = write(fd, buffer, buffer_size);

	if ( n < 0 )
	{
		perror("Could not write table");
		exit(1);
	}

	if ( (size_t)n != buffer_size )
	{
		fprintf(stderr, "Could not write complete table\n");
		exit(1);
	}

	free(buffer);
}

void sparse::read_header(int fd, sparse::header &header)
{
	uint8_t buffer[24 + 8 + 8 + 8];

	int n = read(fd, buffer, sizeof(buffer));

	if ( n < 0 )
	{
		perror("Could not read header");
		exit(1);
	}

	if ( n != sizeof(buffer) )
	{
		fprintf(stderr, "Could not read complete header\n");
		exit(1);
	}

	if ( memcmp(buffer, "packed sparse file v1\0\0\0", 24) != 0 )
	{
		fprintf(stderr, "Invalid header version\n");
		exit(1);
	}

	header.version         = 1;
	header.n_table_entries = unpack_be_uint<uint32_t>(buffer + 24 +  0, 4);
	header. unsparsed_size = unpack_be_uint<uint64_t>(buffer + 24 +  8, 8);
	header.   sparsed_size = unpack_be_uint<uint64_t>(buffer + 24 + 16, 8);
}

void sparse::read_table (int fd, const header &header, std::vector<table_entry> &table)
{
	table.clear();

	size_t   buffer_size = header.n_table_entries * 2 * 8;
	uint8_t *buffer      = (uint8_t*)malloc(buffer_size);

	int n = read(fd, buffer, buffer_size);

	if ( n < 0 )
	{
		perror("Could not read table");
		exit(1);
	}

	if ( (size_t)n != buffer_size )
	{
		fprintf(stderr, "Could not read complete table\n");
		exit(1);
	}

	for(size_t i = 0; i < header.n_table_entries; ++i)
	{
		uint64_t logic_offset = unpack_be_uint<uint64_t>(buffer + i * 2 * 8 + 0, 8);
		uint64_t logic_size   = unpack_be_uint<uint64_t>(buffer + i * 2 * 8 + 8, 8);
		table.push_back({logic_offset, logic_size});
	}
}

