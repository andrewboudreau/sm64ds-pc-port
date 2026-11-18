/* Host player settings: the game's half of settings.json. See
   port/hal/host_settings.h for what the file is and why it is not an
   environment variable.

   The parser here is deliberately small. settings.json is a flat object of
   scalars written by System.Text.Json, and the only thing the game needs
   out of it is a handful of values; pulling in a JSON library to read three
   booleans would be the wrong trade. What it does have to get right is the
   failure mode: anything it cannot make sense of has to land on the
   defaults silently, because the file sits in a folder players edit by
   hand and a half-saved one must not stop the game from starting. */

#include "hal/host_settings.h"

/* run mg16 lane MP2: the instance suffix on the sibling temp file. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

/* settings.json is a small hand-editable file. Anything bigger than this is
   not the launcher's, so it is treated as malformed. */
const long SETTINGS_MAX = 64 * 1024;

/* ASCII case-insensitive compare of exactly n bytes. The launcher
   deserializes with PropertyNameCaseInsensitive, so the game matches keys
   the same way and a hand-typed "swapcameraturndirection" still works. */
int ieq(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return 0;
    }
    return 1;
}


/* Whole file into a NUL-terminated buffer the caller frees. Null on any
   failure at all, which every caller reads as "use the defaults". */
char *slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long n = ftell(f);
    if (n < 0 || n > SETTINGS_MAX) { fclose(f); return 0; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return 0; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

/* Cheap malformed test: the launcher writes one JSON object, so the first
   and last non-space characters have to be the braces. This is what turns a
   truncated write or a half-finished hand edit into "use the defaults"
   rather than into a half-read setting. It is not a JSON validator and is
   not trying to be. A file that passes this and is still broken simply will
   not yield a key to match, which lands on the defaults as well. */
int looks_like_json_object(const char *s)
{
    const char *a = s;
    while (*a == ' ' || *a == '\t' || *a == '\r' || *a == '\n') ++a;
    /* System.Text.Json writes UTF-8; a BOM is legal in the wild. */
    if ((unsigned char)a[0] == 0xEF && (unsigned char)a[1] == 0xBB &&
        (unsigned char)a[2] == 0xBF) {
        a += 3;
        while (*a == ' ' || *a == '\t' || *a == '\r' || *a == '\n') ++a;
    }
    if (*a != '{') return 0;
    const char *b = a + strlen(a);
    while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\r' ||
                     b[-1] == '\n'))
        --b;
    return b > a && b[-1] == '}';
}

/* Find `key` used as an object KEY and return a pointer at the first
   character of its value, or null. Every reader below is built on this, so
   there is one notion of "used as an object KEY" in this file rather than
   one per type.

   Strings that are values rather than keys are stepped over, so a credit
   name that happens to contain the key's spelling cannot be mistaken for
   the key itself. */
const char *json_value(const char *s, const char *key)
{
    const size_t klen = strlen(key);
    for (const char *p = s; *p; ++p) {
        if (*p != '"') continue;
        const char *name = p + 1;
        const char *end = name;
        /* a backslash pair is stepped over and the SAME string goes on,
           the idiom json_value_end uses: stopping at the backslash used to
           leave the scanner inside the string, so every key after a value
           holding one read as absent (review of lane PADCAL) */
        while (*end && *end != '"') end += (*end == '\\' && end[1]) ? 2 : 1;
        if (*end != '"') break;
        const int is_key = (size_t)(end - name) == klen && ieq(name, key, klen);
        const char *q = end + 1;
        while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') ++q;
        if (*q != ':') { p = end; continue; }
        ++q;
        while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') ++q;
        if (is_key) return q;
        p = q - 1;
    }
    return 0;
}

/* One past the last character of the value token starting at `v`. A quoted
   string ends at its closing quote; anything else ends at the first
   separator. Only ever asked about the scalars this file writes, so it does
   not need to walk a nested object or array. */
const char *json_value_end(const char *v)
{
    if (*v == '"') {
        const char *p = v + 1;
        while (*p && *p != '"') p += (*p == '\\' && p[1]) ? 2 : 1;
        return *p ? p + 1 : p;
    }
    /* an array or object ends at its matching bracket, strings stepped over,
       so PadLayouts (the one non-scalar this file writes) can be replaced
       whole by json_set. Unbalanced text ends at the end of the string. */
    if (*v == '[' || *v == '{') {
        int depth = 0;
        const char *p = v;
        while (*p) {
            if (*p == '"') {
                ++p;
                while (*p && *p != '"') p += (*p == '\\' && p[1]) ? 2 : 1;
                if (!*p) break;
            } else if (*p == '[' || *p == '{') {
                ++depth;
            } else if (*p == ']' || *p == '}') {
                if (--depth == 0) return p + 1;
            }
            ++p;
        }
        return p;
    }
    {
        const char *p = v;
        while (*p && *p != ',' && *p != '}' && *p != ']' && *p != ' ' &&
               *p != '\t' && *p != '\r' && *p != '\n')
            ++p;
        return p;
    }
}

/* The JSON true/false after `key`. Returns dflt unless a real boolean is
   there, so a key present but half-edited reads as absent. */
int json_bool(const char *s, const char *key, int dflt)
{
    const char *v = json_value(s, key);
    if (!v) return dflt;
    /* length before the compare: ieq reads a fixed count, so a value shorter
       than the literal must never reach it */
    const size_t vlen = strlen(v);
    if (vlen >= 4 && ieq(v, "true", 4)) return 1;
    if (vlen >= 5 && ieq(v, "false", 5)) return 0;
    return dflt;
}

/* Find `key` used as an object KEY and read a decimal integer after it.
   Returns dflt unless digits (optionally signed) are actually there, so a
   key a player half-edited lands on the default like every other reader. */
int json_int(const char *s, const char *key, int dflt)
{
    const char *v = json_value(s, key);
    if (!v) return dflt;
    if (*v == '"') ++v;              /* "16" is as good as 16 here */
    {
        const char *p = v;
        if (*p == '-' || *p == '+') ++p;
        if (*p < '0' || *p > '9') return dflt;
    }
    return (int)strtol(v, 0, 10);
}

/* THE ONE PLACE THE ASPECT BAND IS SPELLED OUT. Both readers of the key -- the
   settings file and the SM64DS_ASPECT override -- come through here, so the file
   and the environment cannot disagree about what 9.0 means.

     not a positive number   -> 0, the explicit native 4:3 sentinel. Covers
                                absent, unparseable, 0 itself, negatives and NaN
                                (the !(a > 0) spelling catches NaN; the test
                                written the other way round would not).
     positive                -> clamped into [1.0, 4.0]. 1.0 is square; below
                                1.0 is a portrait picture the HUD has no layout
                                for.

                                THE CEILING WAS 3.0 AND 3.0 WAS WRONG. It was
                                picked as "wider than any shipping monitor",
                                and 32:9 monitors exist: 32:9 is 3.5555556, so
                                the old ceiling took a super-ultrawide panel's
                                own ratio and quietly letterboxed it to 3.0.
                                4.0 is still a picture -- the DS's vertical
                                field is a slot long before the arithmetic
                                stops working -- and the arithmetic's own limit
                                is higher still and lives elsewhere: the
                                stacked sub-screen presentation scales by the
                                INTEGER active_h / 192 (ntr/ppu_sub.cpp), which
                                must stay at least 1, so no ceiling above
                                1024/192 = 5.33 is safe at all. 4.0 covers
                                32:9 with room to spare and leaves that scale
                                at 1. */
double aspect_sanitise(double a)
{
    if (!(a > 0.0)) return 0.0;
    if (a < 1.0) return 1.0;
    if (a > 4.0) return 4.0;
    return a;
}

/* Find `key` used as an object KEY and read a DECIMAL FRACTION after it. The
   same contract as json_int one function up -- dflt unless a number is really
   there -- but the value is a ratio, so it cannot be an int. Only reader is the
   Aspect key. strtod is locale-sensitive in principle; this process never calls
   setlocale, so the C locale's '.' is the separator, which is what a settings
   file written by the launcher contains. */
double json_num(const char *s, const char *key, double dflt)
{
    const char *v = json_value(s, key);
    if (!v) return dflt;
    if (*v == '"') ++v;              /* "1.7777778" is as good as 1.7777778 */
    {
        const char *p = v;
        if (*p == '-' || *p == '+') ++p;
        /* a leading '.' is input a player might plausibly type */
        if ((*p < '0' || *p > '9') && *p != '.') return dflt;
    }
    {
        char *end = 0;
        const double d = strtod(v, &end);
        if (end == v) return dflt;   /* nothing consumed: unparseable */
        return d;
    }
}

/* Copy `key`'s value token into out (a quoted string arrives unquoted, so a
   caller comparing against "analog" does not have to know how it was
   written). 1 when a value was there and fitted, 0 otherwise. */
int json_str(const char *s, const char *key, char *out, size_t cap)
{
    const char *v = json_value(s, key);
    if (!v || !cap) return 0;
    const char *e = json_value_end(v);
    if (*v == '"') { ++v; if (e > v) --e; }
    if ((size_t)(e - v) >= cap) return 0;
    memcpy(out, v, (size_t)(e - v));
    out[e - v] = '\0';
    return 1;
}

/* Where settings.json can be. The launcher puts the launcher exe, the game
   exe and settings.json in one bundle directory and launches the game with
   both its working directory and SM64DS_ASSET_ROOT set to that directory,
   so in a player's kit all three candidates are the same folder. They are
   tried in order of how literally each one means "next to the exe", and the
   first that EXISTS wins. A file that exists but will not parse falls back
   to the defaults rather than falling through to the next candidate,
   because a player who edited a file meant that file to be the one read. */
int find_settings(char *out, size_t cap)
{
#ifdef _WIN32
    {
        char exe[MAX_PATH];
        DWORD n = GetModuleFileNameA(NULL, exe, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            char *slash = strrchr(exe, '\\');
            char *fwd = strrchr(exe, '/');
            if (fwd && (!slash || fwd > slash)) slash = fwd;
            if (slash) {
                *slash = '\0';
                if (strlen(exe) + 16 < cap) {
                    snprintf(out, cap, "%s\\settings.json", exe);
                    FILE *f = fopen(out, "rb");
                    if (f) { fclose(f); return 1; }
                }
            }
        }
    }
#endif
    {
        const char *root = getenv("SM64DS_ASSET_ROOT");
        if (root && *root && strlen(root) + 16 < cap) {
            snprintf(out, cap, "%s/settings.json", root);
            FILE *f = fopen(out, "rb");
            if (f) { fclose(f); return 1; }
        }
    }
    {
        snprintf(out, cap, "settings.json");
        FILE *f = fopen(out, "rb");
        if (f) { fclose(f); return 1; }
    }
    return 0;
}

/* Where settings.json WOULD go when there is not one yet. Same candidate
   order as find_settings and the same answer whenever a file exists, so a
   save lands on the file a load read. Beside the exe is the bundle layout,
   so that is the one a fresh save creates. */
int settings_write_path(char *out, size_t cap)
{
    if (find_settings(out, cap)) return 1;
#ifdef _WIN32
    {
        char exe[MAX_PATH];
        DWORD n = GetModuleFileNameA(NULL, exe, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            char *slash = strrchr(exe, '\\');
            char *fwd = strrchr(exe, '/');
            if (fwd && (!slash || fwd > slash)) slash = fwd;
            if (slash) {
                *slash = '\0';
                if (strlen(exe) + 16 < cap) {
                    snprintf(out, cap, "%s\\settings.json", exe);
                    return 1;
                }
            }
        }
    }
#endif
    {
        const char *root = getenv("SM64DS_ASSET_ROOT");
        if (root && *root && strlen(root) + 16 < cap) {
            snprintf(out, cap, "%s/settings.json", root);
            return 1;
        }
    }
    snprintf(out, cap, "settings.json");
    return 1;
}

/* A COPY of `s` with `key` set to `val` -- which is a whole JSON value token,
   quotes and all, because the caller knows whether it is writing a string or
   a number. The key's existing value is replaced where it is; a key that is
   not there yet is inserted just inside the opening brace. Null on any
   failure, which every caller reads as "do not write anything".

   Replacing rather than rewriting is the whole point. settings.json is the
   LAUNCHER's file and it owns keys this program has never heard of (volume,
   crash reports, whatever the launcher grows next). A save that serialized
   the three values this file knows about would silently drop the rest, and
   the player would find out by losing their volume. Every byte that is not
   one of the values being set comes out the far side unchanged. */
char *json_set(const char *s, const char *key, const char *val)
{
    const char *cut_a, *cut_b;
    const char *insert = 0;
    int comma = 0;
    char *out;
    size_t need;

    const char *v = json_value(s, key);
    if (v) {
        cut_a = v;
        cut_b = json_value_end(v);
    } else {
        /* just past the opening brace, which looks_like_json_object has
           already proved is there (BOM and leading space included) */
        const char *b = s;
        while (*b && *b != '{') ++b;
        if (!*b) return 0;
        cut_a = cut_b = b + 1;
        insert = key;
        /* the separator only when something FOLLOWS. An object with no
           members at all is the case that made this necessary: a trailing
           comma is not JSON, System.Text.Json refuses to read it, and the
           launcher would have lost the file on the first save it made into
           an empty one. */
        {
            const char *t = cut_b;
            while (*t == ' ' || *t == '\t' || *t == '\r' || *t == '\n') ++t;
            comma = *t != '}' && *t != '\0';
        }
    }

    need = strlen(s) + strlen(val) + (insert ? strlen(key) + 12 : 0) + 8;
    out = (char *)malloc(need);
    if (!out) return 0;
    {
        char *w = out;
        memcpy(w, s, (size_t)(cut_a - s));
        w += cut_a - s;
        if (insert) {
            *w++ = '\n';
            *w++ = ' ';
            *w++ = ' ';
            *w++ = '"';
            memcpy(w, key, strlen(key));
            w += strlen(key);
            *w++ = '"';
            *w++ = ':';
            *w++ = ' ';
        }
        memcpy(w, val, strlen(val));
        w += strlen(val);
        if (comma) {
            *w++ = ',';
            /* one member a line, the way the launcher writes it. Only when
               the next thing along is not already on its own line, so a
               pretty-printed file keeps its own shape. */
            if (*cut_b != '\n' && *cut_b != '\r' && *cut_b != ' ' &&
                *cut_b != '\t') {
                *w++ = '\n';
                *w++ = ' ';
                *w++ = ' ';
            }
        }
        memcpy(w, cut_b, strlen(cut_b) + 1);
    }
    return out;
}

/* Whole text to `path`, through a sibling temp file and a replacing rename.
   A settings.json truncated by a crash mid-write would read as malformed and
   throw away every launcher key in it, and the launcher's own loader would
   do the same; going via a temp means the file a reader sees is either the
   old one or the new one.

   run mg16 lane MP2: THE TEMP NAME CARRIES THE INSTANCE SUFFIX, and that is a
   BUG FIX rather than tidiness. Two copies of the game share one settings.json,
   so with a single shared "settings.json.tmp" two overlapping saves both open
   the SAME temp, interleave their bytes into it, and then each rename it into
   place -- so MoveFileExA publishes a MIXTURE of two writes and the atomicity
   this comment claims is defeated exactly when it is needed. A per-instance
   temp restores it: each writer has its own file, and the rename is still the
   single atomic publish. Unset, the name is "settings.json.tmp" as before. */
int write_text(const char *path, const char *text)
{
    char tmp[1024];
    if (strlen(path) + 24 >= sizeof tmp) return 0;
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    {
        FILE *f = fopen(tmp, "wb");
        size_t n = strlen(text);
        if (!f) return 0;
        if (fwrite(text, 1, n, f) != n) { fclose(f); remove(tmp); return 0; }
        if (fclose(f) != 0) { remove(tmp); return 0; }
    }
#ifdef _WIN32
    /* a short retry: the launcher (or a second copy of the game) may have
       the old file open for a moment, and a sharing violation on the rename
       is gone a few milliseconds later */
    {
        int tries = 0;
        while (!MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING)) {
            if (++tries >= 5) { remove(tmp); return 0; }
            Sleep(20);
        }
    }
#else
    if (rename(tmp, path) != 0) { remove(tmp); return 0; }
#endif
    return 1;
}

int g_loaded;
int g_swap_camera_turn;              /* default 0 */

/* ---- HOW RUNNING WORKS -------------------------------------------------
   The DS had no run button. Mario runs when the stick is pushed past about
   seven eighths of its travel (func_ov002_020d4748 and its siblings test the
   input record's magnitude against 0xdc7 of 0x1000, with a 0x80 of
   hysteresis), and the DS's stick was the touch screen. A host pad has a
   real stick and a host keyboard has neither, so this program has always
   fed the game the D-pad path -- full magnitude, all the time -- and put
   running on a HELD BUTTON, which is a binding the hardware never had.

   These three settings pick which of those the player gets. They shape the
   input record on the way in and nothing else: the game reads the same
   fields it always read, and src/ has no idea any of this exists.

     RunMode         "button" | "analog" (default) | "auto"
     RunButtonKey    Win32 virtual-key code for the keyboard binding.
                     Default 0x10, which is shift -- what the window has
                     always used. 0 means no keyboard binding at all.
     RunButtonPad    XInput button mask for the pad binding. Default 0x4000,
                     which is X on an Xbox layout -- again what the window
                     has always used. 0 means no pad binding.

   A missing file, a missing key or a value that will not parse is the
   default. The bindings and the keys are unchanged from what this window has
   always used; the MODE now defaults to analog on Tango's order. A keyboard
   has no stick to push, so with no pad (or the stick in its dead zone) it
   falls back to button mode: WASD walk, the bound run key holds the run bit,
   exactly as before. */
int g_run_mode;                      /* default RUN_ANALOG (1) */
int g_run_key;                       /* default 0x10, VK_SHIFT */
int g_run_pad;                       /* default 0x4000, pad X */

const char *const RUN_MODE_KEY[3] = { "button", "analog", "auto" };

/* ---- CameraMode -----------------------------------------------------------
   0 analog, 1 freecam, 2 ds -- tests/walk_window.cpp's CAM_ numbering. The
   default is ds, on Tango's order, because that is the cartridge's own
   stepped rotate and it is the mode the bumpers turn in; analog and freecam
   stay one F1 press (or one menu row) away. The header carries the rest.
   Read by name and, like RunMode, by number too. */
int g_camera_mode;                   /* default 2, ds */
const char *const CAMERA_MODE_KEY[3] = { "analog", "freecam", "ds" };
/* HYBRIDCAM1 (Tango's S1): the hybrid camera, the DS camera that hands over to the analog cam while the mouse or the
   right stick turns it and eases back after a few quiet seconds. It is the default. It is NOT a fourth CameraMode word: the launcher
   (Settings.SanitizeCameraMode) turns any word it does not know into "ds" and writes it back, and 1.0.3 does the same on read. So
   the file keeps CameraMode in its three words and hybrid is "CameraMode ds" with CameraHybrid not false:
       CameraHybrid absent  -> never picked: hybrid when CameraMode is ds (or absent), the default every file had
       CameraHybrid true    -> hybrid (the settings page wrote it)
       CameraHybrid false   -> the player picked a mode on the settings page; ds means the plain DS camera
   A file that names analog or freecam keeps that mode either way (ds was the default, so those were choices). What an old file
   cannot say is whether its "ds" was picked or just the default: the launcher writes every key with its default, so both read the
   same and land on hybrid. host_setting_camera_mode returns 3 for hybrid; the stored word stays 0..2. */
int g_camera_hybrid = -1;            /* -1 absent, 0 false, 1 true */
int camera_mode_eff(void) { return (g_camera_mode == 2 && g_camera_hybrid != 0) ? 3 : g_camera_mode; }

