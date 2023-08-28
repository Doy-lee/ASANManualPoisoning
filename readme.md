# ASAN Manual Poisoning

## TLDR

`__asan_poison_memory_region(ptr, size)`

Poisons the byte region `[ptr, AlignDown(ptr+size, 8))`

`__asan_unpoison_memory_region(ptr, size)`

Unpoisons the byte region `[AlignDown(ptr, 8), ptr+size)`

Use the provided macros that are conditionally enabled if ASAN is
defined from `<sanitizer/asan_interface.h>`.

```
ASAN_POISON_MEMORY_REGION(addr, size)
ASAN_UNPOISON_MEMORY_REGION(addr, size)
```

If in doubt, use `__asan_address_is_poisoned` to sanity check the
ranges requested to be un/poisoned to avoid potential gaps in
marked-up memory that may lead to undetected read/writes.

## Overview

ASAN provides a way to manually markup ranges of bytes to
prohibit or permit reads to those addresses. There's a short
foot-note in Google's [AddressSanitizerManualPoisoning](https://github.com/google/sanitizers/wiki/AddressSanitizerManualPoisoning)
documentation that states:

```
If you have a custom allocation arena, the typical workflow would be
to poison the entire arena first, and then unpoison allocated chunks
of memory leaving poisoned redzones between them. The allocated
chunks should start with 8-aligned addresses.
```

This repository runs some simple tests to clarify the behaviour of
the API on un/aligned addresses at various sizes without having
to dig into source code or read the [ASAN paper](https://static.googleusercontent.com/media/research.google.com/en/pubs/archive/37752.pdf).

We use a stack-allocated 16 byte array and test un/poisoning
various ranges of bytes from different alignments to clarify the
poisoning behaviour of the API. This reveals that calling the API
haphazardly, unaligned or straddling boundaries can lead to gaps in
poisoned memory and hide potential leaks (as also demonstrated in
[Manual ASAN poisoning and alignment](https://github.com/mcgov/asan_alignment_example)).

## References

- [Manual ASAN poisoning and alignment](https://github.com/mcgov/asan_alignment_example) example by `mcgov`
- [Address Sanitizer: A Fast Address Sanity Checker](https://static.googleusercontent.com/media/research.google.com/en/pubs/archive/37752.pdf)
- [sanitizer/asan_interface.h](https://github.com/llvm-mirror/compiler-rt/blob/master/include/sanitizer/asan_interface.h)

## Raw Test Results

Here we poison a sliding window of 7 bytes to demonstrate that ASAN
poisoning will only poison the byte region if the region meets an 8
byte aligned address. It will only poison bytes up to the boundary,
any bytes that straddle the boundary that do not hit the next 8 byte
boundary are not poisoned.

```
   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x     |                        
2. __asan_address_is_poisoned                            |                       

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region      x  x  x  x  x  x  x  |                        
2. __asan_address_is_poisoned       x  x  x  x  x  x  x  |                       

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region         x  x  x  x  x  x  | x                      
2. __asan_address_is_poisoned          x  x  x  x  x  x  |                       

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region            x  x  x  x  x  | x  x                   
2. __asan_address_is_poisoned             x  x  x  x  x  |                       

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region               x  x  x  x  | x  x  x                
2. __asan_address_is_poisoned                x  x  x  x  |                       

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region                  x  x  x  | x  x  x  x             
2. __asan_address_is_poisoned                   x  x  x  |                       

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region                     x  x  | x  x  x  x  x          
2. __asan_address_is_poisoned                      x  x  |                       

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region                        x  | x  x  x  x  x  x       
2. __asan_address_is_poisoned                         x  |                       

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region                           | x  x  x  x  x  x  x    
2. __asan_address_is_poisoned                            |                       

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region                           |    x  x  x  x  x  x  x 
2. __asan_address_is_poisoned                            |    x  x  x  x  x  x  x

```
Now we demonstrate that unpoisoning 1 byte in the 8 byte window
will unpoison all bytes prior to it up until the previous 8 byte 
boundary.

```
   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region x                       |                       
4. __asan_address_is_poisoned       x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region    x                    |                       
4. __asan_address_is_poisoned          x  x  x  x  x  x  | x  x  x  x  x  x  x  x

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region       x                 |                       
4. __asan_address_is_poisoned             x  x  x  x  x  | x  x  x  x  x  x  x  x

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region          x              |                       
4. __asan_address_is_poisoned                x  x  x  x  | x  x  x  x  x  x  x  x

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region             x           |                       
4. __asan_address_is_poisoned                   x  x  x  | x  x  x  x  x  x  x  x

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region                x        |                       
4. __asan_address_is_poisoned                      x  x  | x  x  x  x  x  x  x  x

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region                   x     |                       
4. __asan_address_is_poisoned                         x  | x  x  x  x  x  x  x  x

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region                      x  |                       
4. __asan_address_is_poisoned                            | x  x  x  x  x  x  x  x

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region                         | x                     
4. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  |    x  x  x  x  x  x  x

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region                         |    x                  
4. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  |       x  x  x  x  x  x

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region                         |       x               
4. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  |          x  x  x  x  x

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region                         |          x            
4. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  |             x  x  x  x

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region                         |             x         
4. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  |                x  x  x

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region                         |                x      
4. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  |                   x  x

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region                         |                   x   
4. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  |                      x

   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region                         |                      x
4. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  |                       

```
Unpoisoning across 8 byte boundaries may lead to undesired
behaviour, with all bytes on the left side of the boundary being
unpoisoned

```
   Byte Array                    00 01 02 03 04 05 06 07 | 08 09 10 11 12 13 14 15
1. __asan_poison_memory_region   x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x 
2. __asan_address_is_poisoned    x  x  x  x  x  x  x  x  | x  x  x  x  x  x  x  x
3. __asan_unpoison_memory_region                      x  | x                     
4. __asan_address_is_poisoned                            |    x  x  x  x  x  x  x
```
