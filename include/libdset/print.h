/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBDSET_PRINT_H
#define LIBDSET_PRINT_H

#include <libdset/data.h>			/* enum dset_opt */

typedef int (*dset_printfn)(char *buf, unsigned int len,
			     const struct dset_data *data,
			     enum dset_opt opt, uint8_t env);

#ifdef __cplusplus
extern "C" {
#endif

extern int dset_print_type(char *buf, unsigned int len,
			    const struct dset_data *data,
			    enum dset_opt opt, uint8_t env);
extern int dset_print_number(char *buf, unsigned int len,
			      const struct dset_data *data,
			      enum dset_opt opt, uint8_t env);
extern int dset_print_name(char *buf, unsigned int len,
			    const struct dset_data *data,
			    enum dset_opt opt, uint8_t env);
extern int dset_print_domain(char *buf, unsigned int len,
			     const struct dset_data *data,
			     enum dset_opt opt, uint8_t env);
extern int dset_print_comment(char *buf, unsigned int len,
			     const struct dset_data *data,
			     enum dset_opt opt, uint8_t env);
extern int dset_print_skbmark(char *buf, unsigned int len,
			      const struct dset_data *data,
			      enum dset_opt opt, uint8_t env);
extern int dset_print_skbprio(char *buf, unsigned int len,
				const struct dset_data *data,
				enum dset_opt opt, uint8_t env);
extern int dset_print_flag(char *buf, unsigned int len,
			    const struct dset_data *data,
			    enum dset_opt opt, uint8_t env);
extern int dset_print_elem(char *buf, unsigned int len,
			    const struct dset_data *data,
			    enum dset_opt opt, uint8_t env);

#define dset_print_portnum	dset_print_number

extern int dset_print_data(char *buf, unsigned int len,
			    const struct dset_data *data,
			    enum dset_opt opt, uint8_t env);

#ifdef __cplusplus
}
#endif

#endif /* LIBDSET_PRINT_H */
