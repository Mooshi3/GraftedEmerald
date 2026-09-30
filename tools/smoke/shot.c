#include <mgba/flags.h>
// Tiny scripted mGBA runner: ./shot rom.gba script.txt
// Script lines: "wait N" | "press KEYS [hold]" (KEYS like A, START, UP, A+B) | "mash KEY times gap" | "shot file.png"
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/core/serialize.h>
#include <fcntl.h>
#include <mgba/gba/interface.h>
#include <mgba-util/vfs.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct mCore *core;
static void nolog(struct mLogger *l, int c, enum mLogLevel lv, const char *f, va_list a) {
    if (getenv("SHOTLOG") && 1) { vfprintf(stderr, f, a); fputc('\n', stderr); }
}
static struct mLogger logger = { .log = nolog };

static void frames(int n, int keys) { core->setKeys(core, keys); for (int i = 0; i < n; i++) core->runFrame(core); }

static int parsekeys(const char *s)
{
    int k = 0; char buf[64]; strncpy(buf, s, 63); buf[63] = 0;
    for (char *t = strtok(buf, "+"); t; t = strtok(NULL, "+")) {
        if (!strcmp(t,"A")) k |= 1<<0; else if (!strcmp(t,"B")) k |= 1<<1;
        else if (!strcmp(t,"SELECT")) k |= 1<<2; else if (!strcmp(t,"START")) k |= 1<<3;
        else if (!strcmp(t,"RIGHT")) k |= 1<<4; else if (!strcmp(t,"LEFT")) k |= 1<<5;
        else if (!strcmp(t,"UP")) k |= 1<<6; else if (!strcmp(t,"DOWN")) k |= 1<<7;
        else if (!strcmp(t,"R")) k |= 1<<8; else if (!strcmp(t,"L")) k |= 1<<9;
    }
    return k;
}

static mColor *vbuf; static unsigned w, h;
static void shot(const char *path)
{
    FILE *f = fopen(path, "wb");
    png_structp p = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    png_infop info = png_create_info_struct(p);
    png_init_io(p, f);
    png_set_IHDR(p, info, w, h, 8, PNG_COLOR_TYPE_RGB, 0, 0, 0);
    png_write_info(p, info);
    unsigned char row[240*3];
    for (unsigned y = 0; y < h; y++) {
        for (unsigned x = 0; x < w; x++) {
            uint32_t c = vbuf[y*w+x];
            row[x*3] = c & 0xFF; row[x*3+1] = (c>>8)&0xFF; row[x*3+2] = (c>>16)&0xFF;
        }
        png_write_row(p, row);
    }
    png_write_end(p, 0); fclose(f);
}

int main(int argc, char **argv)
{
    mLogSetDefaultLogger(&logger);
    core = mCoreFind(argv[1]);
    core->init(core);
    mCoreInitConfig(core, NULL);
    w = 240; h = 160;
    vbuf = malloc(w*h*sizeof(mColor));
    core->setVideoBuffer(core, vbuf, w);
    mCoreLoadFile(core, argv[1]);
    core->reset(core);
    FILE *s = fopen(argv[2], "r"); char line[256];
    while (fgets(line, sizeof line, s)) {
        char a[64] = {0}, b[64] = {0}; int n = 0, m = 0;
        if (sscanf(line, "wait %d", &n) == 1) frames(n, 0);
        else if (sscanf(line, "mash %63s %d %d", a, &n, &m) == 3) for (int i = 0; i < n; i++) { frames(2, parsekeys(a)); frames(m, 0); }
        else if (sscanf(line, "press %63s %d", a, &n) >= 1) { frames(n ? n : 2, parsekeys(a)); frames(1, 0); }
        else if (sscanf(line, "shot %63s", b) == 1) shot(b);
        else if (sscanf(line, "save %63s", b) == 1) { struct VFile *vf = VFileOpen(b, O_CREAT|O_TRUNC|O_RDWR); mCoreSaveStateNamed(core, vf, SAVESTATE_SAVEDATA); vf->close(vf); }
        else if (sscanf(line, "load %63s", b) == 1) { struct VFile *vf = VFileOpen(b, O_RDONLY); mCoreLoadStateNamed(core, vf, SAVESTATE_SAVEDATA); vf->close(vf); }
    }
    return 0;
}
