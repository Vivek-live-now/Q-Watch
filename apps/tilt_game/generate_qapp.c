#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/qwatch_api.h"
#include "tilt_game.h"

int main(int argc, char** argv) {
    const char* out_path = (argc > 1) ? argv[1] : "apps/tilt_ball.qapp";
    FILE* fp = fopen(out_path, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open output file: %s\n", out_path);
        return 1;
    }

#if defined(__aarch64__)
    uint32_t code_payload[4] = {
        0x58000040, // ldr x0, 0x8
        0xd65f03c0, // ret
        0x00000000,
        0x00000000
    };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset = 8;
#elif defined(__x86_64__)
    uint8_t code_payload[11] = {
        0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, // movabs $imm64, %rax
        0xc3                                 // ret
    };
    uint32_t code_len = sizeof(code_payload);
    uint32_t reloc_offset = 2;
#else
    uint32_t code_payload[4] = { 0 };
    uint32_t code_len = 16;
    uint32_t reloc_offset = 4;
#endif

    const QAppHeader* src_hdr = get_tilt_game_header();
    uint32_t data_len = sizeof(QAppHeader);

    QAppFileHeader fhdr;
    memset(&fhdr, 0, sizeof(fhdr));
    fhdr.magic = QAPP_MAGIC;
    fhdr.api_version = QAPP_API_VERSION;
    fhdr.required_caps = (QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED);
    strncpy(fhdr.name, "Tilt Ball", sizeof(fhdr.name) - 1);
    strncpy(fhdr.version, "1.0.0", sizeof(fhdr.version) - 1);
    strncpy(fhdr.author, "007 Agent", sizeof(fhdr.author) - 1);
    fhdr.required_psram = 2048;

    fhdr.code_offset = sizeof(QAppFileHeader);
    fhdr.code_size = code_len;
    fhdr.data_offset = fhdr.code_offset + fhdr.code_size;
    fhdr.data_size = data_len;
    fhdr.bss_size = 64;
    fhdr.reloc_offset = fhdr.data_offset + fhdr.data_size;
    fhdr.reloc_count = 1;
    fhdr.entry_offset = 0;

    fwrite(&fhdr, sizeof(fhdr), 1, fp);
    fwrite(code_payload, 1, code_len, fp);
    fwrite(src_hdr, 1, data_len, fp);

    QAppReloc reloc;
    reloc.section = 0;
    reloc.type = QRELOC_DATA_ADDR;
    reloc.reserved = 0;
    reloc.offset = reloc_offset;
    fwrite(&reloc, sizeof(reloc), 1, fp);

    fclose(fp);
    printf("Successfully generated relocatable %s (%zu bytes)\n",
           out_path, sizeof(fhdr) + code_len + data_len + sizeof(reloc));
    return 0;
}