/* ---- THE CONTROL BINDINGS -------------------------------------------------
   One settings.json key per action, keyboard and pad in two tables indexed by
   the HOST_KEY_ / HOST_PAD_ enums in the header, which is also where the
   meaning of every default is written down. The two tables below hold the
   SPELLING and the DEFAULT side by side so they cannot drift apart, and the
   stderr line at the end of load_once walks the same tables.

   The run action is special in one way only: its old spellings, RunButtonKey
   and RunButtonPad, still work. KeyRun / PadRun win when present; the old
   name is read when the new one is not there; and the save path writes both.
   That is the whole of the alias, and it lives in the two lines that read the
   run entry rather than in a rule anywhere else. */
struct BindKey { const char *name; int dflt; };
const BindKey KEY_BIND[HOST_KEY_COUNT] = {
    { "KeyUp",       0x57 },      /* W */
    { "KeyDown",     0x53 },      /* S */
    { "KeyLeft",     0x41 },      /* A */
    { "KeyRight",    0x44 },      /* D */
    { "KeyUpAlt",    0x26 },      /* VK_UP */
    { "KeyDownAlt",  0x28 },      /* VK_DOWN */
    { "KeyLeftAlt",  0x25 },      /* VK_LEFT */
    { "KeyRightAlt", 0x27 },      /* VK_RIGHT */
    { "KeyJump",     0x20 },      /* VK_SPACE */
    { "KeyAttack",   0x58 },      /* X */
    { "KeyCrouch",   0x11 },      /* VK_CONTROL */
    { "KeyRun",      0x10 },      /* VK_SHIFT; alias RunButtonKey */
    { "KeyStart",    0x0d },      /* VK_RETURN */
    { "KeySelect",   0x08 },      /* VK_BACK */
    { "KeyLook",     0x5a },      /* Z: the DS X button; see header */
    { "KeyCamCenter", 0x43 },     /* C: centre camera; see header */
    { "KeyCamLeft",  0x51 },      /* Q: rotate camera left */
    { "KeyCamRight", 0x45 },      /* E: rotate camera right */
    { "KeyCamMode",  0x70 },      /* F1: cycle the camera mode */
    { "KeyCamUp",    0x52 },      /* R: tilt up (analog / free camera) */
    { "KeyCamDown",  0x46 },      /* F: tilt down */
    { "KeyCamZoomIn",  0 },       /* CAMPACK1: unbound by default */
    { "KeyCamZoomOut", 0 },
};
const BindKey PAD_BIND[HOST_PAD_COUNT] = {
    { "PadJump",   0x1000 },      /* A */
    { "PadAttack", 0x2000 },      /* B */
    { "PadCrouch", 0x20000 },     /* RT, the trigger pseudo-button; see header */
    { "PadRun",    0x4000 },      /* X; alias RunButtonPad */
    { "PadStart",  0x0010 },      /* START */
    { "PadSelect", 0      },      /* BACK opens the debug menu; see header */
    { "PadLook",   0x8000 },      /* Y: the DS X button; see header */
    { "PadCamCenter", 0x10000 },  /* LT: centre camera; see header */
    { "PadCamLeft",  0x0100 },    /* LB: rotate camera left */
    { "PadCamRight", 0x0200 },    /* RB: rotate camera right */
    { "PadCamMode",  0x0080 },    /* right stick click: cycle the camera mode */
    { "PadCamUp",    0 },         /* CAMPACK1: the right stick tilts; these are extra buttons */
    { "PadCamDown",  0 },
    { "PadCamZoomIn",  0 },
    { "PadCamZoomOut", 0 },
};
/* CAMPACK1: mouse buttons, Win32 VK codes (0 none, 2 right, 4 middle, 5 X1, 6 X2) */
const BindKey MOUSE_BIND[HOST_MOUSE_COUNT] = {
    { "MouseCamHold",    2 },     /* right button held: mouse look, as it always was */
    { "MouseCamCenter",  0 },
    { "MouseCamLeft",    0 },
    { "MouseCamRight",   0 },
    { "MouseCamUp",      0 },
    { "MouseCamDown",    0 },
    { "MouseCamZoomIn",  0 },
    { "MouseCamZoomOut", 0 },
    { "MouseCamMode",    0 },
    { "MouseLook",       0 },
    { "MouseJump",       0 },     /* MOUSEBIND1: the game actions, unbound until the player binds one */
    { "MouseAttack",     1 },     /* CLICK1: left click attacks unless the player chose otherwise (mouse_attack_default) */
    { "MouseCrouch",     0 },
    { "MouseRun",        0 },
    { "MouseStart",      0 },
    { "MouseSelect",     0 },
};
int g_mouse[HOST_MOUSE_COUNT];
int g_camera_stick_zoom;
inline int mouse_vk_ok(int v) { return v == 0 || v == 2 || (v >= 4 && v <= 6); }
/* MOUSELOCK1: the left button (VK 1) may sit on any action but the Mouse look button (a held left that steers would be the press every
   menu reads as the stylus; Tango asked for left to punch). Where left is the stylus is tests/walk_window.cpp's mo_pen_moment. */
inline int mouse_vk_ok_for(int action, int v) { return mouse_vk_ok(v) || (v == 1 && action != HOST_MOUSE_CAM_HOLD); }
/* CLICK1 (Tango, 11-12: "Only left click not the right click to jump"): players expected left click to attack and it did nothing,
   because every mouse action shipped unbound. Left is Attack now for anyone whose settings.json does not name MouseAttack (a fresh
   install, and every player who never chose a mouse button for Attack). A file that names a valid MouseAttack, 0 included, is the
   player's choice and stands; the in-game page's choice is written to the file and stands the same way. And the default steps
   aside while the player has put left on any other mouse action (MouseJump = 1, a camera button on left), so left is never two
   actions the player did not ask for. Right click stays unbound (MouseJump 0); right held is still the Mouse look button.
   The launcher (Settings.GetActionMouse) shows the same answer by the same rule. */
int g_mouse_attack_named;    /* 1: MouseAttack is the player's own (named in the file, or set on the in-game page) */
void mouse_attack_default()
{
    if (g_mouse_attack_named) return;
    int left_taken = 0;
    for (int i = 0; i < HOST_MOUSE_COUNT; ++i)
        if (i != HOST_MOUSE_ATTACK && g_mouse[i] == 1) left_taken = 1;
    g_mouse[HOST_MOUSE_ATTACK] = left_taken ? 0 : 1;
}
int g_key[HOST_KEY_COUNT];
int g_pad[HOST_PAD_COUNT];
/* Which action already held the Look default's key / pad button in a file
   that did not name KeyLook / PadLook (index + 1; 0 = none). Set by load_once,
   said once by its log block. */
int g_look_key_taken;
int g_look_pad_taken;
/* The same for the centre-camera default (KeyCamCenter / PadCamCenter): the
   action that already held it in a file that never named the key. */
int g_cc_key_taken;
int g_cc_pad_taken;
/* LLLSHELL1: THE CAMERA TURN AND TILT DEFAULTS GIVE WAY TOO. A player who moved
   Attack onto E (the camera-right default) got both on every press: the DS
   camera stepped right with its whoosh and the red arrow while he held Attack
   on the Koopa shell. A turn / tilt binding the file never named, and the
   player never set on the page, is unbound while a gameplay action (the
   movement keys up to Look; the pad's Jump up to Look) holds the same key or
   button; it comes back if that action moves off it. A camera binding the
   player named keeps doubling up, as before. Bit i of a mask = named. */
const int CAM_YIELD_KEY[4] = { HOST_KEY_CAM_LEFT, HOST_KEY_CAM_RIGHT, HOST_KEY_CAM_UP, HOST_KEY_CAM_DOWN };
const int CAM_YIELD_PAD[2] = { HOST_PAD_CAM_LEFT, HOST_PAD_CAM_RIGHT };
unsigned g_cam_key_named, g_cam_pad_named;
int g_cam_key_taken[4], g_cam_pad_taken[2];   /* index + 1 of the action holding the default */
void cam_turn_yield(void)
{
    for (int j = 0; j < 4; ++j) {
        const int a = CAM_YIELD_KEY[j];
        g_cam_key_taken[j] = 0;
        if (g_cam_key_named & (1u << a)) continue;
        g_key[a] = KEY_BIND[a].dflt;
        for (int i = 0; i <= HOST_KEY_LOOK; ++i)
            if (g_key[i] == g_key[a]) { g_cam_key_taken[j] = i + 1; g_key[a] = 0; break; }
    }
    for (int j = 0; j < 2; ++j) {
        const int a = CAM_YIELD_PAD[j];
        g_cam_pad_taken[j] = 0;
        if (g_cam_pad_named & (1u << a)) continue;
        g_pad[a] = PAD_BIND[a].dflt;
        for (int i = 0; i <= HOST_PAD_LOOK; ++i)
            if (g_pad[i] & g_pad[a]) { g_cam_pad_taken[j] = i + 1; g_pad[a] = 0; break; }
    }
}
void cam_turn_named_from(const char *text)
{
    for (int j = 0; j < 4; ++j)
        if (json_value(text, KEY_BIND[CAM_YIELD_KEY[j]].name)) g_cam_key_named |= 1u << CAM_YIELD_KEY[j];
    for (int j = 0; j < 2; ++j)
        if (json_value(text, PAD_BIND[CAM_YIELD_PAD[j]].name)) g_cam_pad_named |= 1u << CAM_YIELD_PAD[j];
}

/* ---- THE LIVE CONTROL KEYS (run menu1, lane SETLIVE1) ---------------------
   Six numbers the in-game settings page changes while the game runs, all
   plain integers in settings.json (the launcher keeps unknown keys, and ints
   survive its round trip as ints):

     StickDeadzone      percent of the left stick's travel that reads as
                        centred, 0..50. Default = today's floor, 7849 of
                        32767 (24 percent): an absent key AND the value 24
                        both mean exactly 7849, so the default feel is the
                        shipped feel to the bit.
     RunThreshold       percent of the usable travel (past the dead zone)
                        where Mario starts to run, 30..100. The cartridge
                        starts a run at 0xe47 of 0x1000 (89.2 percent); an
                        absent key AND the value 89 mean no remap at all.
     InvertCameraY      true/false, the analog and free camera's up/down.
     CameraSensitivity  percent of the right stick's (and Q/E's) turn rate,
                        10..400, 100 = today. The DS camera steps a fixed
                        0x400 per tick in the ROM; the right stick there turns it by how
                        far it is pushed (a full push at 100 is that step), scaled by this.
     MouseSensitivity   percent of the mouse look, 10..400, 100 = today.
   g_ctl_gen moves whenever any control or camera value changes (a set or a
   file reload), so the frame loop refreshes its copies on a counter compare. */
int g_stick_dead = 7849;             /* raw units of 32767 */
int g_run_thresh_pct = -1;           /* -1 = the ROM's own point, no remap */
int g_invert_camera_y;               /* default 0 */
int g_camera_sens = 100;
int g_cam_stick_accel, g_cam_stick_decel;   /* CameraStickAccel / CameraStickDecel 0..10, 0 = instant (SKYCAM1 phase 2) */
int g_mouse_sens = 100;
int g_ctl_gen = 1;
int g_cam_gen = 1;                   /* moves when CameraMode is SET or reloaded */
int g_poll_flag;                     /* an API set wants the next poll to say 1 */

const int STICK_DEAD_DEFAULT = 7849;
const int RUN_THRESH_ROM_PCT = 89;   /* 0xe47 / 0x1000 = 89.2 percent */

int stick_dead_from_pct(int pct)
{
    if (pct < 0) pct = 0;
    if (pct > 50) pct = 50;
    if (pct == 24) return STICK_DEAD_DEFAULT;
    return (int)((long long)pct * 32767 / 100);
}
int stick_dead_to_pct(int raw)
{
    return (int)(((long long)raw * 100 + 16383) / 32767);
}

/* ---- THE DS SCREEN GAP -------------------------------------------------
   The four keys behind the launcher's "remove minigame gap" checkbox and
   the three that shape the gap when it is left in. See port/hal/screen_gap.h
   for what the gap IS; these are only how a player says what they want.

     MinigameGap    true (default) leaves the hinge simulated, which is what
                    the game's own code does and what makes an object that
                    crosses between the screens move at one speed. The
                    launcher's checkbox is the INVERSE of this key: ticking
                    "remove minigame gap" writes false, and false pulls the
                    two screens back together and puts the seam jump back.
     GapFillMode    "ambient" (default), "solid" or "custom". Ignored when
                    GapPeek is true; see below.
     GapColor       "#RRGGBB" for the solid fill. Default "#000000". Ignored
                    when GapPeek is true; see below.
     GapPeek        false (default). True draws the sprites that are genuinely
                    inside the band, over the scene's own art if there is any
                    and over BLACK if there is not.

   "CUSTOM" IS A PICTURE DRAWN FOR ONE MINIGAME, and it comes off disk rather
   than out of a setting:

       <bundle>/gap/scene_<id>.bmp        gap/scene_368.bmp is Bob-omb Squad

   in the same folder settings.json is looked for, named by the scene's own id.
   24-bit or 32-bit uncompressed BMP, 256 x G or 512 x 2G (the 2x sheet, taken
   down by nearest sampling), BAND ROWS ONLY -- no top-screen or bottom-screen
   context rows. MAGENTA #FF00FF IS THE TRANSPARENCY KEY: a keyed pixel shows
   whatever is behind the art, which is the ambient wash with peek off and
   black with peek on, so a band can be a painted machine down the middle and
   the ordinary backdrop either side of it. Anything wrong with the file -- not
   there, wrong size, wrong depth, truncated -- is ONE LINE on stderr and the
   ambient fill, never a crash and never a guess. hal/gap_art.h carries the
   long form.

   PEEK IGNORES THE FILL MODE, and that is a decision about what the mode is
   FOR rather than a shortcut. With peek off the band is decoration and these
   keys are the player's taste in it. With peek on the band stops being
   decoration and becomes a view of what is actually in those rows, and the
   true state of a row with no sprite in it is empty. A FILL behind that would
   put something over the answer and call the mixture the answer: an ambient
   wash blurs into the sprites it sits behind, and a chosen colour cannot be
   told apart from a sprite of the same colour.

   THE ART IS ALLOWED BEHIND PEEK ANYWAY, because it is a different kind of
   thing. A hand-drawn band reads as a picture -- a hinge, a machine, a painted
   backing -- and nobody mistakes it for the game submitting a sprite, which is
   exactly what a wash or a flat colour invites. So peek's backdrop is the
   scene's art when the file is there and black when it is not, and everything
   over that backdrop is still only what the engines really submitted.

   All four are optional and each falls back on its own, so a settings.json
   written by a launcher that predates any of them is read exactly as a file
   that sets it to the default -- which is the property that lets the game
   and the launcher ship on their own schedules. */
int g_gap_on;                        /* default 1 */
/* 0 solid, 1 ambient, 2 custom -- the ntr::GAP_FILL_* numbering, which is what
   hal/screen_gap.cpp hands straight to ntr::stack_layout. That function
   re-validates it and reads anything it does not know as ambient, so the two
   spellings cannot disagree about an unknown value. */
int g_gap_fill;                      /* default 1, ambient */
int g_audio_quality = 1;             /* AudioQuality: 1 "hq" (default), 0 "ds"; lane AUDIOHQ1 */
unsigned g_gap_color;                /* default 0xFF000000, black */
int g_gap_peek;                      /* default 0 */






/* Volume, 0..100, or -1 while the file has never named one. The launcher owns
   this key and also passes it as SM64DS_VOLUME at launch; the file copy exists
   so the live re-read below can move it while the game is running. */
int g_volume = -1;

/* MouseCapture: 1 when the player asked the window to hold the pointer and
   steer the camera with bare movement instead of a right-button drag. Default
   1 since MOUSELOCK1 (Tango, 11-07: an on-by-default toggle); a file that
   saved 0 keeps 0. See the header for the list
   of places it deliberately does not engage; every one of them is enforced in
   tests/walk_window.cpp, because whether a pointer may be taken is a question
   about the window and not about this file. */
int g_mouse_capture;

/* StartFullscreen: 1 when the window opens in the same borderless fullscreen
   F12 toggles. Default 0 (today). Read at boot only. */
int g_start_fullscreen;


/* DrawDistance: percent of the ROM's own per-actor draw distance an actor may
   be drawn out to (100 = the ROM exactly, the default; 10000 = Max). Drawing
   only: dActor_c::BeforeBehavior keeps the ROM's update decision untouched. */
int g_draw_dist = 100;
/* g_tex_swap_key: a virtual-key setting the port's other callers read
   (a key that steps through picture choices). F7 by default; 0 = unbound. */
int g_tex_swap_key = 0x76;
int tex_swap_key_clamp(int v) { return v >= 0 && v <= 0xFE ? v : 0x76; }
int draw_dist_clamp(int v) { return v < 100 ? 100 : v > 10000 ? 10000 : v; }








/* KeepLives: 1 keeps each save file's life count in the port's own side file (save/lives.json) and gives it back when that file starts; 0 is the DS, which starts every file with 4 lives. Default 0. Read live. */
int g_keep_lives = 0;
/* SKIPINTRO1: SkipOpening, 1 skips a new file's opening (Peach's letter, Lakitu's flight) and starts the file at the castle with the opening marked seen; 0 (the default) is the DS. Read live. */
int g_skip_opening = 0;
char g_launcher_nick[24];    /* PROFILE1: the launcher's Nickname key, read for the popup's first suggestion only */
/* FixOriginalBugs (RABBIT1): 1 applies the port's fixes for the cartridge's own bugs (the glowing rabbits rank themselves by catch order). Default 1. Read live. */
int g_fix_original_bugs = 1;
/* CameraCollision (CAMCOLL1): 1 = the analog / free camera's eye collides with the level (0.9.3), 0 = it passes through (0.9.2). Default 1. Read live. */
int g_camera_collision = 1;
/* WideDrawingPage (DOODLEWIDE1): 1 makes the title's drawing page as wide as the window at the DS pixel scale. Default 1. Read live (applied at the next page open). */
int g_wide_drawing = 1;
/* PeachJingle (PEACHJINGLE1): 1 plays the cartridge's own NCS_BGM_PEACH tune when a star quota message opens. Default 1. Read live. */
int g_peach_jingle = 1;

/* Aspect: the presentation ratio, WIDTH DIVIDED BY HEIGHT. 0 (the default) is
   the explicit native sentinel -- the DS's own 4:3 at the 512x384 window the
   port has always opened. 1.7777778 is 16:9. It is a NUMBER and not a
   Widescreen boolean for one reason that matters and one that follows from it:
   a boolean cannot express ultrawide, and a number means an odd monitor needs
   no new mode name, only its own ratio.

   ACCEPTED: 0 for native, or a ratio CLAMPED TO [1.0, 4.0]. Absent,
   unparseable, negative or otherwise not a positive number all read as 0, so a
   file written before this key existed -- or half-edited by hand -- lands on the
   shipped 4:3 look like every other key here. A positive number outside the band
   is clamped rather than rejected: 9.0 becomes 4.0, which is a picture, where
   rejecting it would be a surprise. The ceiling is 4.0 and not 3.0 because
   32:9 is 3.5555556 and those monitors are real; see aspect_sanitise for why
   4.0 and not higher.

   BOOT-LATCHED, not read live: the framebuffer aspect is chosen once, at boot,
   and threaded through the whole render path (ntr::configure_aspect), so unlike
   the settings read live, a mid-run change cannot take. The only reader is
   walk_window's boot, which calls ntr::configure_aspect(host_setting_aspect()). */
double g_aspect = 0.0;

int g_vs_luigi_seconds = 0;


/* Steps once per live re-read that changed an answer. hal/screen_gap.cpp
   latches on it. */
int g_setgen;

/* ---- PadLayouts ------------------------------------------------------------
   The learned controller maps, see the header. Kept as a small fixed table;
   the file's array is parsed object by object with the same scalar readers
   as every other key, each object copied out on its own so "a" inside one
   object cannot be found inside another. */
HostPadLayout g_padlayouts[HOST_PAD_LAYOUT_MAX];
int g_padlayout_n;

