/*
 * Implements helper functions to set unix style permissions
 */

#include <permissions.h>

/* set permissions */
permissions_t set_permissions(int r, int w, int x)
{
    permissions_t p;

    if(!r && !w && !x)
        p = ___;
    else if(!r && !w && x)
        p = __X;
    else if(!r && w && !x)
        p = _W_;
    else if(!r && w && x)
        p = _WX;
    else if(r && !w && !x)
        p = R__;
    else if(r && !w && x)
        p = R_X;
    else if(r && w && !x)
        p = RW_;
    else {
        p = RWX;
    }

    return p;
}


int is_readable(permissions_t p)
{
    if( (p == R__) || (p == RW_) || (p == R_X) || (p == RWX) ) {
        return 1;
    }
    return 0;
}

int is_writeable(permissions_t p)
{
    if( (p == _W_) || (p == _WX) || (p == RW_) || (p == RWX) ) {
        return 1;
    }
    return 0;
}

int is_executable(permissions_t p)
{
    if( (p == __X) || (p == _WX) || (p == R_X) || (p == RWX) ) {
        return 1;
    }
    return 0;
}

