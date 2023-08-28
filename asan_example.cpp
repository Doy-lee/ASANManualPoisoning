
#include <sanitizer/asan_interface.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define IsPowerOfTwo(value, pot) ((((size_t)value) & (((size_t)pot) - 1)) == 0)
#define AsanAlignment 8

#define PrintByteSpacing(space) \
    if (index) \
        printf(space); \
    if (index && (index % 8 == 0)) \
        printf("| ")

static void PrintByteArray(char const *array, size_t array_size)
{
    printf("   Byte Array                    ");
    for (size_t index = 0 ; index < array_size; index++) {
        PrintByteSpacing(" ");
        printf("%02zu", index);
    }
    printf("\n");
}

static void PrintPoisonedBytes(int step, char const *array, size_t array_size)
{
    printf("%d. __asan_address_is_poisoned    ", step);
    for (size_t index = 0 ; index < array_size; index++) {
        PrintByteSpacing("  ");
        printf("%c", __asan_address_is_poisoned(array + index) ? 'x' : ' ');
    }
    printf("\n");
}

static void PrintPoisonMemoryIndex(int step, char const *array, size_t array_size, size_t poison_index)
{
    printf("%d. __asan_poison_memory_region   ", step);
    for (size_t index = 0; index < array_size; index++) {
        PrintByteSpacing(" ");
        printf("%c ", index == poison_index ? 'x' : ' ');
    }
    printf("\n");
}

static void PrintPoisonMemoryRegion(int step, char const *array, size_t array_size, size_t poison_start_index, size_t poison_end_index)
{
    printf("%d. __asan_poison_memory_region   ", step);
    for (size_t index = 0; index < array_size; index++) {
        PrintByteSpacing(" ");
        printf("%c ", index >= poison_start_index && index <= poison_end_index ? 'x' : ' ');
    }
    printf("\n");
}

