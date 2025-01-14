/*
 * Copyright (c) 2021, Arm Limited or its affiliates. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "val_framework.h"
#include "val_interfaces.h"

/**
 * @brief - Allocates requested buffer size in bytes in a contiguous memory.
 *
 * @param  size - allocation size in bytes
 * @return - Returns pointer to allocated memory/NULL if it fails.
**/
void *val_memory_alloc(size_t size)
{
    return pal_memory_alloc(size);
}

/**
 * @brief - Free the allocated memory.
 *
 * @param address - Buffer the base address of the memory range to be freed.
 * @return - Returns success/error status code.
**/
uint32_t val_memory_free(void *address, size_t size)
{
    return pal_memory_free(address, size);
}

/**
  @brief  Compare the two input buffer content
  @param  src   - Source buffer to be compared
  @dest   dest  - Destination buffer to be compared
  @return Zero if buffer content are equal, else non-zero
**/
int val_memcmp(void *src, void *dest, size_t len)
{
  return pal_memcmp(src, dest, len);
}

/**
  @brief  Copy src buffer content into dst
  @param  dst  - Pointer to the destination buffer
  @param  src  - Pointer to the source buffer
  @param  len  - Number of bytes in buffer to copy
  @return dst
**/
void *val_memcpy(void *dst, const void *src, size_t len)
{
  return pal_memcpy(dst, src, len);
}

/**
  @brief  Fill a buffer with a known specified input value
  @param  dst   - Pointer to the buffer to fill
  @param  value - Value to fill buffer with
  @param  count  - Number of bytes in buffer to fill
  @return None
**/
void *val_memset(void *dst, int val, size_t count)
{
  pal_memset(dst, val, count);
  return dst;
}

/**
  @brief  Appends the string pointed to by str2 to the
          end of the string pointed to by str1
  @param  str1  - Pointer of destination string
  @param  str2  - Pointer of string to be appended
  @param  output_buff_size - Size of str1 string
  @return Pointer of destination string
**/

char *val_strcat(char *str1, char *str2, size_t output_buff_size)
{
  size_t length = 0, i;

  while (str1[length] != '\0')
  {
    ++length;
  }

  /* Concatenate str1 to str2 */
  for (i = 0; str2[i] != '\0'; ++i, ++length)
  {
     if (length < output_buff_size)
     {
         str1[length] = str2[i];
     }
     else
     {
         VAL_PANIC("\tError: Buffer overflow detected\n");
     }
  }

  str1[length] = '\0';

  return str1;
}

/**
 * @brief       - Returns the physical address of the input virtual address
 * @param       - va: Virtual address of the memory to be converted
 * @return      - Physical address(PA or IPA)
**/
void *val_mem_virt_to_phys(void *va)
{
  return pal_mem_virt_to_phys(va);
}

#ifdef TARGET_LINUX
/**
 * @brief Map the given region into page table
 * @param mem_desc - memory addrs and attributes needed for page table mapping.
 * @return status
**/
uint32_t val_mem_map_pgt(memory_region_descriptor_t *mem_desc)
{
    return VAL_SUCCESS;
}
#endif
