#include "memory_layout.h"

    .resource_table SLAVE00_SOURCE_TABLE_ADDR: {
	. = ALIGN(4);
	*(.resource_table)
    }
