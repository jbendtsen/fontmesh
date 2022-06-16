#include <stdio.h>
#include <stdlib.h>
#include "../fontmesh.h"

typedef unsigned char u8;

int main(int argc, char **argv)
{
    if (argc != 3) {
        printf("fontmesh test\nConvert TTF to curved fontmesh file\nUsage:\n%s <input ttf> <output mfc>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f)
        return 2;

    fseek(f, 0, SEEK_END);
    int in_size = ftell(f);
    rewind(f);
    if (in_size < 0x1c)
        return 3;

    u8 *in_buf = malloc(in_size);
    fread(in_buf, 1, in_size, f);
    fclose(f);

    u8 *out_buf;
    int out_size;
    if (import_ttf(&out_buf, &out_size, in_buf, in_size) < 0)
        return 4;

    f = fopen(argv[2], "wb");
    fwrite(out_buf, 1, out_size, f);
    fclose(f);
    free(in_buf);
    free(out_buf);

    return 0;
}