/* The generic fallback row of pad_backend.cpp's PAD_LAYOUTS, field for
   field. Every field a file object lacks reads as this. */
void padlayout_default(HostPadLayout *o)
{
    memset(o, 0, sizeof *o);
    o->a = 0; o->b = 1; o->x = 2; o->y = 3; o->lb = 4; o->rb = 5;
    o->back = 8; o->start = 9; o->lthumb = 10; o->rthumb = 11;
    o->lt_btn = 6; o->rt_btn = 7; o->lt_axis = -1; o->rt_axis = -1;
    o->lx_axis = 0; o->ly_axis = 1; o->rx_axis = 2; o->ry_axis = 5;
    o->lx_sign = 1; o->ly_sign = -1; o->rx_sign = 1; o->ry_sign = -1;
}

/* The field table: spelling, member, range. One row per PadLayout field so
   the parser, the clamp and the writer walk the same list. */
struct PadField { const char *name; int HostPadLayout::*f; int lo, hi; };
const PadField PAD_FIELDS[22] = {
    { "a",       &HostPadLayout::a,       -1, 31 },
    { "b",       &HostPadLayout::b,       -1, 31 },
    { "x",       &HostPadLayout::x,       -1, 31 },
    { "y",       &HostPadLayout::y,       -1, 31 },
    { "lb",      &HostPadLayout::lb,      -1, 31 },
    { "rb",      &HostPadLayout::rb,      -1, 31 },
    { "back",    &HostPadLayout::back,    -1, 31 },
    { "start",   &HostPadLayout::start,   -1, 31 },
    { "lthumb",  &HostPadLayout::lthumb,  -1, 31 },
    { "rthumb",  &HostPadLayout::rthumb,  -1, 31 },
    { "lt_btn",  &HostPadLayout::lt_btn,  -1, 31 },
    { "rt_btn",  &HostPadLayout::rt_btn,  -1, 31 },
    { "lt_axis", &HostPadLayout::lt_axis, -1, 5 },
    { "rt_axis", &HostPadLayout::rt_axis, -1, 5 },
    { "lx_axis", &HostPadLayout::lx_axis, -1, 5 },
    { "ly_axis", &HostPadLayout::ly_axis, -1, 5 },
    { "rx_axis", &HostPadLayout::rx_axis, -1, 5 },
    { "ry_axis", &HostPadLayout::ry_axis, -1, 5 },
    { "lx_sign", &HostPadLayout::lx_sign, -1, 1 },
    { "ly_sign", &HostPadLayout::ly_sign, -1, 1 },
    { "rx_sign", &HostPadLayout::rx_sign, -1, 1 },
    { "ry_sign", &HostPadLayout::ry_sign, -1, 1 },
};

/* One object's text (NUL-terminated, braces included) into *o. 0 when the
   object has no usable vid:pid, which drops it. A sign of 0 is out of
   range: an axis that moves neither way is not a choice. */
int padlayout_parse_object(const char *obj, HostPadLayout *o)
{
    padlayout_default(o);
    const int vid = json_int(obj, "vid", 0);
    const int pid = json_int(obj, "pid", 0);
    if (vid < 1 || vid > 0xffff || pid < 1 || pid > 0xffff) return 0;
    o->vid = vid;
    o->pid = pid;
    for (int i = 0; i < 22; ++i) {
        const int dflt = o->*PAD_FIELDS[i].f;
        const int v = json_int(obj, PAD_FIELDS[i].name, dflt);
        const int is_sign = PAD_FIELDS[i].lo == -1 && PAD_FIELDS[i].hi == 1;
        if (v >= PAD_FIELDS[i].lo && v <= PAD_FIELDS[i].hi && !(is_sign && v == 0))
            o->*PAD_FIELDS[i].f = v;
    }
    if (json_str(obj, "name", o->name, sizeof o->name)) {
        /* json_str hands the value back as written; the writer below
           escapes quotes and backslashes, so undo exactly those two */
        char *w = o->name;
        for (const char *r = o->name; *r; ++r) {
            if (*r == '\\' && (r[1] == '"' || r[1] == '\\')) ++r;
            *w++ = *r;
        }
        *w = '\0';
    }
    return 1;
}

/* The whole array. Malformed text stops the walk where it is found; the
   objects already read stay. */
void padlayouts_parse(const char *text)
{
    g_padlayout_n = 0;
    const char *v = json_value(text, "PadLayouts");
    if (!v || *v != '[') return;
    const char *end = json_value_end(v);
    const char *p = v + 1;
    while (p < end && g_padlayout_n < HOST_PAD_LAYOUT_MAX) {
        while (p < end && *p != '{' && *p != ']') ++p;
        if (p >= end || *p == ']') break;
        const char *e = json_value_end(p);
        if (e <= p || e > end) break;
        const size_t n = (size_t)(e - p);
        char *obj = (char *)malloc(n + 1);
        if (!obj) break;
        memcpy(obj, p, n);
        obj[n] = '\0';
        HostPadLayout o;
        const int ok = padlayout_parse_object(obj, &o);
        free(obj);
        if (ok) {
            /* a later object for the same pad replaces the earlier one, so
               a hand edit that duplicates a row reads as its last word */
            int slot = g_padlayout_n;
            for (int i = 0; i < g_padlayout_n; ++i)
                if (g_padlayouts[i].vid == o.vid && g_padlayouts[i].pid == o.pid)
                    slot = i;
            g_padlayouts[slot] = o;
            if (slot == g_padlayout_n) ++g_padlayout_n;
        }
        p = e;
    }
}

/* The array as text, the shape the header shows, into a malloc'd buffer. */
char *padlayouts_text(void)
{
    const size_t cap = 64 + (size_t)g_padlayout_n * 512;
    char *out = (char *)malloc(cap);
    if (!out) return 0;
    size_t w = 0;
    w += (size_t)snprintf(out + w, cap - w, "[");
    for (int i = 0; i < g_padlayout_n; ++i) {
        const HostPadLayout *o = &g_padlayouts[i];
        /* the name is written with quotes escaped, and backslashes and
           control characters DROPPED, so a product name cannot break the
           file; the name is for a person reading it, not for matching */
        char nm[96];
        size_t k = 0;
        for (const char *c = o->name; *c && k + 3 < sizeof nm; ++c) {
            if ((unsigned char)*c < 0x20 || *c == '\\') continue;
            if (*c == '"') nm[k++] = '\\';
            nm[k++] = *c;
        }
        nm[k] = '\0';
        w += (size_t)snprintf(out + w, cap - w,
                              "%s\n    { \"vid\": %d, \"pid\": %d, \"name\": \"%s\"",
                              i ? "," : "", o->vid, o->pid, nm);
        for (int f = 0; f < 22; ++f)
            w += (size_t)snprintf(out + w, cap - w, ",%s\"%s\": %d",
                                  (f % 6 == 0) ? "\n      " : " ",
                                  PAD_FIELDS[f].name, o->*PAD_FIELDS[f].f);
        w += (size_t)snprintf(out + w, cap - w, " }");
        if (w >= cap) { free(out); return 0; }
    }
    snprintf(out + w, cap - w, "%s]", g_padlayout_n ? "\n  " : "");
    return out;
}

/* "#RRGGBB" to 0xFFRRGGBB. Returns dflt for anything that is not exactly six
   hex digits after an optional '#', so a half-typed colour is the default
   rather than a colour nobody chose. Case insensitive, because a player
   pasting a colour out of a picker gets either case. */
unsigned parse_hex_color(const char *s, unsigned dflt)
{
    unsigned v = 0;
    int n = 0;
    if (!s) return dflt;
    if (*s == '#') ++s;
    for (; *s; ++s, ++n) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
        else return dflt;
        if (n >= 6) return dflt;
        v = (v << 4) | (unsigned)d;
    }
    if (n != 6) return dflt;
    return 0xFF000000u | v;
}


/* ---- FrameRate: THE ONE PLACE THE PRESENTATION BAND IS SPELLED OUT -------
   Both readers of the key -- settings.json and the SM64DS_FRAME_RATE override
   -- come through here, so the file and the environment cannot disagree about
   what 1000 or 45 means.

     not a positive number  -> 0, the explicit native sentinel: one picture per
                               game tick, the port's shipped behaviour. Covers
                               absent, unparseable, 0 itself and negatives.
     1..59                  -> 0 as well. 60 is the FLOOR because 60 is the
                               fastest the ROM's own clock ever runs
                               (data_0208ee44 == 1, every minigame), so a
                               smaller number would be asking the port to
                               present less often than the game ticks. That is
                               not a presentation choice, it is a slower game,
                               and this key does not do that.
     60..240                -> that many pictures a second.
     above 240              -> clamped to 240, the Aspect rule: a number that
                               is a picture beats an error. */
int frame_rate_sanitise(int n)
{
    if (n <= 0) return 0;
    if (n < 60) return 0;
    /* SM64DS_FRAME_RATE_MAX=N (OPTIM1, measurement only, inert unset): lifts the 240 clamp so a run can show how far the picture rate goes
       when nothing caps it. Never in a settings file's reach: the file's own value is still read through this same function. */
    static int top = 0;
    if (!top) { const char *e = getenv("SM64DS_FRAME_RATE_MAX"); top = (e && atoi(e) > 240 && atoi(e) <= 2000) ? atoi(e) : 240; }
    if (n > top) return top;
    return n;
}

/* The primary display's refresh rate, once, for the key's "display" spelling.
   user32 is HAND-LOADED rather than imported, the rule this whole port follows
   (port/hal/pad_backend.cpp and hal/asset_root_refuse.cpp do the same): a
   static import table maps over 0x02000000 and the ROM's own address space
   lives there. A display that reports 0 or 1 (the "driver default" answer
   EnumDisplaySettings is allowed to give) is no answer, so it reads as 0 and
   the sanitiser turns that into native. */
