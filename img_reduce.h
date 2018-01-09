#ifndef _IMGREDUCE_H
#define _IMGREDUCE_H

void __attribute__ ((constructor)) libinit_img_reduce();
int_fast8_t init_img_reduce();

long IMG_REDUCE_cleanbadpix_fast(const char *IDname, const char *IDbadpix_name, const char *IDoutname, int streamMode);

long IMG_REDUCE_cubesimplestat(const char *IDin_name);
int IMG_REDUCE_cubeprocess(const char *IDin_name);

#endif
