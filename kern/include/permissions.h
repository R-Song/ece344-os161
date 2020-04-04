
#ifndef _PERMISSIONS_H_
#define _PERMISSIONS_H_

/* 
 * Implementation of unix style permissions
 */
typedef enum {
	___,
	__X,
	_W_,
	_WX,
	R__,
	R_X,
	RW_,
	RWX,
} permissions_t;

/* returns new permissions */
permissions_t set_permissions(int r, int w, int x);

/* does current permissions allow for reading */
int is_readable(permissions_t p);

/* does current permissions allow for writing */
int is_writeable(permissions_t p);

/* does current permissions allow for executing */
int is_executable(permissions_t p);


#endif /* _PERMISSIONS_H_ */