int display_refresh_hz(void)
{
#ifdef _WIN32
    typedef BOOL (WINAPI *EnumDisplaySettingsA_t)(LPCSTR, DWORD, DEVMODEA *);
    static int hz = -1;
    if (hz >= 0) return hz;
    hz = 0;
    if (HMODULE u = LoadLibraryA("user32.dll")) {
        EnumDisplaySettingsA_t f =
            (EnumDisplaySettingsA_t)GetProcAddress(u, "EnumDisplaySettingsA");
        if (f) {
            DEVMODEA dm;
            memset(&dm, 0, sizeof dm);
            dm.dmSize = sizeof dm;
            if (f(0, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 1)
                hz = (int)dm.dmDisplayFrequency;
        }
    }
    return hz;
#else
    return 0;
#endif
}

/* FrameRate: the presentation rate in pictures a second, 0 for native (one
   picture per game tick). BOOT-LATCHED like g_aspect and for the same kind of
   reason: the presentation clock's shape is settled at the pacer's first turn
   and a mid-run change would be a second code path nobody tests. The only
   reader is walk_window's pacer. See the header for the whole contract. */
int g_frame_rate = 0;
/* run hunt8, lane FPSLIVE1: FrameRate follows a change while the game runs.
   g_frame_rate_gen steps whenever the answer host_setting_frame_rate gives
   moves (the pacer re-reads its cached copy when it sees the step), and
   g_frame_rate_touched says a live change (the settings page's save, or the
   launcher rewriting settings.json) has happened, after which the
   SM64DS_FRAME_RATE override no longer sits in front: the override is the
   boot value, a choice made in game replaces it. */
int g_frame_rate_gen = 0;
int g_frame_rate_touched = 0;

/* ---- THE THREE PICTURE-QUALITY KEYS' BANDS (run hd1) ---------------------
   One sanitiser each, and both readers of every key -- settings.json and the
   environment override -- come through it, so the file and the environment
   cannot disagree about what 9 or -1 means. The shape is FrameRate's exactly.

     RenderScale   not a positive number -> 0, the explicit default sentinel:
                   the multiplier the port picks today. Covers absent,
                   unparseable, 0 itself and negatives. 1..8 are themselves.
                   Above 8 clamps to 8 rather than being refused, the Aspect
                   rule: a number that is a picture beats an error. The
                   ceiling is 8 because 8 x 192 is 1536, the height of the
                   NTR_WIDE_RT allocation (ntr/ppu.h; RESSCALE1 raised it
                   from 4 and 768).
     SmoothModels  the same, clamped into 0..3, which is the range the
                   subdivision itself is defined over (ntr/smooth.h). */
int render_scale_sanitise(int n)
{
    if (n <= 0) return 0;
    if (n > 8) return 8;            /* RESSCALE1: 8 x 192 = 1536 rows, the NTR_WIDE_RT tier's height (ntr/ppu.h) */
    return n;
}

int smooth_models_sanitise(int n)
{
    if (n <= 0) return 0;
    if (n > 3) return 3;
    return n;
}

/* ---- THE TWO PICTURE-SMOOTHING KEYS' BANDS (run hd2) --------------------
   The same shape again, one sanitiser each, both readers through it.

     TextureFilter  0 nearest (the sampler the port has always used), 1
                    bilinear, 2 trilinear. Absent, unparseable, 0 itself and
                    negative all land on 0; above 2 clamps to 2 rather than
                    being refused, the Aspect rule. The ceiling is 2 because
                    trilinear is the last mode the raster has a chain for:
                    anisotropic filtering would need a per-pixel footprint
                    the sampler does not compute.
     AntiAliasing   0 off, 1 edge smoothing. Same band, ceiling 1, because 1
                    is the only mode measured on this renderer. */
int texture_filter_sanitise(int n)
{
    if (n <= 0) return 0;
    if (n > 2) return 2;
    return n;
}

int anti_aliasing_sanitise(int n)
{
    if (n <= 0) return 0;
    if (n > 1) return 1;
    return n;
}

/* ---- THE THREE PRESENT KEYS' BANDS (run hd2, lane GPU1) -----------------
   The same shape once more, one sanitiser each, both readers through it.

     PresentBackend  0 the ordinary Windows path the port has always used
                     (StretchDIBits), 1 Direct3D 11. Absent, unparseable, 0
                     itself and negative all land on 0; above 1 clamps to 1
                     rather than being refused, the Aspect rule. The ceiling
                     is 1 because Direct3D 11 is the only other path that
                     exists.
     PresentFilter   0 nearest (the whole-pixel drop-and-duplicate the port
                     has always presented with), 1 smooth, 2 sharp. Ceiling 2
                     because sharp is the last mode the present shader has.
                     READ ONLY WHEN THE BACKEND IS 1; the Windows path has
                     its own filter knob.
     AntiAliasing's band shape exactly for VSync: 0 off, 1 on, ceiling 1.
     READ ONLY WHEN THE BACKEND IS 1, because the Windows path has no vsync
     of any kind to switch on. */
int present_backend_sanitise(int n)
{
    if (n <= 0) return 0;
    if (n > 1) return 1;
    return n;
}

int present_filter_sanitise(int n)
{
    if (n <= 0) return 0;
    if (n > 2) return 2;
    return n;
}

int vsync_sanitise(int n)
{
    if (n <= 0) return 0;
    if (n > 1) return 1;
    return n;
}

/* ---- THE RENDERER KEY'S BAND (run hd2, lane GPU2) -----------------------
     Renderer   0 the software rasteriser the port has always drawn the 3D
                picture with, which is the default and the only byte-exact
                reference; 1 Direct3D 11, which draws the whole 3D picture on
                the graphics card (the see-through polygons, the shadows and
                the edge smoothing too, run cardfull) and, with PresentBackend
                1, shows it straight from the card. Absent, unparseable, 0
                itself and negative all land on 0; above 1 clamps to 1 rather
                than being refused, the Aspect rule. The ceiling is 1 because
                Direct3D 11 is the only other renderer that exists.
     It is INDEPENDENT of PresentBackend: either can be on without the other,
     and with both on the process still makes exactly one device. */
int renderer_sanitise(int n)
{
    if (n <= 0) return 0;
    if (n > 1) return 1;
    return n;
}

/* The three keys' stored values. BOOT-LATCHED like g_aspect and g_frame_rate
   and for the same kind of reason: the render size is threaded into the
   framebuffer, the pack is opened once and the subdivision level sizes
   per-polygon work, so all three are settled before the first frame and a
   mid-run change would be a second code path nobody tests. */
int g_render_scale = 0;
int g_smooth_models = 0;
/* SmoothMotion (run interp1): 0 off, 1 on. Boot-latched like FrameRate. */
int g_smooth_motion = 0;
/* MinigameUpscale (MGHIRES1): 0 off, 1 the pixel-art upscaler on the minigames' 2D. */
int g_minigame_upscale = 0;
/* MinigameHiRes (MGHIRES1): 0 the DS-pixel picture (the default), 1 the minigames' turned and moving 2D at render scale. */
int g_minigame_hires = 0;

/* run hd2's two, latched beside them and for the same kind of reason: the
   filter mode decides whether the texture cache builds a mip chain at the
   first bind of each texture, and the smoothing pass sizes a scratch buffer
   the width of the picture, so both are settled before the first frame. */
int g_texture_filter = 0;
int g_anti_aliasing = 0;

/* run hd2 lane GPU1's three, latched beside them and for the same kind of
   reason: which backend presents the picture decides which library is loaded
   and which device exists, and the filter sizes nothing but is read at the
   same moment, so all three are settled before the first picture is handed
   over and a mid-run change would be a second code path nobody tests. */
int g_present_backend = 0;
int g_present_filter = 0;
int g_vsync = 0;

/* ---- THE IMPROVED MINIMAP'S TWO KEYS ---------------------------------------

   MinimapScale is a multiplier on the panel the port draws TODAY (half a DS
   screen, so 128x96), which is what "scaled from how it is" means: 1 is the
   current picture, byte for byte.

   IT IS A FREE NUMBER NOW, not one of six rows, because the owner asked to
   drag the map's corner and scale it against the window by hand. The launcher
   keeps its 1 / 1.25 / 1.5 / 2 / 3 / 4 picker and every one of those six is
   still exact; a number between them is just as legal, and the picker's own
   round trip shows the nearest row for one the player dragged to.

   THE ONE THING A SIZE HAS TO BE is a whole number of pixels in both axes.
   The map is drawn from a 256x192 source and keeps its 4:3 shape, so its
   drawn width k must be a multiple of four for 3k/4 to be whole. Every size
   is therefore quantised onto that grid -- steps of four pixels of width,
   three of height, one thirty-second of a multiplier -- and the six picker
   rows land on it exactly: 128, 160, 192, 256, 384 and 512.

   The ratio handed to the compose and to the stylus inverse is k/256, which
   for those six reduces to the 1/2, 5/8, 3/4, 1/1, 3/2 and 2/1 they used
   before and draws the identical picture. */
int g_improved_minimap = 1;   /* ABSENT MEANS ON: the owner's order */

/* SaveMenuOnTop: while the level-clear save menu is up, the two DS screens
   trade places -- the menu becomes the big picture and the course-clear tally
   drops into the corner. ABSENT MEANS ON, the second key in this file with
   that default and for the same reason the improved minimap has it: it is a
   mod the owner asked for and recommended on. 0 is the picture the port drew
   before it existed. */
int g_save_menu_on_top = 1;   /* ABSENT MEANS ON: the owner's recommendation */
double g_minimap_scale = 1.0; /* the multiplier, 1 or more */
/* A HAND HAS MOVED THE SIZE THIS RUN. Set by the drag's own write and by
   nothing else; read in host_setting_minimap_scale_value, where the block
   above that function says what it is for. */
int g_minimap_scale_dragged;

/* THE MAP'S DRAWN WIDTH for a multiplier, on the grid above. Absent, zero,
   negative and unparseable all read as 1, the Aspect rule: a number that is a
   picture beats an error. The ceiling is a sanity bound only -- the real
   limit is "the whole panel still fits in the picture" and only the drawing
   layer knows the picture. */
int minimap_scale_k(double v)
{
    if (!(v > 0.0)) v = 1.0;
    int k = (int)(v * 128.0 + 0.5);
    k = (k + 2) & ~3;
    if (k < 128) k = 128;
    if (k > 4096) k = 4096;
    return k;
}

/* Put a multiplier on the grid, and say through `snapped` whether it had to
   move, so the one caller that wants to say so can say it once rather than
   every reader printing a line. */
double minimap_scale_sanitise(double v, int *snapped)
{
    const double q = minimap_scale_k(v) / 128.0;
    if (snapped) *snapped = (v != q) ? 1 : 0;
    return q;
}

/* run hd2 lane GPU2's one, latched beside them and for the same kind of
   reason: which rasteriser draws the frame decides which library is loaded,
   which device exists and which buffers are allocated on the card, so it is
   settled before the first frame and a mid-run change would be a second code
   path nobody tests. */
int g_renderer = 0;

void load_once(void)
{
    if (g_loaded) return;
    g_loaded = 1;
    g_swap_camera_turn = 0;
    g_run_mode = 1;      /* RunMode analog, on Tango's order */
    g_run_key = 0x10;
    g_run_pad = 0x4000;
    g_camera_mode = 2;   /* CameraMode ds, on Tango's order */
    g_camera_hybrid = -1;   /* HYBRIDCAM1: absent, so ds reads as hybrid */
    g_stick_dead = STICK_DEAD_DEFAULT;
    g_run_thresh_pct = -1;
    g_invert_camera_y = 0;
    g_camera_sens = 100;
    g_cam_stick_accel = g_cam_stick_decel = 0;
    g_mouse_sens = 100;
    for (int i = 0; i < HOST_KEY_COUNT; ++i) g_key[i] = KEY_BIND[i].dflt;
    for (int i = 0; i < HOST_PAD_COUNT; ++i) g_pad[i] = PAD_BIND[i].dflt;
    for (int i = 0; i < HOST_MOUSE_COUNT; ++i) g_mouse[i] = MOUSE_BIND[i].dflt;
    g_mouse_attack_named = 0;     /* CLICK1: no file names it yet */
    g_camera_stick_zoom = 0;
    g_look_key_taken = 0;
    g_look_pad_taken = 0;
    g_cc_key_taken = 0;
    g_cc_pad_taken = 0;
    g_cam_key_named = g_cam_pad_named = 0;   /* LLLSHELL1 */
    g_gap_on = 1;
    g_gap_fill = 1;
    g_audio_quality = 1;
    g_gap_color = 0xFF000000u;
    g_gap_peek = 0;
    g_mouse_capture = 1;     /* MOUSELOCK1: on by default */
    g_tex_swap_key = 0x76;
    g_keep_lives = 0;
    g_skip_opening = 0;
    g_launcher_nick[0] = '\0';
    g_fix_original_bugs = 1;
    g_camera_collision = 1;
    g_wide_drawing = 1;
    g_peach_jingle = 1;
    g_aspect = 0.0;
    g_vs_luigi_seconds = 0;
    g_padlayout_n = 0;
    /* FrameRate: native, one picture per game tick, read here with the other
       defaults so a missing file and a file that will not parse both land on
       the behaviour the port shipped with. */
    g_frame_rate = 0;
    /* The three picture-quality keys, defaulted here beside FrameRate for its
       reason: a missing file and a file that will not parse both have to land
       on the picture the port shipped with, and these are the values that say
       so -- the render multiplier the port picks for itself, the ROM's own
       textures and the ROM's own geometry. */
    g_render_scale = 0;
    g_smooth_models = 0;
    g_smooth_motion = 0;
    g_minigame_upscale = 0;
    g_minigame_hires = 0;
    /* run hd2's two, here for the same reason: nearest sampling and no
       smoothing pass are the picture the port shipped with, so a missing file
       and a file that will not parse both have to land on them. */
    g_texture_filter = 0;
    g_anti_aliasing = 0;
    /* run hd2 lane GPU1's three, here for the same reason: the ordinary
       Windows present path, its own nearest scaler and no vsync are what the
       port has always shipped, so a missing file and a file that will not
       parse both have to land on them. */
    g_present_backend = 0;
    g_renderer = 0;
    g_present_filter = 0;
    g_vsync = 0;

    char path[1024];
    if (!find_settings(path, sizeof path)) return;
    char *text = slurp(path);
    if (!text) return;
    if (looks_like_json_object(text)) {
        g_swap_camera_turn = json_bool(text, "SwapCameraTurnDirection", 0);
        g_invert_camera_y = json_bool(text, "InvertCameraY", 0);
        {
            const int sd = json_int(text, "StickDeadzone", 24);
            g_stick_dead = stick_dead_from_pct(sd);
            const int rt = json_int(text, "RunThreshold", RUN_THRESH_ROM_PCT);
            g_run_thresh_pct = (rt >= 30 && rt <= 100 && rt != RUN_THRESH_ROM_PCT) ? rt : -1;
            int cs = json_int(text, "CameraSensitivity", 100);
            g_camera_sens = cs < 10 ? 10 : cs > 400 ? 400 : cs;
            const int ca = json_int(text, "CameraStickAccel", 0), cd = json_int(text, "CameraStickDecel", 0);
            g_cam_stick_accel = ca < 0 ? 0 : ca > 10 ? 10 : ca;
            g_cam_stick_decel = cd < 0 ? 0 : cd > 10 ? 10 : cd;
            int ms = json_int(text, "MouseSensitivity", 100);
            g_mouse_sens = ms < 10 ? 10 : ms > 400 ? 400 : ms;
        }
        /* the mode by NAME, and by number as well: the launcher serializes a
           C# enum as an integer unless it is told otherwise, and a player
           editing by hand will write the word */
        char mode[16];
        if (json_str(text, "RunMode", mode, sizeof mode)) {
            int matched = 0;
            for (int i = 0; i < 3; ++i)
                if (strlen(mode) == strlen(RUN_MODE_KEY[i]) &&
                    ieq(mode, RUN_MODE_KEY[i], strlen(mode))) {
                    g_run_mode = i;
                    matched = 1;
                }
            if (!matched) {
                const int n = json_int(text, "RunMode", 1);
                if (n >= 0 && n <= 2) g_run_mode = n;
            }
        }
        /* a binding outside the code space is a typo, not a choice */
        {
            const int k = json_int(text, "RunButtonKey", 0x10);
            if (k >= 0 && k <= 0xff) g_run_key = k;
            const int p = json_int(text, "RunButtonPad", 0x4000);
            if (p >= 0 && p <= HOST_PAD_MASK_MAX) g_run_pad = p;
        }
        /* the camera mode, by name and by number like RunMode */
        {
            char mode[16];
            if (json_str(text, "CameraMode", mode, sizeof mode)) {
                int matched = 0;
                for (int i = 0; i < 3; ++i)
                    if (strlen(mode) == strlen(CAMERA_MODE_KEY[i]) &&
                        ieq(mode, CAMERA_MODE_KEY[i], strlen(mode))) {
                        g_camera_mode = i;
                        matched = 1;
                    }
                if (!matched) {
                    const int n = json_int(text, "CameraMode", 2);
                    if (n >= 0 && n <= 2) g_camera_mode = n;
                }
            }
            /* HYBRIDCAM1: see CAMERA_MODE_KEY */
            if (json_value(text, "CameraHybrid")) g_camera_hybrid = json_bool(text, "CameraHybrid", 1) ? 1 : 0;
        }
        /* the bindings, each read against its OWN default so a file that
           moves jump and says nothing about the rest is honoured for jump.
           Out of the code space is a typo, not a choice, like the run pair. */
        int keyrun_ok = 0, padrun_ok = 0;   /* KeyRun / PadRun parsed IN RANGE */
        for (int i = 0; i < HOST_KEY_COUNT; ++i) {
            const int k = json_int(text, KEY_BIND[i].name, KEY_BIND[i].dflt);
            if (k >= 0 && k <= 0xff) {
                g_key[i] = k;
                if (i == 11 && json_value(text, KEY_BIND[i].name)) keyrun_ok = 1;
            }
        }
        for (int i = 0; i < HOST_PAD_COUNT; ++i) {
            const int p = json_int(text, PAD_BIND[i].name, PAD_BIND[i].dflt);
            if (p >= 0 && p <= HOST_PAD_MASK_MAX) {
                g_pad[i] = p;
                if (i == 3 && json_value(text, PAD_BIND[i].name)) padrun_ok = 1;
            }
        }
        for (int i = 0; i < HOST_MOUSE_COUNT; ++i) {
            const int m = json_int(text, MOUSE_BIND[i].name, MOUSE_BIND[i].dflt);
            if (mouse_vk_ok_for(i, m)) g_mouse[i] = m;
            if (i == HOST_MOUSE_ATTACK)     /* CLICK1: a value that parsed and is allowed is the player's; a typo is not */
                g_mouse_attack_named = json_value(text, MOUSE_BIND[i].name) && mouse_vk_ok_for(i, m);
        }
        mouse_attack_default();
        g_camera_stick_zoom = json_bool(text, "CameraStickZoom", 0);
        /* THE ALIAS: KeyRun / PadRun win when the file names them WITH A VALUE
           THAT PARSED; when it does not, the old RunButtonKey / RunButtonPad
           answer stands, which is what the two reads above already left in
           g_run_key / g_run_pad. "Parsed", not "present": a KeyRun of 999 is a
           typo and must not beat a valid RunButtonKey beside it, which a
           presence test would let it do by handing over the default. After
           these two lines g_run_key and g_key[RUN] are one value. */
        if (keyrun_ok) g_run_key = g_key[11];
        else g_key[11] = g_run_key;
        if (padrun_ok) g_run_pad = g_pad[3];
        else g_pad[3] = g_run_pad;
        /* LOOK, THE BINDING NEWER THAN THE FILE (the header's "CAME AFTER
           0.4.2" rule). A file that does not name KeyLook / PadLook holds the
           default from the table above, unless another action in that same
           file already has the default's key or button; then that half stays
           unbound instead of doubling up a choice the player made. After the
           alias lines, so a run moved by its old spelling counts. */
        if (!json_value(text, KEY_BIND[HOST_KEY_LOOK].name))
            for (int i = 0; i < HOST_KEY_COUNT; ++i)
                if (i != HOST_KEY_LOOK && g_key[i] == g_key[HOST_KEY_LOOK]) {
                    g_look_key_taken = i + 1;
                    g_key[HOST_KEY_LOOK] = 0;
                    break;
                }
        if (!json_value(text, PAD_BIND[HOST_PAD_LOOK].name))
            for (int i = 0; i < HOST_PAD_COUNT; ++i)
                if (i != HOST_PAD_LOOK && (g_pad[i] & g_pad[HOST_PAD_LOOK])) {
                    g_look_pad_taken = i + 1;
                    g_pad[HOST_PAD_LOOK] = 0;
                    break;
                }
        /* CENTRE CAMERA, newer than the file like Look: the same rule. The
           other three camera actions are NOT held to it, because each names a
           button the program has always read for the camera (Q / E / LB / RB /
           F1 / the stick click), so a file that moved something onto one of
           them has always had both happen and keeps it. */
        if (!json_value(text, KEY_BIND[HOST_KEY_CAM_CENTER].name))
            for (int i = 0; i < HOST_KEY_COUNT; ++i)
                if (i != HOST_KEY_CAM_CENTER &&
                    g_key[i] == g_key[HOST_KEY_CAM_CENTER]) {
                    g_cc_key_taken = i + 1;
                    g_key[HOST_KEY_CAM_CENTER] = 0;
                    break;
                }
        if (!json_value(text, PAD_BIND[HOST_PAD_CAM_CENTER].name))
            for (int i = 0; i < HOST_PAD_COUNT; ++i)
                if (i != HOST_PAD_CAM_CENTER &&
                    (g_pad[i] & g_pad[HOST_PAD_CAM_CENTER])) {
                    g_cc_pad_taken = i + 1;
                    g_pad[HOST_PAD_CAM_CENTER] = 0;
                    break;
                }
        /* LLLSHELL1: the turn / tilt defaults, by the same rule (see cam_turn_yield) */
        cam_turn_named_from(text);
        cam_turn_yield();
        /* the screen gap. Each key is read against its OWN default, so a file
           that sets one of the four and none of the others is honoured for
           the one it set. */
        g_gap_on = json_bool(text, "MinigameGap", 1);
        g_gap_peek = json_bool(text, "GapPeek", 0);
        g_keep_lives = json_bool(text, "KeepLives", 0);
        g_skip_opening = json_bool(text, "SkipOpening", 0);
        /* PROFILE1: the player's profile name and Yoshi colour. The name is cleaned the way the popup writes it. */
        {
            char nm[40];
            char hb[16], hs[16];
            /* the bros' colours, one key per bro and row */
            for (int c = 0; c < 3; ++c)
                for (int k = 0; k < 4; ++k) {
                    char hx[16];
                }
            g_launcher_nick[0] = '\0';
        }
        g_fix_original_bugs = json_bool(text, "FixOriginalBugs", 1);
        g_camera_collision = json_bool(text, "CameraCollision", 1);
        g_peach_jingle = json_bool(text, "PeachJingle", 1);
        /* the presentation aspect, read against its own default of 0 (native)
           so a file written before this key existed reads as the 4:3 window.
           Sanitised HERE rather than at the accessor, so the stored value is
           always one the render path can size a framebuffer from. Boot-latched:
           walk_window's boot is the only reader and it reads once. */
        {
            double a = json_num(text, "Aspect", 0.0);
            /* LEGACY: the boolean this key replaced. Only consulted when Aspect
               itself is absent, so a file carrying both is decided by the newer
               key and nobody has to migrate anything by hand. */
            if (!json_value(text, "Aspect") && json_bool(text, "Widescreen", 0))
                a = 16.0 / 9.0;
            g_aspect = aspect_sanitise(a);
        }
        {
            char who[24];
            /* "", an absent key and any other spelling are all Yoshi */
        }
        {
            char mode[16];
            if (json_str(text, "GapFillMode", mode, sizeof mode)) {
                if (strlen(mode) == 5 && ieq(mode, "solid", 5)) g_gap_fill = 0;
                else if (strlen(mode) == 7 && ieq(mode, "ambient", 7))
                    g_gap_fill = 1;
                else if (strlen(mode) == 6 && ieq(mode, "custom", 6))
                    g_gap_fill = 2;
                /* anything else keeps the default, like every other reader */
            }
            char col[16];
            if (json_str(text, "GapColor", col, sizeof col))
                g_gap_color = parse_hex_color(col, 0xFF000000u);
        }
        {
            const int v = json_int(text, "Volume", -1);
            if (v >= 0) g_volume = v > 100 ? 100 : v;
        }
        /* AudioQuality (lane AUDIOHQ1): "hq" (the default; an absent key, an
           absent file and anything else read as this) mixes at 48000 Hz with a
           4-point Hermite resampler; "ds" is the DS's own 32768 Hz mixer with
           linear interpolation, bit-identical to builds before the key. Read
           once at start: the mixer's rate cannot change while the game runs.
           SM64DS_AUDIO_HQ=0 (or 1) in the environment overrides it. */
        {
            char q[16];
            if (json_str(text, "AudioQuality", q, sizeof q)) {
                if (strlen(q) == 2 && ieq(q, "ds", 2)) g_audio_quality = 0;
            }
        }
        /* NetMode: the transport's mode word. The default (again as of 0.3.7,
           and what an absent key, an absent file and an unparseable file all
           read as) or the one opt-in word; only that word moves it;
           anything else keeps the default, like every other reader. Read once
           at load; the transport asks at install, SM64DS_NETMODE overrides
           there, and a seated session wider than the transport's player cap
           (host_settings.h) takes the opt-in mode regardless. */
        {
            char nm[16];
        }
        /* MOUSELOCK1: a file that never named the key gets the default, which
           is on now; a file that saved false keeps false */
        g_mouse_capture = json_bool(text, "MouseCapture", 1);
        g_start_fullscreen = json_bool(text, "StartFullscreen", 0);
        g_draw_dist = draw_dist_clamp(json_int(text, "DrawDistance", 100));
        /* the learned controller maps; absent is none, like every key */
        padlayouts_parse(text);
        /* FrameRate: the presentation rate, read against its own default of 0
           (native) so a file written before this key existed reads as one
           picture per game tick. Either a whole number or the word "display",
           which is the primary display's refresh rate read once here.
           Sanitised HERE rather than at the accessor, the Aspect shape, so the
           stored value is always one the pacer can keep. */
        {
            char fr[16];
            int n;
            if (json_str(text, "FrameRate", fr, sizeof fr) &&
                strlen(fr) == 7 && ieq(fr, "display", 7))
                n = display_refresh_hz();
            else
                n = json_int(text, "FrameRate", 0);
            g_frame_rate = frame_rate_sanitise(n);
        }
        /* The three picture-quality keys, each read against its own default
           beside FrameRate and sanitised HERE rather than at the accessor --
           the Aspect shape -- so the stored value is always one the render
           path can size a framebuffer, open a pack or subdivide a polygon
           from. A file written before these keys existed reads as one that
           left all three off, which is the shipped picture. */
        g_render_scale = render_scale_sanitise(json_int(text, "RenderScale", 0));
        /* THE IMPROVED MINIMAP, and the default is the odd one in this file:
           ABSENT IS ON. Every other feature key here defaults off so a file
           written before the key existed reads as the shipped picture; this
           one is on by the owner's explicit order, so a file with no
           ImprovedMinimap line gets the improved map. Both spellings of the
           toggle, the RunMode rule: the launcher serialises a C# bool and a
           player editing by hand may write 1. */
        g_improved_minimap = (json_int(text, "ImprovedMinimap", 1) != 0 &&
                              json_bool(text, "ImprovedMinimap", 1) != 0) ? 1 : 0;
        /* THE SAVE MENU ON THE TOP SCREEN, the same absent-is-on shape and
           both spellings of the toggle, because the launcher serialises a C#
           bool and a player editing by hand may write 1. */
        g_save_menu_on_top = (json_int(text, "SaveMenuOnTop", 1) != 0 &&
                              json_bool(text, "SaveMenuOnTop", 1) != 0) ? 1 : 0;
        {
            int snapped = 0;
            const double want = json_num(text, "MinimapScale", 1.0);
            g_minimap_scale = minimap_scale_sanitise(want, &snapped);
            if (snapped)
                fprintf(stderr, "[settings] MinimapScale %g is not a whole "
                        "number of pixels in both axes -- using %g, the "
                        "nearest that is\n", want, g_minimap_scale);
        }
        g_smooth_models =
            smooth_models_sanitise(json_int(text, "SmoothModels", 0));
        /* SmoothMotion: both spellings of a toggle, the same rule as the toggle above. */
        g_smooth_motion = (json_int(text, "SmoothMotion", 0) != 0 ||
                           json_bool(text, "SmoothMotion", 0) != 0) ? 1 : 0;
        g_minigame_upscale = (json_int(text, "MinigameUpscale", 0) != 0 ||
                              json_bool(text, "MinigameUpscale", 0) != 0) ? 1 : 0;
        /* MinigameHiRes: both spellings of a toggle, the same rule (absent = off) */
        g_minigame_hires = (json_int(text, "MinigameHiRes", 0) != 0 ||
                            json_bool(text, "MinigameHiRes", 0) != 0) ? 1 : 0;
        /* run hd2's two, read the same way and sanitised here rather than at
           the accessor, so the stored value is always one the sampler can
           choose a tap count from and the smoothing pass can size itself
           against. A file written before these keys existed reads as one that
           left both off, which is the shipped picture. */
        g_texture_filter =
            texture_filter_sanitise(json_int(text, "TextureFilter", 0));
        g_anti_aliasing =
            anti_aliasing_sanitise(json_int(text, "AntiAliasing", 0));
        /* run hd2 lane GPU1's three, read the same way and sanitised here
           rather than at the accessor, so the stored value is always one the
           present path can pick a backend, a sampler and a wait from. A file
           written before these keys existed reads as one that left all three
           off, which is the present path the port shipped with. Both
           spellings of the two toggles, the RunMode rule: the launcher
           serialises a C# bool as true/false and a player editing by hand may
           write 1. */
        g_present_backend = present_backend_sanitise(
            (json_int(text, "PresentBackend", 0) != 0 ||
             json_bool(text, "PresentBackend", 0) != 0) ? 1 : 0);
        g_present_filter =
            present_filter_sanitise(json_int(text, "PresentFilter", 0));
        g_vsync = vsync_sanitise((json_int(text, "VSync", 0) != 0 ||
                                  json_bool(text, "VSync", 0) != 0) ? 1 : 0);
        /* run hd2 lane GPU2's one, read the same way and sanitised here for
           the same reason. A file written before this key existed reads as one
           that left it off, which is the software rasteriser the port has
           always drawn with. Both spellings, the RunMode rule. */
        g_renderer = renderer_sanitise(
            (json_int(text, "Renderer", 0) != 0 ||
             json_bool(text, "Renderer", 0) != 0) ? 1 : 0);
    }
    free(text);

    /* Said out loud only when a setting is off its default, so an ordinary
       run's log is unchanged and a support log shows at a glance which
       non-default choices were in force. */
    if (g_swap_camera_turn)
        fprintf(stderr, "[settings] SwapCameraTurnDirection on (%s)\n", path);
    /* Off its default, so it is said once: this copy keeps the level-clear
       save menu on the corner panel instead of composing it onto the big
       picture. */
    if (!g_save_menu_on_top)
        fprintf(stderr, "[settings] SaveMenuOnTop off -- the save menu "
                "after a star stays on the corner panel (%s)\n", path);
    if (g_run_mode != 1 || g_run_key != 0x10 || g_run_pad != 0x4000)
        fprintf(stderr, "[settings] RunMode %s key 0x%02x pad 0x%04x (%s)\n",
                RUN_MODE_KEY[g_run_mode], (unsigned)g_run_key,
                (unsigned)g_run_pad, path);
    if (g_camera_mode != 2)
        fprintf(stderr, "[settings] CameraMode %s (%s)\n",
                CAMERA_MODE_KEY[g_camera_mode], path);
    else   /* HYBRIDCAM1: which of the two ds readings this file is */
        fprintf(stderr, "[settings] CameraMode %s (CameraHybrid %s) (%s)\n",
                g_camera_hybrid != 0 ? "hybrid" : "ds",
                g_camera_hybrid < 0 ? "absent: never picked" : (g_camera_hybrid ? "true" : "false"), path);
    for (int i = 0; i < g_padlayout_n; ++i)
        fprintf(stderr, "[settings] PadLayouts: learned layout for %04x:%04x "
                        "%s (%s)\n", (unsigned)g_padlayouts[i].vid,
                (unsigned)g_padlayouts[i].pid, g_padlayouts[i].name, path);
    for (int i = 0; i < HOST_MOUSE_COUNT; ++i)
        if (g_mouse[i] != MOUSE_BIND[i].dflt)
            fprintf(stderr, "[settings] %s %d (default %d) (%s)\n",
                    MOUSE_BIND[i].name, g_mouse[i], MOUSE_BIND[i].dflt, path);
    fprintf(stderr, "[settings] MouseAttack %d: %s (%s)\n", g_mouse[HOST_MOUSE_ATTACK],
            g_mouse_attack_named ? "the player's choice"
            : g_mouse[HOST_MOUSE_ATTACK] ? "never chosen, left click attacks (CLICK1 default)"
                                         : "never chosen, left is on another mouse action, so not Attack", path);
    if (g_camera_stick_zoom)
        fprintf(stderr, "[settings] CameraStickZoom on: right stick up/down zooms the camera (%s)\n", path);
    /* one line per binding the player moved, so a support log answers "what
       was jump bound to" without anyone opening the file */
    for (int i = 0; i < HOST_KEY_COUNT; ++i)
        if (g_key[i] != KEY_BIND[i].dflt)
            fprintf(stderr, "[settings] %s 0x%02x (default 0x%02x) (%s)\n",
                    KEY_BIND[i].name, (unsigned)g_key[i],
                    (unsigned)KEY_BIND[i].dflt, path);
    for (int i = 0; i < HOST_PAD_COUNT; ++i)
        if (g_pad[i] != PAD_BIND[i].dflt)
            fprintf(stderr, "[settings] %s 0x%04x (default 0x%04x) (%s)\n",
                    PAD_BIND[i].name, (unsigned)g_pad[i],
                    (unsigned)PAD_BIND[i].dflt, path);
    if (g_look_key_taken)
        fprintf(stderr, "[settings] KeyLook left unbound: its default key "
                        "0x%02x is already %s (%s)\n",
                (unsigned)KEY_BIND[HOST_KEY_LOOK].dflt,
                KEY_BIND[g_look_key_taken - 1].name, path);
    if (g_look_pad_taken)
        fprintf(stderr, "[settings] PadLook left unbound: its default button "
                        "0x%04x is already %s (%s)\n",
                (unsigned)PAD_BIND[HOST_PAD_LOOK].dflt,
                PAD_BIND[g_look_pad_taken - 1].name, path);
    if (g_cc_key_taken)
        fprintf(stderr, "[settings] KeyCamCenter left unbound: its default key "
                        "0x%02x is already %s (%s)\n",
                (unsigned)KEY_BIND[HOST_KEY_CAM_CENTER].dflt,
                KEY_BIND[g_cc_key_taken - 1].name, path);
    if (g_cc_pad_taken)
        fprintf(stderr, "[settings] PadCamCenter left unbound: its default "
                        "button 0x%04x is already %s (%s)\n",
                (unsigned)PAD_BIND[HOST_PAD_CAM_CENTER].dflt,
                PAD_BIND[g_cc_pad_taken - 1].name, path);
    for (int j = 0; j < 4; ++j)   /* LLLSHELL1 */
        if (g_cam_key_taken[j])
            fprintf(stderr, "[settings] %s left unbound: its default key 0x%02x is already %s (%s)\n",
                    KEY_BIND[CAM_YIELD_KEY[j]].name, (unsigned)KEY_BIND[CAM_YIELD_KEY[j]].dflt,
                    KEY_BIND[g_cam_key_taken[j] - 1].name, path);
    for (int j = 0; j < 2; ++j)
        if (g_cam_pad_taken[j])
            fprintf(stderr, "[settings] %s left unbound: its default button 0x%04x is already %s (%s)\n",
                    PAD_BIND[CAM_YIELD_PAD[j]].name, (unsigned)PAD_BIND[CAM_YIELD_PAD[j]].dflt,
                    PAD_BIND[g_cam_pad_taken[j] - 1].name, path);
    if (!g_gap_on || g_gap_fill != 1 || g_gap_color != 0xFF000000u ||
        g_gap_peek)
        fprintf(stderr, "[settings] MinigameGap %s, fill %s #%06x, peek %s "
                "(%s)\n", g_gap_on ? "on" : "OFF",
                g_gap_fill == 0   ? "solid"
                : g_gap_fill == 2 ? "custom"
                                  : "ambient",
                g_gap_color & 0xffffffu, g_gap_peek ? "ON" : "off", path);
    if (g_mouse_capture)
        fprintf(stderr, "[settings] MouseCapture on -- an adventure window "
                        "holds the pointer and bare mouse movement turns the "
                        "camera, with no right button held. Escape opens the "
                        "settings page, which hands the pointer back. (%s)\n", path);
    /* Said on its own line and in plain words, like the picture keys below: a
       support log for "it looks smoother than a DS" should carry the reason.
       And it says what it does NOT do, because the honest sentence at this rung
       is that the picture repeats. */
    if (g_frame_rate)
        fprintf(stderr, "[settings] FrameRate %d -- the finished picture is "
                        "handed to the display %d times a second instead of "
                        "once per game tick. The game still ticks at its own "
                        "rate and nothing is interpolated yet, so the same "
                        "picture repeats. This is a mod, not the game. "
                        "(%s)\n", g_frame_rate, g_frame_rate, path);
    /* The three picture-quality keys, one plain line each and only when the
       key is off its default, so an ordinary run's log is unchanged and a
       support log for "it looks different from the video" names the reason on
       one line. Each says what it changes and that it is a host setting and
       not the game. */
    if (g_render_scale)
        fprintf(stderr, "[settings] RenderScale %d -- the 3D picture is drawn "
                        "at %d host rows per DS row (%d rows; the width "
                        "follows this run's Aspect) and presented into the "
                        "same window. The game is untouched; this is how "
                        "sharp the picture is and nothing else. (%s)\n",
                g_render_scale, g_render_scale, g_render_scale * 192, path);
    if (g_smooth_motion)
        fprintf(stderr, "[settings] SmoothMotion on -- with FrameRate above "
                        "the game's tick rate, the pictures between two ticks "
                        "are drawn with the moving 3D blended between them "
                        "(%s)\n", path);
    if (g_smooth_models)
        fprintf(stderr, "[settings] SmoothModels %d -- the game's models are "
                        "subdivided %d level(s) before they are drawn, so the "
                        "silhouettes are rounder than the ROM's. This is a "
                        "mod, not the game. (%s)\n",
                g_smooth_models, g_smooth_models, path);
    /* run hd2's two, one plain line each and only off their default, the rule
       every key above follows. */
    if (g_texture_filter)
        fprintf(stderr, "[settings] TextureFilter %d -- textures are sampled "
                        "%s instead of one texel per pixel, so surfaces read "
                        "smoother and the texel grid stops showing up close. "
                        "This is a mod, not the game. (%s)\n",
                g_texture_filter,
                g_texture_filter >= 2
                    ? "trilinear (four texels blended, at the two texture "
                      "sizes nearest the surface's distance)"
                    : "bilinear (the four texels around the sample point, "
                      "blended)",
                path);
    if (g_anti_aliasing)
        fprintf(stderr, "[settings] AntiAliasing %d -- the edges of the 3D "
                        "picture are smoothed after it is drawn and before "
                        "anything 2D is put over it, so text and the HUD are "
                        "untouched. This is a mod, not the game. (%s)\n",
                g_anti_aliasing, path);
    /* run hd2 lane GPU1's three, one plain line each and only off their
       default, the rule every key above follows. The filter and the wait are
       said on the backend's line rather than on lines of their own, because
       with the backend off they are settings nothing reads and a support log
       that listed them as if they were in force would be lying. */
    if (g_present_backend)
        fprintf(stderr, "[settings] PresentBackend 1 -- the finished picture is "
                        "handed to the screen through the graphics card "
                        "(Direct3D 11) instead of the ordinary Windows drawing "
                        "call. Scaling %s, VSync %s. The picture itself is "
                        "drawn exactly as before; this is only how it reaches "
                        "the screen, and it falls back to the old way if the "
                        "card will not have it. (%s)\n",
                g_present_filter >= 2 ? "sharp"
                                      : (g_present_filter == 1 ? "smooth"
                                                               : "nearest"),
                g_vsync ? "on" : "off", path);
    else if (g_present_filter || g_vsync)
        fprintf(stderr, "[settings] PresentFilter %d and VSync %d are set but "
                        "PresentBackend is 0, so nothing reads them: the "
                        "ordinary Windows present path has its own scaler and "
                        "no vsync. (%s)\n", g_present_filter, g_vsync, path);
    /* run hd2 lane GPU2's one, one plain line and only off its default. It is
       a line of its own rather than a clause on the backend's, because the two
       settings are independent: either can be on without the other. */
    if (g_renderer)
        fprintf(stderr, "[settings] Renderer 1 -- the whole 3D picture (the "
                        "solid part, the see-through polygons, the shadows and "
                        "the edge smoothing) is drawn by the graphics card "
                        "instead of by the processor, and with PresentBackend "
                        "1 it is shown straight from the card. The game itself "
                        "runs exactly as before. It is not pixel for pixel the "
                        "same picture as the ordinary renderer, and it falls "
                        "back to it if the card will not have it. (%s)\n", path);
}

/* ---- the live re-read -----------------------------------------------------
   What the file's write time was when it was last read, so a poll can tell
   "changed" from "looked at". Zero until the first poll finds the file. */
#ifdef _WIN32
unsigned long long g_watch_time;
unsigned long long g_watch_size;
#endif

/* One mode word by NAME or by number, the way load_once reads RunMode and
   CameraMode, landing on `cur` when the key is absent or unreadable. */
int read_mode(const char *text, const char *key, const char *const *names,
              int n, int cur)
{
    char mode[16];
    if (!json_value(text, key)) return cur;
    if (json_str(text, key, mode, sizeof mode)) {
        for (int i = 0; i < n; ++i)
            if (strlen(mode) == strlen(names[i]) &&
                ieq(mode, names[i], strlen(mode)))
                return i;
    }
    const int v = json_int(text, key, cur);
    return (v >= 0 && v < n) ? v : cur;
}

/* The control and camera keys a launcher edit can change while the game runs
   (run menu1 SETLIVE1): bindings, run mode, camera mode and the new numbers.
   Each lands on the value it already has when the file stops naming it.
   Returns 1 when anything moved, which bumps g_ctl_gen so the frame loop
   re-copies its working values. */
int reload_controls(const char *text)
{
    int changed = 0;
#define CTL_SET(var, expr) do { const int nv_ = (expr); if (nv_ != (var)) { (var) = nv_; changed = 1; } } while (0)
    CTL_SET(g_swap_camera_turn, json_bool(text, "SwapCameraTurnDirection", g_swap_camera_turn));
    CTL_SET(g_invert_camera_y, json_bool(text, "InvertCameraY", g_invert_camera_y));
    CTL_SET(g_run_mode, read_mode(text, "RunMode", RUN_MODE_KEY, 3, g_run_mode));
    {
        const int was_cam = camera_mode_eff();
        CTL_SET(g_camera_mode, read_mode(text, "CameraMode", CAMERA_MODE_KEY, 3, g_camera_mode));
        if (json_value(text, "CameraHybrid"))   /* HYBRIDCAM1 */
            CTL_SET(g_camera_hybrid, json_bool(text, "CameraHybrid", 1) ? 1 : 0);
        if (camera_mode_eff() != was_cam) ++g_cam_gen;
    }
    CTL_SET(g_stick_dead, stick_dead_from_pct(
        json_int(text, "StickDeadzone", stick_dead_to_pct(g_stick_dead))));
    {
        const int cur = g_run_thresh_pct < 0 ? RUN_THRESH_ROM_PCT : g_run_thresh_pct;
        int rt = json_int(text, "RunThreshold", cur);
        if (rt < 30 || rt > 100 || rt == RUN_THRESH_ROM_PCT) rt = (rt == RUN_THRESH_ROM_PCT) ? -1 : g_run_thresh_pct;
        CTL_SET(g_run_thresh_pct, rt);
    }
    {
        int cs = json_int(text, "CameraSensitivity", g_camera_sens);
        CTL_SET(g_camera_sens, cs < 10 ? 10 : cs > 400 ? 400 : cs);
        const int ca = json_int(text, "CameraStickAccel", g_cam_stick_accel), cd = json_int(text, "CameraStickDecel", g_cam_stick_decel);
        CTL_SET(g_cam_stick_accel, ca < 0 ? 0 : ca > 10 ? 10 : ca);
        CTL_SET(g_cam_stick_decel, cd < 0 ? 0 : cd > 10 ? 10 : cd);
        int ms = json_int(text, "MouseSensitivity", g_mouse_sens);
        CTL_SET(g_mouse_sens, ms < 10 ? 10 : ms > 400 ? 400 : ms);
    }
    for (int i = 0; i < HOST_KEY_COUNT; ++i) {
        int k = json_int(text, KEY_BIND[i].name, g_key[i]);
        if (i == HOST_KEY_RUN && !json_value(text, KEY_BIND[i].name))
            k = json_int(text, "RunButtonKey", g_key[i]);
        if (k >= 0 && k <= 0xff) CTL_SET(g_key[i], k);
    }
    for (int i = 0; i < HOST_PAD_COUNT; ++i) {
        int k = json_int(text, PAD_BIND[i].name, g_pad[i]);
        if (i == HOST_PAD_RUN && !json_value(text, PAD_BIND[i].name))
            k = json_int(text, "RunButtonPad", g_pad[i]);
        if (k >= 0 && k <= HOST_PAD_MASK_MAX) CTL_SET(g_pad[i], k);
    }
    for (int i = 0; i < HOST_MOUSE_COUNT; ++i) {
        const int m = json_int(text, MOUSE_BIND[i].name, g_mouse[i]);
        if (mouse_vk_ok_for(i, m)) CTL_SET(g_mouse[i], m);
    }
    {   /* CLICK1: the launcher's Reset takes MouseAttack out of the file; the default then answers again */
        const int m = json_int(text, "MouseAttack", -1);
        g_mouse_attack_named = json_value(text, "MouseAttack") && mouse_vk_ok_for(HOST_MOUSE_ATTACK, m);
        const int was = g_mouse[HOST_MOUSE_ATTACK];
        mouse_attack_default();
        const int now = g_mouse[HOST_MOUSE_ATTACK];
        g_mouse[HOST_MOUSE_ATTACK] = was;
        CTL_SET(g_mouse[HOST_MOUSE_ATTACK], now);
    }
    CTL_SET(g_camera_stick_zoom, json_bool(text, "CameraStickZoom", g_camera_stick_zoom));
    {   /* LLLSHELL1: the turn / tilt defaults follow a gameplay key moved on or off them (cam_turn_yield) */
        int was_k[4], was_p[2];
        for (int j = 0; j < 4; ++j) was_k[j] = g_key[CAM_YIELD_KEY[j]];
        for (int j = 0; j < 2; ++j) was_p[j] = g_pad[CAM_YIELD_PAD[j]];
        cam_turn_named_from(text);
        cam_turn_yield();
        for (int j = 0; j < 4; ++j) if (was_k[j] != g_key[CAM_YIELD_KEY[j]]) changed = 1;
        for (int j = 0; j < 2; ++j) if (was_p[j] != g_pad[CAM_YIELD_PAD[j]]) changed = 1;
    }
#undef CTL_SET
    g_run_key = g_key[HOST_KEY_RUN];
    g_run_pad = g_pad[HOST_PAD_RUN];
    if (changed) {
        ++g_ctl_gen;
        fprintf(stderr, "[settings] live re-read: controls/camera changed "
                "(run %d camera %d swap %d invertY %d dead %d runthr %d "
                "camsens %d mousesens %d)\n", g_run_mode, g_camera_mode,
                g_swap_camera_turn, g_invert_camera_y, g_stick_dead,
                g_run_thresh_pct, g_camera_sens, g_mouse_sens);
    }
    return changed;
}

/* Re-read ONLY the keys the header promises reload live: the four screen-gap
   keys, Volume, MouseCapture and the control keys. Returns 1 when an answer
   changed. Each key lands on the value it already has rather than its default
   when the file no longer names it, because "the launcher stopped writing a key" and "the
   player turned a key off" are different sentences and only the second one has
   a picture. */
int reload_live(const char *text)
{
    int changed = 0;
    const int gap = json_bool(text, "MinigameGap", g_gap_on);
    const int peek = json_bool(text, "GapPeek", g_gap_peek);
    const int mcap = json_bool(text, "MouseCapture", g_mouse_capture);
    int fill = g_gap_fill;
    unsigned color = g_gap_color;
    int vol = g_volume;
    {
        char mode[16];
        if (json_str(text, "GapFillMode", mode, sizeof mode)) {
            if (strlen(mode) == 5 && ieq(mode, "solid", 5)) fill = 0;
            else if (strlen(mode) == 7 && ieq(mode, "ambient", 7)) fill = 1;
            else if (strlen(mode) == 6 && ieq(mode, "custom", 6)) fill = 2;
        }
        char col[16];
        if (json_str(text, "GapColor", col, sizeof col))
            color = parse_hex_color(col, g_gap_color);
    }
    {
        const int v = json_int(text, "Volume", -1);
        if (v >= 0) vol = v > 100 ? 100 : v;
    }
    /* the control keys: read UNCONDITIONALLY and before the compare below, because
       they own their own change detection. Folding them into the gap
       comparison would have meant a controls-only edit either changing nothing
       (if the read were inside the if) or being announced as a gap change. */
    if (reload_controls(text)) changed = 1;
    /* FrameRate (run hunt8, lane FPSLIVE1): live. Read the way load_once reads
       it, through the same sanitiser, and compared with the value the FILE
       last gave (g_frame_rate), so a launcher edit that leaves the key alone
       cannot knock an SM64DS_FRAME_RATE override off its value. */
    {
        char fr[16];
        int n;
        if (json_str(text, "FrameRate", fr, sizeof fr) &&
            strlen(fr) == 7 && ieq(fr, "display", 7))
            n = display_refresh_hz();
        else
            n = json_int(text, "FrameRate", g_frame_rate);
        n = frame_rate_sanitise(n);
        if (n != g_frame_rate) {
            g_frame_rate = n;
            g_frame_rate_touched = 1;
            ++g_frame_rate_gen;
            changed = 1;
            fprintf(stderr, "[settings] live re-read: FrameRate %d\n", n);
        }
    }
    /* DrawDistance: presentation only, so it follows the file at once */
    g_draw_dist = draw_dist_clamp(json_int(text, "DrawDistance", g_draw_dist));
    {
        const int kv = json_bool(text, "KeepLives", g_keep_lives);
        if (kv != g_keep_lives) {
            g_keep_lives = kv;
            changed = 1;
            fprintf(stderr, "[settings] live re-read: KeepLives %s\n", kv ? "ON" : "off");
        }
    }
    {
        const int so = json_bool(text, "SkipOpening", g_skip_opening);
        if (so != g_skip_opening) {
            g_skip_opening = so;
            changed = 1;
            fprintf(stderr, "[settings] live re-read: SkipOpening %s\n", so ? "ON" : "off");
        }
    }
    {
        const int fv = json_bool(text, "FixOriginalBugs", g_fix_original_bugs);
        if (fv != g_fix_original_bugs) {
            g_fix_original_bugs = fv;
            changed = 1;
            fprintf(stderr, "[settings] live re-read: FixOriginalBugs %s\n", fv ? "ON" : "off");
        }
    }
    {
        const int cc = json_bool(text, "CameraCollision", g_camera_collision);
        if (cc != g_camera_collision) {
            g_camera_collision = cc;
            changed = 1;
            fprintf(stderr, "[settings] live re-read: CameraCollision %s\n", cc ? "ON" : "off");
        }
    }
    {
        const int pv = json_bool(text, "PeachJingle", g_peach_jingle);
        if (pv != g_peach_jingle) {
            g_peach_jingle = pv;
            changed = 1;
            fprintf(stderr, "[settings] live re-read: PeachJingle %s\n", pv ? "ON" : "off");
        }
    }
    if (gap != g_gap_on || peek != g_gap_peek || fill != g_gap_fill ||
        color != g_gap_color || vol != g_volume || mcap != g_mouse_capture) {
        g_gap_on = gap;
        g_gap_peek = peek;
        g_gap_fill = fill;
        g_gap_color = color;
        g_volume = vol;
        g_mouse_capture = mcap;
        changed = 1;
        fprintf(stderr, "[settings] live re-read: MinigameGap %s, fill %s "
                "#%06x, peek %s, volume %d, mouse capture %s\n",
                g_gap_on ? "on" : "OFF",
                g_gap_fill == 0   ? "solid"
                : g_gap_fill == 2 ? "custom"
                                  : "ambient",
                g_gap_color & 0xffffffu, g_gap_peek ? "ON" : "off", g_volume,
                g_mouse_capture ? "ON" : "off");
    }
    return changed;
}

}  /* namespace */

