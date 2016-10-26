/*
 * Copyright (C) 2016 Tiago Gonçalves
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <cstdint>
#include <vector>

namespace sparse
{
	struct header
	{
		uint8_t  version;
		uint32_t n_table_entries;
		uint64_t unsparsed_size;
		uint64_t   sparsed_size;
	};

	struct table_entry
	{
		uint64_t logic_offset;
		uint64_t logic_size;
	};

	std::vector<table_entry> get_fiemap_entries(int fd);

	void write_header(int fd, const header                   &);
	void write_table (int fd, const std::vector<table_entry> &);

	void read_header(int fd,       header &                            );
	void read_table (int fd, const header &, std::vector<table_entry> &);
}

