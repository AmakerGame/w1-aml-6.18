#ifndef _LINUX_AMLOGIC_WLAN_PLAT_H
#define _LINUX_AMLOGIC_WLAN_PLAT_H

/* Minimal stub for mainline build: only what aml_static_buf.c needs */
void *wifi_mem_prealloc(int section, unsigned long size);
void wifi_mem_free(void *ptr);

#endif