extern "C" int host_setting_swap_camera_turn(void)
{
    load_once();
    return g_swap_camera_turn;
}

extern "C" int host_camera_turn_sign(void)
{
    load_once();
    return g_swap_camera_turn ? 1 : -1;
}

/* RunMode: 0 button, 1 analog, 2 auto. See the block above load_once for
   what each one is and why the DS did not need any of them. */
extern "C" int host_setting_run_mode(void)
{
    load_once();
    return g_run_mode;
}

/* The keyboard virtual-key code and the XInput button mask bound to running.
   Zero on either one means that device has no run binding. */
extern "C" int host_setting_run_key(void)
{
    load_once();
    return g_run_key;
}

extern "C" int host_setting_run_pad(void)
{
    load_once();
    return g_run_pad;
}

/* CameraMode: 0 analog, 1 freecam, 2 ds, 3 hybrid (HYBRIDCAM1). See the header. */
extern "C" int host_setting_camera_mode(void)
{
    load_once();
    return camera_mode_eff();
}

/* The bindings. An index outside the table is 0, unbound, so a caller that
   grew an action before this file did gets "nothing" and not a wild read. */
extern "C" int host_setting_key(int action)
{
    load_once();
    if (action < 0 || action >= HOST_KEY_COUNT) return 0;
    return g_key[action];
}