int main()
{
    printf(
        "# ASAN Manual Poisoning\n"
        "\n"
        "## TLDR\n"
        "\n"
        "`__asan_poison_memory_region(ptr, size)`\n"
        "\n"
        "Poisons the byte region `[ptr, AlignDown(ptr+size, 8))`\n"
        "\n"
        "`__asan_unpoison_memory_region(ptr, size)`\n"
        "\n"
        "Unpoisons the byte region `[AlignDown(ptr, 8), ptr+size)`\n"
        "\n"
        "Use the provided macros that are conditionally enabled if ASAN is\n"
        "defined from `<sanitizer/asan_interface.h>`.\n"
        "\n"
        "```\n"
        "ASAN_POISON_MEMORY_REGION(addr, size)\n"
        "ASAN_UNPOISON_MEMORY_REGION(addr, size)\n"
        "```\n"
        "\n"
        "If in doubt, use `__asan_address_is_poisoned` to sanity check the\n"
        "ranges requested to be un/poisoned to avoid potential gaps in\n"
        "marked-up memory that may lead to undetected read/writes.\n"
        "\n"
        "## Overview\n"
        "\n"
        "ASAN provides a way to manually markup ranges of bytes to\n"
        "prohibit or permit reads to those addresses. In\n"
        "`<sanitizer/asan_interface.h>` there's a vague brief mention to\n"
        "alignment requirements for the poison API:\n"
        "\n"
        "```cpp\n"
        "/// ... This function is not guaranteed to poison the entire region -\n"
        "/// it could poison only a subregion of <c>[addr, addr+size)</c> due to ASan\n"
        "/// alignment restrictions.\n"
        "void __asan_poison_memory_region(void const volatile *addr, size_t size);\n"
        "\n"
        "/// ... This function could unpoison a super-region of <c>[addr, addr+size)</c> due\n"
        "/// to ASan alignment restrictions.\n"
        "void __asan_unpoison_memory_region(void const volatile *addr, size_t size);\n"
        "```\n"
        "\n"
        "There's another small foot-note in Google's "
        "[AddressSanitizerManualPoisoning](https://github.com/google/"
        "sanitizers/wiki/AddressSanitizerManualPoisoning)\n"
        "documentation that states:\n"
        "\n"
        "```\n"
        "If you have a custom allocation arena, the typical workflow would be\n"
        "to poison the entire arena first, and then unpoison allocated chunks\n"
        "of memory leaving poisoned redzones between them. The allocated\n"
        "chunks should start with 8-aligned addresses.\n"
        "```\n"
        "\n"
        "So then this repository runs some simple tests to clarify the behaviour\n"
        "of the API on un/aligned addresses at various sizes without having\n"
        "to dig into source code or read the [ASAN paper](https://static."
        "googleusercontent.com/media/research.google.com/en/pubs/archive/"
        "37752.pdf).\n"
        "\n"
        "We use a stack-allocated 16 byte array and test un/poisoning\n"
        "various ranges of bytes from different alignments to clarify the\n"
        "poisoning behaviour of the API. This reveals that calling the API\n"
        "haphazardly, unaligned or straddling boundaries can lead to gaps in\n"
        "poisoned memory and hide potential leaks (as also demonstrated in\n"
        "[Manual ASAN poisoning and alignment]"
        "(https://github.com/mcgov/asan_alignment_example)).\n"
        "\n"
        "## References\n"
        "\n"
        "- [Manual ASAN poisoning and alignment](https://github.com/mcgov/asan_alignment_example) example by `mcgov`\n"
        "- [Address Sanitizer: A Fast Address Sanity Checker](https://static.googleusercontent.com/media/research.google.com/en//pubs/archive/37752.pdf)\n"
        "- [sanitizer/asan_interface.h](https://github.com/llvm-mirror/compiler-rt/blob/master/include/sanitizer/asan_interface.h)\n"
        "\n"
        "## Raw Test Results\n"
        "\n"
        "Here we poison a sliding window of 7 bytes to demonstrate that ASAN\n"
        "poisoning will only poison the byte region if the region meets an 8\n"
        "byte aligned address. It will only poison bytes up to the boundary,\n"
        "any bytes that straddle the boundary that do not hit the next 8 byte\n"
        "boundary are not poisoned.\n"
        "\n");

    uint32_t const ASAN_ALIGNMENT = 8;
    uint32_t const REGION_WINDOW  = 7;
    char array[16]                = {};

    printf("```\n");
    for (size_t poison_index = 0; poison_index < sizeof(array) - (REGION_WINDOW - 1); poison_index++) {
        assert(IsPowerOfTwo(array, ASAN_ALIGNMENT));

        // NOTE: Print byte array ==================================================================

        PrintByteArray(array, sizeof(array));

        // NOTE: Poison the array ==================================================================

        __asan_poison_memory_region(&array[poison_index], REGION_WINDOW);
        PrintPoisonMemoryRegion(1 /*step*/, array, sizeof(array), poison_index, poison_index + (REGION_WINDOW - 1));

        // NOTE: Print the poison-ed bytes =========================================================

        PrintPoisonedBytes(2 /*step*/, array, sizeof(array));

        // NOTE: Cleanup ===========================================================================

        __asan_unpoison_memory_region(array, sizeof(array));
        printf("\n");
    }
    printf("```\n");

    printf(
        "Now we demonstrate that unpoisoning 1 byte in the 8 byte window\n"
        "will unpoison all bytes prior to it up until the previous 8 byte \n"
        "boundary.\n"
        "\n");

    printf("```\n");
    for (size_t unpoison_index = 0; unpoison_index < sizeof(array); unpoison_index++) {
        assert(IsPowerOfTwo(array, ASAN_ALIGNMENT));

        // NOTE: Print byte array ==================================================================

        PrintByteArray(array, sizeof(array));

        // NOTE: Poison the array ==================================================================

        __asan_poison_memory_region(array, sizeof(array));
        PrintPoisonMemoryRegion(1 /*step*/, array, sizeof(array), 0, sizeof(array) - 1);

        // NOTE: Print the poison-ed bytes =========================================================

        PrintPoisonedBytes(2 /*step*/, array, sizeof(array));

        // NOTE: Unpoison byte at each position separately =========================================

        printf("3. __asan_unpoison_memory_region ");
        __asan_unpoison_memory_region(array + unpoison_index, 1);

        for (size_t index = 0 ; index < sizeof(array); index++) {
            PrintByteSpacing("  ");
            printf("%c", index == unpoison_index ? 'x' : ' ');
        }
        printf("\n");

        // NOTE: Print the poison-ed bytes =========================================================

        printf("4. __asan_address_is_poisoned    ");
        for (size_t index = 0 ; index < sizeof(array); index++) {
            PrintByteSpacing("  ");
            printf("%c", __asan_address_is_poisoned(array + index) ? 'x' : ' ');
        }
        printf("\n\n");

        // NOTE: Cleanup ===========================================================================

        __asan_unpoison_memory_region(array, sizeof(array));
    }
    printf("```\n");

    printf(
        "Unpoisoning across 8 byte boundaries may lead to undesired\n"
        "behaviour, with all bytes on the left side of the boundary being\n"
        "unpoisoned\n\n");

    printf("```\n");
    {
        // NOTE: Print byte array ==================================================================

        char array[16] = {};
        PrintByteArray(array, sizeof(array));

        // NOTE: Poison the array ==================================================================

        __asan_poison_memory_region(array, sizeof(array));
        PrintPoisonMemoryRegion(1 /*step*/, array, sizeof(array), 0, sizeof(array) - 1);

        // NOTE: Print the poison-ed bytes =========================================================

        PrintPoisonedBytes(2 /*step*/, array, sizeof(array));

        // NOTE: Unpoison across the 8 byte boundary ===============================================

        printf("3. __asan_unpoison_memory_region ");
        uint32_t const start_poison_index = 7;
        uint32_t const bytes_to_unpoison  = 2;
        uint32_t const end_poison_index   = start_poison_index + (bytes_to_unpoison - 1);
        __asan_unpoison_memory_region(array + start_poison_index, bytes_to_unpoison);

        for (size_t index = 0 ; index < sizeof(array); index++) {
            PrintByteSpacing("  ");
            printf("%c", index >= start_poison_index && index <= end_poison_index ? 'x' : ' ');
        }
        printf("\n");

        // NOTE: Print the poison-ed bytes =========================================================

        PrintPoisonedBytes(4 /*step*/, array, sizeof(array));
    }
    printf("```\n");
    return 0;
}
