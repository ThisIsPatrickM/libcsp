#pragma once

#include <csp/csp_interface.h>

/**
   Add interface to the list.

   @param[in] iface interface. The interface must remain valid as long as the application is running.
   @return #CSP_ERR_NONE on success, otherwise an error code.
*/
int csp_iflist_add(csp_iface_t * iface);

csp_iface_t * csp_iflist_get_by_name(const char *name);
csp_iface_t * csp_iflist_get_by_addr(uint16_t addr);
csp_iface_t * csp_iflist_get_by_subnet(uint16_t addr);

void csp_iflist_print(void);
csp_iface_t * csp_iflist_get(void);

/* Convert bytes to readable string */
int csp_bytesize(char *buffer, int buffer_len, unsigned long int bytes);