extern "C" int host_setting_pad(int action)
{
    load_once();
    if (action < 0 || action >= HOST_PAD_COUNT) return 0;
    return g_pad[action];
}

extern "C" int host_setting_mouse(int action)
{
    load_once();
    if (action < 0 || action >= HOST_MOUSE_COUNT) return 0;
    return g_mouse[action];
}
extern "C" const char *host_setting_mouse_name(int action)
{
    if (action < 0 || action >= HOST_MOUSE_COUNT) return "";
    return MOUSE_BIND[action].name;
}
extern "C" int host_setting_camera_stick_zoom(void)
{
    load_once();
    return g_camera_stick_zoom;
}

extern "C" const char *host_setting_key_name(int action)
{
    if (action < 0 || action >= HOST_KEY_COUNT) return "";
    return KEY_BIND[action].name;
}

extern "C" const char *host_setting_pad_name(int action)
{
    if (action < 0 || action >= HOST_PAD_COUNT) return "";
    return PAD_BIND[action].name;
}

/* The four screen-gap keys. See the block above load_once for what each one
   means; hal/screen_gap.cpp is the only reader. */
extern "C" int host_setting_minigame_gap(void)
{
    load_once();
    return g_gap_on;
}

extern "C" int host_setting_gap_fill_mode(void)
{
    load_once();
    return g_gap_fill;
}

extern "C" unsigned host_setting_gap_color(void)
{
    load_once();
    return g_gap_color;
}

extern "C" int host_setting_gap_peek(void)
{
    load_once();
    return g_gap_peek;
}







/* AudioQuality: 1 "hq" (default), 0 "ds". */
extern "C" int host_setting_audio_quality(void)
{
    load_once();
    return g_audio_quality;
}

extern "C" int host_setting_volume(void)
{
    load_once();
    return g_volume;
}


/* MouseCapture: 1 when the window may hold the pointer. Only ever a
   PERMISSION -- the window decides whether to take it this frame, and the
   header lists the seven places where the answer is no however this reads. */
extern "C" int host_setting_mouse_capture(void)
{
    load_once();
    return g_mouse_capture;
}


/* DrawDistance, percent. SM64DS_DRAW_DISTANCE=<100..10000> pins it for a proof
   run without touching a player's file. Read per actor per frame, so cheap. */
extern "C" int host_setting_draw_distance_pct(void)
{
    static int env = -2;
    load_once();
    if (env == -2) {
        const char *e = getenv("SM64DS_DRAW_DISTANCE");
        env = e ? draw_dist_clamp(atoi(e)) : -1;
    }
    return env >= 0 ? env : g_draw_dist;
}

extern "C" int host_setting_start_fullscreen(void)
{
    load_once();
    return g_start_fullscreen;
}







static int g_adventure_env = -2;   /* SEAMHUB1: the env latch below, out where the in-place leave can re-arm it */


/* Aspect: width divided by height, 0 for the native 4:3 window. Same shape as
   host_setting_run_mode / host_setting_camera_mode above -- load_once, then the
   stored value -- with two environment overrides in front of it, because a proof
   run has to capture both aspects off ONE build without editing a player's
   settings file.

     SM64DS_ASPECT       the ratio directly, through the same sanitiser the file
                         goes through: 0 native, 1.7777778 16:9, 9.0 clamps to
                         3.0, junk reads as 0.
     SM64DS_WIDESCREEN   the LEGACY boolean, kept because it is one line and
                         because the proof scripts written before the key became
                         a number spell it that way. 0 forces native, anything
                         else forces 16:9. Only consulted when SM64DS_ASPECT is
                         unset, so the newer variable wins if both are set.

   Read once at boot -- the aspect is latched into the framebuffer and cannot
   change mid-run. */
extern "C" double host_setting_aspect(void)
{
    static int env_read = 0;
    static double env = -1.0;        /* <0 means "the environment said nothing" */
    if (!env_read) {
        env_read = 1;
        const char *e = getenv("SM64DS_ASPECT");
        if (e && *e) {
            char *end = 0;
            const double d = strtod(e, &end);
            /* an unparseable override is still an override: it says "native",
               the same answer an unparseable file value gives */
            env = (end != e) ? aspect_sanitise(d) : 0.0;
        } else if ((e = getenv("SM64DS_WIDESCREEN")) != 0 && *e) {
            env = (e[0] == '0' && e[1] == 0) ? 0.0 : 16.0 / 9.0;
        }
    }
    if (env >= 0.0) return env;
    load_once();
    return g_aspect;
}



/* The C++ predicate the header promises. One reader, so the render and
   no-collision paths cannot drift from the accessor's answer. */
namespace port {
}  // namespace port

/* SEAMHUB1: leaving a world in place takes SM64DS_ADVENTURE out of the environment; the next
   read of that setting reads it again, so the answer is the file's, as a solo boot's is. */
extern "C" void host_setting_adventure_env_reread(void)
{
    g_adventure_env = -2;
}







extern "C" int host_settings_gen(void)
{
    return g_setgen;
}

/* See the header. The steady-state cost is one counter compare; the file's
   write time is asked for every 30th call, and the file is read only when
   that time (or the size) moved. A read that fails or parses as garbage
   changes NOTHING -- the next poll simply tries again -- so a torn write
   can delay a change but never invent one. */
