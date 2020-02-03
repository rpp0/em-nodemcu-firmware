#ifndef DES_OPENSSL_H_
#define DES_OPENSSL_H_


#include "des_local.h"

int DES_encrypt1(DES_LONG* data, DES_key_schedule* ks, int enc);
void DES_ecb_encrypt(const_DES_cblock *input, DES_cblock *output,
                     DES_key_schedule *ks, int enc);
void DES_set_key_unchecked(const_DES_cblock *key, DES_key_schedule *schedule);

#endif