extern "C" int host_settings_poll(void)
{
#ifdef _WIN32
    static int tick;
    /* SETLIVE1: a debounced save waiting out its half second goes first, and
       a set made through the API says 1 on the very next call so the caller
       pushes the volume and the mouse capture without the 30-frame wait. */
    host_settings_save_tick();
    if (g_poll_flag) { g_poll_flag = 0; return 1; }
    if (++tick < 30) return 0;
    /* A save is still queued: the file is behind memory right now, and a
       reload would put a half-dragged slider back where it started. */
    if (host_settings_save_pending()) return 0;
    tick = 0;
    load_once();

    char path[1024];
    if (!find_settings(path, sizeof path)) return 0;
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fa)) return 0;
    const unsigned long long t =
        ((unsigned long long)fa.ftLastWriteTime.dwHighDateTime << 32) |
        fa.ftLastWriteTime.dwLowDateTime;
    const unsigned long long sz =
        ((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
    if (t == g_watch_time && sz == g_watch_size) return 0;
    g_watch_time = t;
    g_watch_size = sz;

    char *text = slurp(path);
    if (!text) return 0;
    int changed = 0;
    if (looks_like_json_object(text)) changed = reload_live(text);
    free(text);
    if (changed) ++g_setgen;
    return changed;
#else
    return 0;
#endif
}

/* Take the three values and PERSIST them, so a choice made in the debug menu
   is still there after a restart -- which is the whole reason this is a
   settings key and not a runtime toggle. Returns 1 when the file on disk now
   says so.

   The in-memory values move whether or not the write lands: a player who
   changed the mode is owed the change for the rest of this run even if the
   folder is read-only, and the failure is on stderr rather than in their
   face. Every key this program did not write is carried across untouched
   (see json_set). */
/* run mg16 lane MP2: A KNOWN, DELIBERATE LIMITATION WITH TWO INSTANCES.
   This is a READ-MODIFY-WRITE over a file both copies of the game share: it
   reloads, edits three keys and rewrites the WHOLE document. So if P1 changes
   its volume and then P2 saves anything, P2 rewrites the file from the state it
   loaded and P1's change is gone -- LAST WRITER WINS, silently.
   That is left as it is on purpose. Giving each instance its own settings.json
   would mean the launcher's own keys stop being shared, which is the wrong
   trade for a file whose whole job is to carry launcher settings into the game.
   The TEMP-FILE interleave that used to sit underneath this is a different
   thing and IS fixed, in write_text above.
   hal/instance_tag.h's survey carries the same note for a reader who arrives
   from the other direction. */
/* The shared write: settings.json reloaded, `n` keys set in order, the whole
   document published through write_text. Every key this program did not
   write is carried across untouched (see json_set). Says why on stderr when
   the write fails; the in-memory values are the caller's to have moved
   already. */
/* SETFIX1 (L1): the settings file's cross-process lock. The launcher and the
   game both read-modify-write the same settings.json; without a lock a launcher
   save landing between the game's read and its rename is overwritten (and the
   game then re-reads the stale value). A NAMED mutex, shared with the launcher,
   held around the whole read-modify-write-rename. The name is derived from the
   file's full path so two installs do not share it:

     "Local\\SM64DSPort.settings." + 8 hex digits of FNV-1a (32 bit) over the
     UTF-16 code units of the full path, A-Z lowered and '/' turned into '\\'.

   The launcher (Settings.cs) computes the same name. A short timeout (500 ms)
   then proceed: a stuck holder can never hang a frame; the game takes the
   lock only when it saves (the debounced tick), never per frame. */
#ifdef _WIN32
namespace {
void settings_lock_name(const char *path, char *out, size_t cap)
{
    wchar_t wide[1100], full[1100];
    unsigned h = 2166136261u;
    full[0] = 0;
    if (MultiByteToWideChar(CP_ACP, 0, path, -1, wide, 1100) &&
        GetFullPathNameW(wide, 1100, full, 0)) {
        for (const wchar_t *c = full; *c; ++c) {
            unsigned ch = *c;
            if (ch >= 'A' && ch <= 'Z') ch += 32;
            if (ch == '/') ch = '\\';
            h = (h ^ ch) * 16777619u;
        }
    }
    snprintf(out, cap, "Local\\SM64DSPort.settings.%08x", h);
}
struct SettingsLock {
    HANDLE m;
    explicit SettingsLock(const char *path) : m(0)
    {
        char name[96];
        settings_lock_name(path, name, sizeof name);
        m = CreateMutexA(0, FALSE, name);
        if (m && WaitForSingleObject(m, 500) == WAIT_TIMEOUT) {
            CloseHandle(m);   /* proceed without it: never hang a frame */
            m = 0;
        }
    }
    ~SettingsLock()
    {
        if (m) { ReleaseMutex(m); CloseHandle(m); }
    }
    int held() const { return m != 0; }
};
}  /* namespace */
#else
namespace {
struct SettingsLock { explicit SettingsLock(const char *) {} int held() const { return 0; } };
}
#endif
/* test seam for the smoke program: the lock name for a path */
extern "C" void host_settings_lock_name(const char *path, char *out, int cap)
{
#ifdef _WIN32
    settings_lock_name(path, out, (size_t)cap);
#else
    snprintf(out, (size_t)cap, "%s", path);
#endif
}


namespace {
int save_keys(const char *const *keys, const char *const *vals, int n,
              const char *what)
{
    char path[1024];
    if (!settings_write_path(path, sizeof path)) return 0;

    SettingsLock lock(path);   /* held to the end: read, edit, rename, last-good */
    char *doc = slurp(path);
    if (doc && !looks_like_json_object(doc)) {
        /* a file that will not parse is one the loader is already ignoring,
           so replacing it loses nothing a reader was going to honour. SETFIX1:
           but keep what it held once, beside it, so nothing is lost for good. */
#ifdef _WIN32
        {
            char bad[1100];
            snprintf(bad, sizeof bad, "%s.bad", path);
            CopyFileA(path, bad, TRUE);   /* keeps an earlier .bad */
        }
#endif
        free(doc);
        doc = 0;
    }
    if (!doc) {
        doc = (char *)malloc(4);
        if (!doc) return 0;
        memcpy(doc, "{\n}", 4);
    }
    int ok = 1;
    for (int i = 0; i < n && ok; ++i) {
        char *next = json_set(doc, keys[i], vals[i]);
        free(doc);
        doc = next;
        if (!doc) ok = 0;
    }
    if (ok) ok = write_text(path, doc);
#ifdef _WIN32
    if (ok) {   /* SETFIX1 (L2): the last good file, for the launcher's cold start */
        char lg[1100];
        snprintf(lg, sizeof lg, "%s.lastgood", path);
        CopyFileA(path, lg, FALSE);
    }
#endif
    free(doc);
    if (!ok)
        fprintf(stderr, "[settings] could not write %s -- the %s is set for "
                        "this run only\n", path, what);
    return ok;
}
}  /* namespace */

/* ---- SETLIVE1: the live setters and the debounced save -------------------
   Every set* below moves the working value NOW (the frame loop sees it on its
   next counter compare) and queues the settings.json key. A queued key is
   written at once when the file has been quiet for half a second, otherwise
   by host_settings_save_tick once the half second is up, so a slider dragged
   for two seconds writes about four times, with the last value always
   landing. The last write is also forced by host_settings_save_flush (the
   page calls it on mouse-up and at close). Values are written typed: bools
   as true/false, numbers as numbers, the two mode words quoted lowercase. */
namespace {
struct Pend { char key[40]; char val[40]; };
Pend g_pend[24];
int g_npend;
int g_session_only;   /* host_settings_session_only: a dev knob's picks never reach settings.json */
unsigned long long g_last_write_ms;

unsigned long long now_ms(void)
{
#ifdef _WIN32
    return GetTickCount64();
#else
    return 0;
#endif
}

int pending_flush(void)
{
    if (g_session_only) { g_npend = 0; return 1; }
    if (!g_npend) return 1;
    const char *keys[24];
    const char *vals[24];
    for (int i = 0; i < g_npend; ++i) { keys[i] = g_pend[i].key; vals[i] = g_pend[i].val; }
    const int ok = save_keys(keys, vals, g_npend, "live settings");
    /* SETFIX1 (G2): a failed write keeps the queue, retried on the next tick */
    if (ok) g_npend = 0;
    g_last_write_ms = now_ms();
    return ok;
}

void queue_save(const char *key, const char *val)
{
    int i;
    for (i = 0; i < g_npend; ++i)
        if (strcmp(g_pend[i].key, key) == 0) break;
    if (i == g_npend) {
        if (g_npend >= 24 && !pending_flush()) {
            /* still full after a failed write: drop the oldest */
            memmove(&g_pend[0], &g_pend[1], sizeof(Pend) * (24 - 1));
            --g_npend;
        }
        i = g_npend++;
    }
    snprintf(g_pend[i].key, sizeof g_pend[i].key, "%s", key);
    snprintf(g_pend[i].val, sizeof g_pend[i].val, "%s", val);
    if (now_ms() - g_last_write_ms >= 500) pending_flush();
}
void queue_int(const char *key, int v)
{
    char b[24]; snprintf(b, sizeof b, "%d", v); queue_save(key, b);
}
void queue_bool(const char *key, int v) { queue_save(key, v ? "true" : "false"); }
void queue_word(const char *key, const char *w)
{
    char b[32]; snprintf(b, sizeof b, "\"%s\"", w); queue_save(key, b);
}
}  /* namespace */

extern "C" void host_settings_save_tick(void)
{
    if (g_npend && now_ms() - g_last_write_ms >= 500) pending_flush();
}
extern "C" int host_settings_save_pending(void) { return g_npend; }
extern "C" int host_settings_save_flush(void) { return pending_flush(); }
extern "C" void host_settings_session_only(void) { g_session_only = 1; g_npend = 0; }
extern "C" int host_settings_ctl_gen(void) { return g_ctl_gen; }
extern "C" int host_settings_cam_gen(void) { return g_cam_gen; }

/* SETFIX1 (G4): the pause must stay reachable, so Start cannot be unbound on
   the keyboard and the pad at the same time. The reason is kept for the page. */
namespace { char g_refusal[96]; }
extern "C" const char *host_setting_refusal(void) { return g_refusal; }

extern "C" int host_setting_set_key(int action, int vk)
{
    load_once();
    g_refusal[0] = 0;
    if (action < 0 || action >= HOST_KEY_COUNT || vk < 0 || vk > 0xff) return 0;
    if (action == HOST_KEY_START && vk == 0 && g_pad[HOST_PAD_START] == 0) {
        snprintf(g_refusal, sizeof g_refusal, "%s",
                 "Start needs a key or a button, or the pause cannot open");
        return 0;
    }
    if (g_key[action] == vk) return 1;
    g_key[action] = vk;
    for (int j = 0; j < 4; ++j)   /* LLLSHELL1: a camera turn / tilt key the player set is his; then re-derive the rest */
        if (CAM_YIELD_KEY[j] == action) g_cam_key_named |= 1u << action;
    cam_turn_yield();
    if (action == HOST_KEY_RUN) {
        g_run_key = vk;
        queue_int("RunButtonKey", vk);
    }
    queue_int(KEY_BIND[action].name, vk);
    ++g_ctl_gen;
    return 1;
}

extern "C" int host_setting_set_mouse(int action, int vk)
{
    load_once();
    g_refusal[0] = 0;
    if (action < 0 || action >= HOST_MOUSE_COUNT || !mouse_vk_ok_for(action, vk)) return 0;
    if (action == HOST_MOUSE_ATTACK && !g_mouse_attack_named) {   /* CLICK1: chosen on the page, so it is the player's from now on */
        g_mouse_attack_named = 1;
        g_mouse[action] = vk;
        queue_int(MOUSE_BIND[action].name, vk);
        ++g_ctl_gen;
        return 1;
    }
    if (g_mouse[action] == vk) return 1;
    g_mouse[action] = vk;
    queue_int(MOUSE_BIND[action].name, vk);
    mouse_attack_default();     /* CLICK1: left moved onto (or off) another action: the unchosen Attack default follows */
    ++g_ctl_gen;
    return 1;
}

extern "C" int host_setting_set_camera_stick_zoom(int on)
{
    load_once();
    on = on ? 1 : 0;
    if (g_camera_stick_zoom != on) { g_camera_stick_zoom = on; ++g_ctl_gen; }
    queue_bool("CameraStickZoom", on);
    return 1;
}

extern "C" int host_setting_set_pad(int action, int mask)
{
    load_once();
    g_refusal[0] = 0;
    if (action < 0 || action >= HOST_PAD_COUNT || mask < 0 || mask > HOST_PAD_MASK_MAX) return 0;
    if (action == HOST_PAD_START && mask == 0 && g_key[HOST_KEY_START] == 0) {
        snprintf(g_refusal, sizeof g_refusal, "%s",
                 "Start needs a key or a button, or the pause cannot open");
        return 0;
    }
    if (g_pad[action] == mask) return 1;
    g_pad[action] = mask;
    for (int j = 0; j < 2; ++j)   /* LLLSHELL1: as host_setting_set_key */
        if (CAM_YIELD_PAD[j] == action) g_cam_pad_named |= 1u << action;
    cam_turn_yield();
    if (action == HOST_PAD_RUN) {
        g_run_pad = mask;
        queue_int("RunButtonPad", mask);
    }
    queue_int(PAD_BIND[action].name, mask);
    ++g_ctl_gen;
    return 1;
}

extern "C" int host_setting_set_run_mode(int mode)
{
    load_once();
    if (mode < 0 || mode > 2) return 0;
    if (g_run_mode != mode) { g_run_mode = mode; ++g_ctl_gen; }
    queue_word("RunMode", RUN_MODE_KEY[mode]);
    return 1;
}

extern "C" int host_setting_set_camera_mode(int mode)
{
    load_once();
    if (mode < 0 || mode > 3) return 0;
    /* always bumps: the page's pick wins over an F1 cycle done in play */
    g_camera_mode = mode == 3 ? 2 : mode;
    g_camera_hybrid = mode == 3 ? 1 : 0;   /* HYBRIDCAM1: a pick is a pick, so a later "ds" from the launcher stays plain DS */
    ++g_ctl_gen;
    ++g_cam_gen;
    queue_word("CameraMode", CAMERA_MODE_KEY[g_camera_mode]);
    queue_bool("CameraHybrid", g_camera_hybrid);
    return 1;
}

extern "C" int host_setting_set_swap_camera_turn(int on)
{
    load_once();
    on = on ? 1 : 0;
    if (g_swap_camera_turn != on) { g_swap_camera_turn = on; ++g_ctl_gen; }
    queue_bool("SwapCameraTurnDirection", on);
    return 1;
}

extern "C" int host_setting_set_invert_camera_y(int on)
{
    load_once();
    on = on ? 1 : 0;
    if (g_invert_camera_y != on) { g_invert_camera_y = on; ++g_ctl_gen; }
    queue_bool("InvertCameraY", on);
    return 1;
}

extern "C" int host_setting_set_stick_deadzone(int pct)
{
    load_once();
    if (pct < 0 || pct > 50) return 0;
    const int raw = stick_dead_from_pct(pct);
    if (g_stick_dead != raw) { g_stick_dead = raw; ++g_ctl_gen; }
    queue_int("StickDeadzone", pct);
    return 1;
}

extern "C" int host_setting_set_run_threshold(int pct)
{
    load_once();
    if (pct < 30 || pct > 100) return 0;
    const int v = (pct == RUN_THRESH_ROM_PCT) ? -1 : pct;
    if (g_run_thresh_pct != v) { g_run_thresh_pct = v; ++g_ctl_gen; }
    queue_int("RunThreshold", pct);
    return 1;
}

extern "C" int host_setting_set_camera_sensitivity(int pct)
{
    load_once();
    if (pct < 10 || pct > 400) return 0;
    if (g_camera_sens != pct) { g_camera_sens = pct; ++g_ctl_gen; }
    queue_int("CameraSensitivity", pct);
    return 1;
}

/* SKYCAM1 phase 2: the right stick's camera turn eases in (accel) and out (decel); 0..10, 0 = instant (1.0.6) */
extern "C" int host_setting_set_camera_stick_accel(int v)
{
    load_once();
    if (v < 0 || v > 10) return 0;
    if (g_cam_stick_accel != v) { g_cam_stick_accel = v; ++g_ctl_gen; }
    queue_int("CameraStickAccel", v);
    return 1;
}
extern "C" int host_setting_set_camera_stick_decel(int v)
{
    load_once();
    if (v < 0 || v > 10) return 0;
    if (g_cam_stick_decel != v) { g_cam_stick_decel = v; ++g_ctl_gen; }
    queue_int("CameraStickDecel", v);
    return 1;
}
extern "C" int host_setting_camera_stick_accel(void) { load_once(); return g_cam_stick_accel; }
extern "C" int host_setting_camera_stick_decel(void) { load_once(); return g_cam_stick_decel; }

extern "C" int host_setting_set_mouse_sensitivity(int pct)
{
    load_once();
    if (pct < 10 || pct > 400) return 0;
    if (g_mouse_sens != pct) { g_mouse_sens = pct; ++g_ctl_gen; }
    queue_int("MouseSensitivity", pct);
    return 1;
}

extern "C" int host_setting_set_mouse_capture(int on)
{
    load_once();
    on = on ? 1 : 0;
    g_mouse_capture = on;
    g_poll_flag = 1;
    queue_bool("MouseCapture", on);
    return 1;
}

extern "C" int host_setting_set_draw_distance(int pct)
{
    load_once();
    if (pct < 100 || pct > 10000) return 0;
    g_draw_dist = pct;
    queue_int("DrawDistance", pct);
    return 1;
}

extern "C" int host_setting_set_volume(int pct)
{
    load_once();
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    g_volume = pct;
    g_poll_flag = 1;
    queue_int("Volume", pct);
    return 1;
}



/* KeepLives, plus SM64DS_KEEP_LIVES (0 forces it off, any other value on) for proof runs. */
extern "C" int host_setting_keep_lives(void)
{
    static int env = -2;
    if (env == -2) {
        const char *e = getenv("SM64DS_KEEP_LIVES");
        env = e ? ((e[0] == '0' && e[1] == 0) ? 0 : 1) : -1;
    }
    if (env >= 0) return env;
    load_once();
    return g_keep_lives;
}

/* FixOriginalBugs, plus SM64DS_FIX_ORIGINAL_BUGS (0 forces it off, any other value on) for proof runs and for turning it off by env. */
extern "C" int host_setting_fix_original_bugs(void)
{
    static int env = -2;
    if (env == -2) {
        const char *e = getenv("SM64DS_FIX_ORIGINAL_BUGS");
        env = (e && *e) ? ((e[0] == '0' && e[1] == 0) ? 0 : 1) : -1;
    }
    if (env >= 0) return env;
    load_once();
    return g_fix_original_bugs;
}

/* WideDrawingPage, plus SM64DS_WIDE_DRAWING (0 forces it off, any other value on) for proof runs. */
extern "C" int host_setting_wide_drawing(void)
{
    static int env = -2;
    if (env == -2) {
        const char *e = getenv("SM64DS_WIDE_DRAWING");
        env = (e && *e) ? ((e[0] == '0' && e[1] == 0) ? 0 : 1) : -1;
    }
    if (env >= 0) return env;
    load_once();
    return g_wide_drawing;
}

extern "C" int host_setting_set_wide_drawing(int on)
{
    load_once();
    on = on ? 1 : 0;
    g_wide_drawing = on;
    g_poll_flag = 1;
    return 1;
}

extern "C" int host_setting_set_fix_original_bugs(int on)
{
    load_once();
    on = on ? 1 : 0;
    g_fix_original_bugs = on;
    g_poll_flag = 1;
    queue_bool("FixOriginalBugs", on);
    return 1;
}

/* CameraCollision, plus SM64DS_CAMERA_COLLISION (0 forces it off, any other value on) for proof runs. */
extern "C" int host_setting_camera_collision(void)
{
    static int env = -2;
    if (env == -2) {
        const char *e = getenv("SM64DS_CAMERA_COLLISION");
        env = (e && *e) ? ((e[0] == '0' && e[1] == 0) ? 0 : 1) : -1;
    }
    if (env >= 0) return env;
    load_once();
    return g_camera_collision;
}

extern "C" int host_setting_set_camera_collision(int on)
{
    load_once();
    on = on ? 1 : 0;
    g_camera_collision = on;
    g_poll_flag = 1;
    queue_bool("CameraCollision", on);
    return 1;
}

/* PeachJingle, plus SM64DS_PEACH_JINGLE (0 forces it off, any other value on) for proof runs. */
extern "C" int host_setting_peach_jingle(void)
{
    static int env = -2;
    if (env == -2) {
        const char *e = getenv("SM64DS_PEACH_JINGLE");
        env = (e && *e) ? ((e[0] == '0' && e[1] == 0) ? 0 : 1) : -1;
    }
    if (env >= 0) return env;
    load_once();
    return g_peach_jingle;
}

extern "C" int host_setting_set_peach_jingle(int on)
{
    load_once();
    on = on ? 1 : 0;
    g_peach_jingle = on;
    g_poll_flag = 1;
    queue_bool("PeachJingle", on);
    return 1;
}


extern "C" int host_setting_set_keep_lives(int on)
{
    load_once();
    on = on ? 1 : 0;
    g_keep_lives = on;
    g_poll_flag = 1;
    queue_bool("KeepLives", on);
    return 1;
}

extern "C" const char *host_setting_launcher_nick(void)
{
    load_once();
    return g_launcher_nick;
}
namespace {
}  // namespace







/* PAUSE3: the screen gap's setters (see host_settings.h). The same variables reload_live writes from the file. */
extern "C" int host_setting_set_minigame_gap(int on)
{
    load_once();
    on = on ? 1 : 0;
    if (g_gap_on != on) { g_gap_on = on; ++g_setgen; }
    queue_bool("MinigameGap", on);
    return 1;
}

extern "C" int host_setting_gap_look(void)
{
    load_once();
    return g_gap_peek ? 2 : (g_gap_fill == 0 ? 0 : 1);
}

extern "C" int host_setting_set_gap_look(int look)
{
    load_once();
    if (look < 0 || look > 2) return 0;
    const int peek = look == 2 ? 1 : 0;
    int fill = g_gap_fill;
    if (look == 0) fill = 0;
    else if (look == 1 && fill == 0) fill = 1;
    if (peek != g_gap_peek || fill != g_gap_fill) { g_gap_peek = peek; g_gap_fill = fill; ++g_setgen; }
    queue_bool("GapPeek", peek);
    queue_word("GapFillMode", fill == 0 ? "solid" : (fill == 2 ? "custom" : "ambient"));
    return 1;
}

extern "C" int host_setting_set_gap_color(unsigned rgb)
{
    load_once();
    const unsigned c = 0xFF000000u | (rgb & 0xffffffu);
    if (g_gap_color != c) { g_gap_color = c; ++g_setgen; }
    char b[24];
    snprintf(b, sizeof b, "\"#%06X\"", rgb & 0xffffffu);
    queue_save("GapColor", b);
    return 1;
}

extern "C" int host_setting_stick_deadzone_raw(void)
{
    load_once();
    return g_stick_dead;
}
extern "C" int host_setting_stick_deadzone_pct(void)
{
    load_once();
    return stick_dead_to_pct(g_stick_dead);
}
/* the digital walk threshold the left stick uses beside the d-pad: 12000 at
   the default dead zone, scaled with the dead zone so the two stay in the
   same proportion */
extern "C" int host_setting_stick_digital(void)
{
    load_once();
    if (g_stick_dead == STICK_DEAD_DEFAULT) return 12000;
    const long long v = (long long)12000 * g_stick_dead / STICK_DEAD_DEFAULT;
    /* SETFIX1: never below 4000 of 32767, so stick drift cannot walk Mario in
       button / auto run mode at Deadzone 0 (the analog injection may still
       use the raw zero dead zone) */
    return v < 4000 ? 4000 : v > 30000 ? 30000 : (int)v;
}
extern "C" int host_setting_run_threshold_pct(void)
{
    load_once();
    return g_run_thresh_pct < 0 ? RUN_THRESH_ROM_PCT : g_run_thresh_pct;
}
extern "C" int host_setting_run_threshold_is_default(void)
{
    load_once();
    return g_run_thresh_pct < 0;
}
/* magnitude (0..0x1000 of usable travel) -> the magnitude the ROM is given.
   Default: unchanged, exactly. Otherwise the player's point T maps to the
   ROM's start-running point 0xe47: 0..T -> 0..0xe46 linear (walk speed is
   linear in the magnitude, so walking is stretched or squeezed to fill the
   same walk range), T..1 -> 0xe47..0x1000 linear. */
extern "C" int host_setting_run_remap(int mag)
{
    load_once();
    if (mag <= 0) return 0;
    if (mag > 0x1000) mag = 0x1000;
    if (g_run_thresh_pct < 0) return mag;
    const int tm = (int)((long long)g_run_thresh_pct * 4096 / 100);
    if (tm >= 4096) return (int)((long long)mag * 0xe46 / 4096);
    if (mag < tm) return (int)((long long)mag * 0xe47 / tm);
    return 0xe47 + (int)((long long)(mag - tm) * (0x1000 - 0xe47) / (4096 - tm));
}
/* the multiplayer fold's run point as a fraction of usable travel: 0.87 at
   the default (the ROM split), the player's point otherwise */
extern "C" double host_setting_run_norm(void)
{
    load_once();
    return g_run_thresh_pct < 0 ? 0.87 : g_run_thresh_pct / 100.0;
}
extern "C" int host_setting_invert_camera_y(void)
{
    load_once();
    return g_invert_camera_y;
}
extern "C" int host_setting_camera_sensitivity(void)
{
    load_once();
    return g_camera_sens;
}
extern "C" int host_setting_mouse_sensitivity(void)
{
    load_once();
    return g_mouse_sens;
}

extern "C" int host_setting_save_run(int mode, int key, int pad)
{
    load_once();
    if (mode < 0 || mode > 2) mode = 0;
    if (key < 0 || key > 0xff) key = 0;
    if (pad < 0 || pad > HOST_PAD_MASK_MAX) pad = 0;
    g_run_mode = mode;
    g_run_key = key;
    g_run_pad = pad;
    g_key[11] = key;
    g_pad[3] = pad;

    /* BOTH spellings of each binding, so a launcher that reads the old name
       and one that reads the new name both see the choice, and the loader's
       "KeyRun wins when present" rule cannot resurrect a stale old value. */
    char vmode[24], vkey[24], vpad[24];
    snprintf(vmode, sizeof vmode, "\"%s\"", RUN_MODE_KEY[mode]);
    snprintf(vkey, sizeof vkey, "%d", key);
    snprintf(vpad, sizeof vpad, "%d", pad);
    /* SETFIX1 (G3): through the queue, so an older page value still pending
       cannot be flushed over this one afterwards */
    queue_save("RunMode", vmode);
    queue_save("RunButtonKey", vkey);
    queue_save("KeyRun", vkey);
    queue_save("RunButtonPad", vpad);
    queue_save("PadRun", vpad);
    return pending_flush();   /* a menu row persists on the spot, behind anything queued */
}

/* The camera row's twin of the above: the live mode moves whether or not the
   write lands, and the file carries it to the next boot. */
extern "C" int host_setting_save_camera_mode(int mode)
{
    load_once();
    if (mode < 0 || mode > 3) mode = 0;
    g_camera_mode = mode == 3 ? 2 : mode;
    g_camera_hybrid = mode == 3 ? 1 : 0;   /* HYBRIDCAM1 */
    char v[24];
    snprintf(v, sizeof v, "\"%s\"", CAMERA_MODE_KEY[g_camera_mode]);
    queue_save("CameraMode", v);   /* SETFIX1 (G3): through the queue, then on the spot */
    queue_save("CameraHybrid", g_camera_hybrid ? "true" : "false");
    return pending_flush();
}

/* ---- PadLayouts -----------------------------------------------------------
   See the header. The table is what load_once parsed plus whatever the learn
   flow saved this run. */
extern "C" void host_pad_layout_default(HostPadLayout *out)
{
    padlayout_default(out);
}

extern "C" int host_setting_pad_layout(int vid, int pid, HostPadLayout *out)
{
    load_once();
    for (int i = 0; i < g_padlayout_n; ++i)
        if (g_padlayouts[i].vid == vid && g_padlayouts[i].pid == pid) {
            if (out) *out = g_padlayouts[i];
            return 1;
        }
    return 0;
}

extern "C" int host_setting_pad_layout_count(void)
{
    load_once();
    return g_padlayout_n;
}

extern "C" int host_setting_pad_layout_at(int i, HostPadLayout *out)
{
    load_once();
    if (i < 0 || i >= g_padlayout_n) return 0;
    if (out) *out = g_padlayouts[i];
    return 1;
}

extern "C" int host_setting_save_pad_layout(const HostPadLayout *layout)
{
    load_once();
    if (!layout || layout->vid < 1 || layout->vid > 0xffff ||
        layout->pid < 1 || layout->pid > 0xffff)
        return 0;
    int slot = g_padlayout_n;
    for (int i = 0; i < g_padlayout_n; ++i)
        if (g_padlayouts[i].vid == layout->vid && g_padlayouts[i].pid == layout->pid)
            slot = i;
    if (slot >= HOST_PAD_LAYOUT_MAX) {
        fprintf(stderr, "[settings] PadLayouts is full (%d pads); the layout "
                        "is set for this run only\n", HOST_PAD_LAYOUT_MAX);
        return 0;
    }
    /* clamp through the same table the parser uses, so what the file gets
       is what a reader will accept */
    HostPadLayout o = *layout;
    HostPadLayout d;
    padlayout_default(&d);
    for (int f = 0; f < 22; ++f) {
        const int v = o.*PAD_FIELDS[f].f;
        const int is_sign = PAD_FIELDS[f].lo == -1 && PAD_FIELDS[f].hi == 1;
        if (v < PAD_FIELDS[f].lo || v > PAD_FIELDS[f].hi || (is_sign && v == 0))
            o.*PAD_FIELDS[f].f = d.*PAD_FIELDS[f].f;
    }
    o.name[sizeof o.name - 1] = '\0';
    g_padlayouts[slot] = o;
    if (slot == g_padlayout_n) ++g_padlayout_n;

    char *arr = padlayouts_text();
    if (!arr) return 0;
    const char *const keys[1] = { "PadLayouts" };
    const char *const vals[1] = { arr };
    const int ok = save_keys(keys, vals, 1, "pad layout");
    free(arr);
    return ok;
}

/* FrameRate: the presentation rate in pictures a second, 0 for native. Same
   shape as host_setting_aspect above -- an environment override in front of
   load_once and the stored value -- with the same grammar on both channels,
   the word "display" included.

     SM64DS_FRAME_RATE   the rate directly, through the same sanitiser the file
                         goes through: 0 or anything under 60 is native, 144 is
                         144, 1000 clamps to 240, "display" is the primary
                         display's refresh rate, junk reads as 0.

   Read once at boot -- the pacer latches the answer on its first turn and the
   presentation clock cannot change shape mid-run. */
extern "C" int host_setting_frame_rate(void)
{
    static int env_read = 0;
    static int env = -1;             /* <0 means "the environment said nothing" */
    if (!env_read) {
        env_read = 1;
        const char *e = getenv("SM64DS_FRAME_RATE");
        if (e && *e) {
            if (strlen(e) == 7 && ieq(e, "display", 7)) {
                env = frame_rate_sanitise(display_refresh_hz());
            } else {
                char *end = 0;
                const long v = strtol(e, &end, 10);
                /* an unparseable override is still an override: it says
                   "native", the same answer an unparseable file value gives */
                env = (end != e) ? frame_rate_sanitise((int)v) : 0;
            }
        }
    }
    if (env >= 0 && !g_frame_rate_touched) return env;
    load_once();
    return g_frame_rate;
}

/* run hunt8, lane FPSLIVE1: steps each time the answer above changes while
   the game runs, so the pacer can keep its cached copy and re-read it on a
   step only. */
extern "C" int host_setting_frame_rate_gen(void)
{
    return g_frame_rate_gen;
}

/* run hunt8, lane FPSLIVE1: the settings page's call. The rate takes effect
   at the pacer's next turn and is written to settings.json for the next boot.
   rate is the key's own grammar: 0 native, 60..240 pictures a second; anything
   else is brought into the band by the same sanitiser the file goes through
   (under 60 reads as native, above 240 as 240). Returns 1 when the file on
   disk now says so; the running game has the new rate either way. */
extern "C" int host_setting_save_frame_rate(int rate)
{
    load_once();
    const int before = host_setting_frame_rate();
    const int n = frame_rate_sanitise(rate);
    g_frame_rate = n;
    g_frame_rate_touched = 1;
    if (n != before) ++g_frame_rate_gen;
    char v[24];
    snprintf(v, sizeof v, "%d", n);
    const char *const keys[1] = { "FrameRate" };
    const char *const vals[1] = { v };
    return save_keys(keys, vals, 1, "frame rate");
}

/* ---- THE THREE PICTURE-QUALITY KEYS' ACCESSORS (run hd1) ----------------
   Every one is host_setting_frame_rate's shape exactly: an environment
   override read once, in front of load_once and the stored value, through
   the same sanitiser the file goes through, so a proof run can pin any of
   them off ONE build without editing a player's settings file and the two
   channels cannot disagree about what a value means. See the header for each
   key's contract. */

/* RenderScale: host rows per DS row, 0 for the multiplier the port picks
   today. SM64DS_RENDER_SCALE overrides; junk reads as 0, which is the answer
   an unparseable file value gives. */
extern "C" int host_setting_render_scale(void)
{
    static int env_read = 0;
    static int env = -1;             /* <0 means "the environment said nothing" */
    if (!env_read) {
        env_read = 1;
        const char *e = getenv("SM64DS_RENDER_SCALE");
        if (e && *e) {
            char *end = 0;
            const long v = strtol(e, &end, 10);
            env = (end != e) ? render_scale_sanitise((int)v) : 0;
        }
    }
    if (env >= 0) return env;
    load_once();
    return g_render_scale;
}

/* RESSCALE1: the in-game Settings row. Boot-latched like the launcher's row: the setter writes the key for the next boot and
   leaves the running picture (ntr::render_scale, latched at boot) and host_setting_render_scale alone, so the logs that report
   this run's scale stay true. The page reads the saved value back so the row shows what was picked. */
static int g_render_scale_saved = -1;   /* <0: nothing picked on the page this run */
extern "C" int host_setting_render_scale_saved(void)
{
    return g_render_scale_saved >= 0 ? g_render_scale_saved : host_setting_render_scale();
}
extern "C" int host_setting_set_render_scale(int n)
{
    load_once();
    n = render_scale_sanitise(n);
    g_render_scale_saved = n;
    queue_int("RenderScale", n);
    return 1;
}

/* ---- THE IMPROVED MINIMAP'S TWO GETTERS ------------------------------------

   THE PIN IS HERE, IN THE GETTER, rather than in the frame loop, and that is
   the whole reason this pair is not two more lines of boilerplate. The option
   changes what the bottom-screen panel DRAWS. Every recorded baseline in this
   tree -- the five level-1 capture hashes, hd1's six key-absent captures, the
   opening gate, every sweep row -- is a picture taken on one of exactly two
   routes: a window selftest (SM64DS_WINDOW_SELFTEST) or a scene run
   (SM64DS_SCENE_FRAMES). A feature that moved those hashes would look like a
   hundred regressions and be none of them. Both frame loops and every proof
   tool in port/tools go through this one function, so pinning it once here
   covers all of them, and tests/walk_window.cpp needs no line of its own.

   THE ENVIRONMENT STILL DISPOSES, in both directions and ahead of the pin: a
   run that means to look at the improved map sets SM64DS_IMPROVED_MINIMAP=1
   and gets it, selftest or not, which is how this lane's own captures were
   taken. That is the same precedence SM64DS_DUAL_SCREEN has over the layout
   proposal in hal/sub_screen.cpp. */
extern "C" int host_setting_improved_minimap(void)
{
    static int env = -2;
    if (env == -2) {
        const char *e = getenv("SM64DS_IMPROVED_MINIMAP");
        env = e ? ((e[0] == 0 || (e[0] == '0' && e[1] == 0)) ? 0 : 1) : -1;
    }
    if (env >= 0) return env;
    if (getenv("SM64DS_WINDOW_SELFTEST") || getenv("SM64DS_SCENE_FRAMES"))
        return 0;
    load_once();
    return g_improved_minimap;
}

/* THE PIN IS A STARTING SIZE, NOT A LOCK ON THE PLAYER'S HAND.
 *
 * SM64DS_MINIMAP_SCALE still outranks the file and still holds for a whole run
 * that never touches the map, which is every scripted run there is: a sweep, a
 * capture, a proof and a gate all ask for a size and none of them has a mouse.
 * That is the reason the override exists and it does not change.
 *
 * What changes is what happens after a HAND has moved it. This getter used to
 * answer the pin ahead of g_minimap_scale unconditionally, and the drag's only
 * write is to g_minimap_scale -- so with the pin set the grab worked, the hold
 * worked, the release worked, and every one of them moved a number that
 * nothing read. The owner's own preview shortcut exports the pin, so on his
 * screen the yellow square took the press, printed that it had, printed a size
 * on release and never once resized the map. Both scripted drags that proved
 * this feature passed the size in settings.json instead, where the pin is not
 * in play, which is the whole of why they were green.
 *
 * So: the pin answers until a drag happens, and the drag answers afterwards.
 * One value, one hand, for the rest of the run. */
/* ---- THE SAVE MENU ON THE TOP SCREEN ---------------------------------------

   The same three-layer shape the improved minimap's getter has, and for the
   same three reasons.

   ABSENT IS ON. It is a mod the owner asked for and recommended on, so a
   settings file written before the key existed gets the composed picture.

   IT IS PINNED OFF ON EVERY COMPARATOR ROUTE. A window selftest and a scene
   sweep row are the two shapes every recorded baseline capture in this tree
   is taken in. This option changes what a frame LOOKS LIKE, so left free it
   would move recorded hashes on any route that reaches a level-clear screen,
   for a reason that has nothing to do with the code under test. Pinning it
   here, in the getter, covers both frame loops and every proof tool in
   port/tools at once.

   THE ENVIRONMENT STILL DISPOSES, in both directions and ahead of the pin: a
   run that means to look at the composed picture sets
   SM64DS_SAVE_MENU_ON_TOP=1 and gets it, selftest or not, which is how every
   capture of it was taken. */
extern "C" int host_setting_save_menu_on_top(void)
{
    static int env = -2;
    if (env == -2) {
        const char *e = getenv("SM64DS_SAVE_MENU_ON_TOP");
        env = e ? ((e[0] == 0 || (e[0] == '0' && e[1] == 0)) ? 0 : 1) : -1;
    }
    if (env >= 0) return env;
    if (getenv("SM64DS_WINDOW_SELFTEST") || getenv("SM64DS_SCENE_FRAMES"))
        return 0;
    load_once();
    return g_save_menu_on_top;
}



extern "C" double host_setting_minimap_scale_value(void)
{
    static int env_read = 0;
    static double env = -1.0;        /* <0 means "the environment said nothing" */
    if (!env_read) {
        env_read = 1;
        const char *e = getenv("SM64DS_MINIMAP_SCALE");
        if (e && *e) {
            char *end = 0;
            const double v = strtod(e, &end);
            env = (end != e) ? minimap_scale_sanitise(v, 0) : 1.0;
        }
    }
    if (env > 0.0 && !g_minimap_scale_dragged) return env;
    load_once();
    return g_minimap_scale;
}

/* The chosen size as the exact rational the drawing and the touch inverse
   share. ONE function, so a size can never mean two things in two files --
   which is the rule the corner panel's geometry was already keeping with its
   single integer divisor and has to keep now that the divisor is a fraction
   the player can drag. */
extern "C" void host_setting_minimap_scale_ratio(int *num, int *den)
{
    const int k = minimap_scale_k(host_setting_minimap_scale_value());
    if (num) *num = k;
    if (den) *den = 256;
}

/* THE DRAG'S TWO WRITES.
 *
 * _live moves the size for this run and nothing else: it is called on every
 * frame of a drag, and a settings file rewritten sixty times a second would
 * be a file the launcher is reading while it is half written.
 *
 * _save is the mouse-up: the same move, and then the number on disk, so the
 * size survives a restart and the launcher's picker opens on it. It goes
 * through save_keys like the debug menu's run and camera rows, which reloads
 * the document and carries every key this program did not write across
 * untouched. SM64DS_MINIMAP_SCALE keeps a run that never drags pinned exactly
 * where it asked to be -- which is every scripted run -- and stands aside for
 * a hand that has actually moved the map; the block above the getter is the
 * argument for that.
 *
 * BOTH WRITES GO THROUGH ONE FUNCTION, and that is not tidiness: the defect
 * this closes was a size written in one place and read from another, so a
 * second writer that forgot to say a hand had moved it would put the map
 * straight back to where the owner found it.
 *
 * %.6g is enough to print any number on the grid exactly (the grid is
 * thirty-seconds, and 4096/128 = 32 is the ceiling), so a value written here
 * reads back as the same value. */
namespace {
void minimap_scale_move_live(double s)
{
    load_once();
    g_minimap_scale = minimap_scale_sanitise(s, 0);
    g_minimap_scale_dragged = 1;
}
}

extern "C" void host_setting_minimap_scale_set_live(double s)
{
    minimap_scale_move_live(s);
}

extern "C" int host_setting_save_minimap_scale(double s)
{
    minimap_scale_move_live(s);
    char v[32];
    snprintf(v, sizeof v, "%.6g", g_minimap_scale);
    const char *const keys[1] = { "MinimapScale" };
    const char *const vals[1] = { v };
    return save_keys(keys, vals, 1, "minimap size");
}

extern "C" int host_setting_set_minimap_scale(double s)
{
    minimap_scale_move_live(s);
    char v[32];
    snprintf(v, sizeof v, "%.6g", g_minimap_scale);
    queue_save("MinimapScale", v);
    return 1;
}








/* SmoothModels: the subdivision level, 0 for the ROM's own geometry.
   SM64DS_SMOOTH_MODELS overrides; junk reads as 0. */
/* SmoothMotion: 1 on, 0 off. SM64DS_SMOOTH_MOTION overrides with the same
   grammar (any nonzero number is on; junk and 0 are off). */
extern "C" int host_setting_smooth_motion(void)
{
    static int env_read = 0;
    static int env = -1;             /* <0 means "the environment said nothing" */
    if (!env_read) {
        env_read = 1;
        const char *e = getenv("SM64DS_SMOOTH_MOTION");
        if (e && *e) {
            char *end = 0;
            const long v = strtol(e, &end, 10);
            env = (end != e && v != 0) ? 1 : 0;
        }
    }
    if (env >= 0) return env;
    load_once();
    return g_smooth_motion;
}

/* MinigameUpscale: 1 on, 0 off; the environment override is read by its one caller (hal/sub_screen.cpp mghr_policy). */
extern "C" int host_setting_minigame_upscale(void)
{
    load_once();
    return g_minigame_upscale;
}
extern "C" int host_setting_minigame_hires(void)
{
    load_once();
    return g_minigame_hires;
}
/* MGHIRES2: the settings page's two rows; the next picture's mghr_policy reads the new value, so both apply at once */
extern "C" int host_setting_set_minigame_hires(int on)
{
    load_once();
    on = on ? 1 : 0;
    g_minigame_hires = on;
    g_poll_flag = 1;
    queue_bool("MinigameHiRes", on);
    return 1;
}
extern "C" int host_setting_set_minigame_upscale(int on)
{
    load_once();
    on = on ? 1 : 0;
    g_minigame_upscale = on;
    g_poll_flag = 1;
    queue_bool("MinigameUpscale", on);
    return 1;
}

extern "C" int host_setting_smooth_models(void)
{
    static int env_read = 0;
    static int env = -1;             /* <0 means "the environment said nothing" */
    if (!env_read) {
        env_read = 1;
        const char *e = getenv("SM64DS_SMOOTH_MODELS");
        if (e && *e) {
            char *end = 0;
            const long v = strtol(e, &end, 10);
            env = (end != e) ? smooth_models_sanitise((int)v) : 0;
        }
    }
    if (env >= 0) return env;
    load_once();
    return g_smooth_models;
}

/* ---- run hd2's two accessors, host_setting_render_scale's shape exactly --
   an environment override read once, in front of load_once and the stored
   value, through the same sanitiser the file goes through, so a proof run can
   pin either off ONE build without editing a player's settings file. */

/* TextureFilter: 0 nearest, 1 bilinear, 2 trilinear.
   SM64DS_TEXTURE_FILTER overrides; junk reads as 0. */
extern "C" int host_setting_texture_filter(void)
{
    static int env_read = 0;
    static int env = -1;             /* <0 means "the environment said nothing" */
    if (!env_read) {
        env_read = 1;
        const char *e = getenv("SM64DS_TEXTURE_FILTER");
        if (e && *e) {
            char *end = 0;
            const long v = strtol(e, &end, 10);
            env = (end != e) ? texture_filter_sanitise((int)v) : 0;
        }
    }
    if (env >= 0) return env;
    load_once();
    return g_texture_filter;
}

/* AntiAliasing: 0 off, 1 edge smoothing.
   SM64DS_ANTI_ALIASING overrides; junk reads as 0. */
extern "C" int host_setting_anti_aliasing(void)
{
    static int env_read = 0;
    static int env = -1;
    if (!env_read) {
        env_read = 1;
        const char *e = getenv("SM64DS_ANTI_ALIASING");
        if (e && *e) {
            char *end = 0;
            const long v = strtol(e, &end, 10);
            env = (end != e) ? anti_aliasing_sanitise((int)v) : 0;
        }
    }
    if (env >= 0) return env;
    load_once();
    return g_anti_aliasing;
}

/* ---- run hd2 lane GPU1's three accessors, the same shape once more -------
   an environment override read once, in front of load_once and the stored
   value, through the same sanitiser the file goes through, so a proof run can
   pin any of them off ONE build without editing a player's settings file.

   SM64DS_PRESENT_FILTER_D3D rather than SM64DS_PRESENT_FILTER: that name has
   belonged to the ordinary Windows path's halftone knob since before this key
   existed (tests/walk_window.cpp reads it), and two settings answering to one
   variable is how a proof run comes to mean something it did not say. */

/* PresentBackend: 0 the ordinary Windows path, 1 Direct3D 11. */
extern "C" int host_setting_present_backend(void)
{
    static int env_read = 0;
    static int env = -1;             /* <0 means "the environment said nothing" */
    if (!env_read) {
        env_read = 1;
        const char *e = getenv("SM64DS_PRESENT_BACKEND");
        if (e && *e) {
            char *end = 0;
            const long v = strtol(e, &end, 10);
            env = (end != e) ? present_backend_sanitise((int)v) : 0;
        }
    }
    if (env >= 0) return env;
    load_once();
    return g_present_backend;
}

/* PresentFilter: 0 nearest, 1 smooth, 2 sharp. Only read when the backend
   is 1; this accessor answers either way and the backend is the one that
   decides whether the answer matters. */
extern "C" int host_setting_present_filter(void)
{
    static int env_read = 0;
    static int env = -1;
    if (!env_read) {
        env_read = 1;
        const char *e = getenv("SM64DS_PRESENT_FILTER_D3D");
        if (e && *e) {
            char *end = 0;
            const long v = strtol(e, &end, 10);
            env = (end != e) ? present_filter_sanitise((int)v) : 0;
        }
    }
    if (env >= 0) return env;
    load_once();
    return g_present_filter;
}

/* Renderer: 0 the software rasteriser, 1 Direct3D 11. INDEPENDENT of
   PresentBackend: either can be on without the other, and with both on the
   process still makes exactly one device (hal/gpu_device.cpp). */
extern "C" int host_setting_renderer(void)
{
    static int env_read = 0;
    static int env = -1;             /* <0 means "the environment said nothing" */
    if (!env_read) {
        env_read = 1;
        const char *e = getenv("SM64DS_RENDERER");
        if (e && *e) {
            char *end = 0;
            const long v = strtol(e, &end, 10);
            env = (end != e) ? renderer_sanitise((int)v) : 0;
        }
    }
    if (env >= 0) return env;
    load_once();
    return g_renderer;
}

/* VSync: 0 off, 1 on. Only read when the backend is 1, for the same reason. */
extern "C" int host_setting_vsync(void)
{
    static int env_read = 0;
    static int env = -1;
    if (!env_read) {
        env_read = 1;
        const char *e = getenv("SM64DS_VSYNC");
        if (e && *e) {
            char *end = 0;
            const long v = strtol(e, &end, 10);
            env = (end != e) ? vsync_sanitise((int)v) : 0;
        }
    }
    if (env >= 0) return env;
    load_once();
    return g_vsync;
}

/* SKIPINTRO1: SkipOpening. Read by hal/level_boot.cpp port_intro_suppressed beside the launcher's SM64DS_SKIP_INTRO. */
extern "C" int host_setting_skip_opening(void)
{
    load_once();
    return g_skip_opening;
}

extern "C" int host_setting_set_skip_opening(int on)
{
    load_once();
    on = on ? 1 : 0;
    g_skip_opening = on;
    g_poll_flag = 1;
    queue_bool("SkipOpening", on);
    return 1;
}




