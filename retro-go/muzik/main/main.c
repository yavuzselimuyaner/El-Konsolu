// MP3 player for Retro-Go. The launcher's "music" tab picks the first song, this app plays every
// song of that folder. The audio task owns the file and decoder, the main task owns the screen and
// the buttons, and they only talk through the `player` struct.

#include <rg_system.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#include "minimp3.h"

#define DEFAULT_MUSIC_PATH RG_BASE_PATH_ROMS "/music"
#define AUDIO_SAMPLE_RATE  44100
#define MAX_TRACKS         1000
#define MAX_LABEL          128
#define INPUT_BUFFER_SIZE  (16 * 1024)
#define READ_THRESHOLD     (8 * 1024)
#define VOLUME_STEP        5
#define RESTART_SECONDS    3

#define COLOR_BG           C_BLACK
#define COLOR_TEXT         C_WHITE
#define COLOR_DIM          C_GRAY
#define COLOR_ACCENT       C_DARK_ORANGE

static struct
{
    char *folder;
    char **files;
    char **titles;
    char **artists;
    char **albums;
    char **labels;
    int count;
} playlist;

static struct
{
    volatile int request;
    volatile int current;
    volatile bool paused;
    volatile bool shuffle;
    volatile bool quit;
    volatile bool stopped;
    volatile bool failed;
    volatile int elapsed;
    volatile int duration;
    volatile int progress;
    volatile int bitrate;
    volatile int samplerate;
} player = {.request = -1, .current = -1};

static rg_app_t *app;
static volatile bool need_redraw = true;

static void clean_trailing_spaces(char *str)
{
    if (!str) return;
    int len = strlen(str);
    while (len > 0 && (unsigned char)str[len - 1] <= ' ')
    {
        str[len - 1] = 0;
        len--;
    }
}

static bool is_valid_utf8(const char *str, size_t len)
{
    size_t i = 0;
    while (i < len && str[i])
    {
        unsigned char c = (unsigned char)str[i];
        if (c < 0x80)
        {
            i++;
        }
        else if ((c & 0xE0) == 0xC0)
        {
            if (i + 1 >= len || ((unsigned char)str[i + 1] & 0xC0) != 0x80)
                return false;
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            if (i + 2 >= len || ((unsigned char)str[i + 1] & 0xC0) != 0x80 || ((unsigned char)str[i + 2] & 0xC0) != 0x80)
                return false;
            i += 3;
        }
        else
        {
            return false;
        }
    }
    return true;
}

static void latin5_to_utf8(char *out, size_t out_sz, const char *in, size_t in_len)
{
    size_t o = 0;
    for (size_t i = 0; i < in_len && in[i] && o + 3 < out_sz; ++i)
    {
        unsigned char c = (unsigned char)in[i];
        if (c < 0x80)
        {
            out[o++] = c;
        }
        else if (c == 0xDE) { out[o++] = 0xC5; out[o++] = 0x9E; } // Ş
        else if (c == 0xFE) { out[o++] = 0xC5; out[o++] = 0x9F; } // ş
        else if (c == 0xD0) { out[o++] = 0xC4; out[o++] = 0x9E; } // Ğ
        else if (c == 0xF0) { out[o++] = 0xC4; out[o++] = 0x9F; } // ğ
        else if (c == 0xDD) { out[o++] = 0xC4; out[o++] = 0xB0; } // İ
        else if (c == 0xFD) { out[o++] = 0xC4; out[o++] = 0xB1; } // ı
        else if (c == 0xC7) { out[o++] = 0xC3; out[o++] = 0x87; } // Ç
        else if (c == 0xE7) { out[o++] = 0xC3; out[o++] = 0xA7; } // ç
        else if (c == 0xD6) { out[o++] = 0xC3; out[o++] = 0x96; } // Ö
        else if (c == 0xF6) { out[o++] = 0xC3; out[o++] = 0xB6; } // ö
        else if (c == 0xDC) { out[o++] = 0xC3; out[o++] = 0x9C; } // Ü
        else if (c == 0xFC) { out[o++] = 0xC3; out[o++] = 0xBC; } // ü
        else
        {
            out[o++] = 0xC0 | (c >> 6);
            out[o++] = 0x80 | (c & 0x3F);
        }
    }
    out[o] = 0;
}

static void parse_id3(const char *path, char *title, size_t title_sz, char *artist, size_t artist_sz, char *album, size_t album_sz)
{
    title[0] = 0;
    artist[0] = 0;
    album[0] = 0;

    FILE *fp = fopen(path, "rb");
    if (!fp)
        return;

    uint8_t h[10];
    if (fread(h, 1, 10, fp) == 10 && memcmp(h, "ID3", 3) == 0)
    {
        uint32_t tag_size = ((uint32_t)h[6] << 21) | ((uint32_t)h[7] << 14) | ((uint32_t)h[8] << 7) | h[9];
        uint32_t to_read = RG_MIN(tag_size, 16384);
        uint8_t *buf = rg_alloc(to_read, MEM_SLOW);
        if (buf && fread(buf, 1, to_read, fp) == to_read)
        {
            size_t pos = 0;
            while (pos + 10 < to_read)
            {
                if (buf[pos] == 0)
                    break;
                char frame_id[5] = {buf[pos], buf[pos + 1], buf[pos + 2], buf[pos + 3], 0};
                uint32_t frame_sz = ((uint32_t)buf[pos + 4] << 24) | ((uint32_t)buf[pos + 5] << 16) |
                                    ((uint32_t)buf[pos + 6] << 8) | buf[pos + 7];
                if (h[3] == 4)
                    frame_sz = ((uint32_t)buf[pos + 4] << 21) | ((uint32_t)buf[pos + 5] << 14) |
                               ((uint32_t)buf[pos + 6] << 7) | buf[pos + 7];

                pos += 10;
                if (frame_sz == 0 || pos + frame_sz > to_read)
                    break;

                uint8_t enc = buf[pos];
                const char *text = (const char *)&buf[pos + 1];
                size_t text_len = frame_sz > 0 ? frame_sz - 1 : 0;

                char *dst = NULL;
                size_t dst_sz = 0;
                if (strcmp(frame_id, "TIT2") == 0 && title[0] == 0)
                {
                    dst = title;
                    dst_sz = title_sz;
                }
                else if (strcmp(frame_id, "TPE1") == 0 && artist[0] == 0)
                {
                    dst = artist;
                    dst_sz = artist_sz;
                }
                else if (strcmp(frame_id, "TALB") == 0 && album[0] == 0)
                {
                    dst = album;
                    dst_sz = album_sz;
                }

                if (dst && text_len > 0)
                {
                    if (enc == 3 || (enc == 0 && is_valid_utf8(text, text_len)))
                    {
                        size_t cpy = RG_MIN(text_len, dst_sz - 1);
                        memcpy(dst, text, cpy);
                        dst[cpy] = 0;
                    }
                    else if (enc == 0)
                    {
                        latin5_to_utf8(dst, dst_sz, text, text_len);
                    }
                    else if (enc == 1 && text_len >= 2)
                    {
                        size_t o = 0;
                        size_t p = 2;
                        while (p + 1 < text_len && o + 2 < dst_sz)
                        {
                            uint16_t ch = buf[pos + 1 + p] | (buf[pos + 1 + p + 1] << 8);
                            if (buf[pos + 1] == 0xFE && buf[pos + 2] == 0xFF)
                                ch = (buf[pos + 1 + p] << 8) | buf[pos + 1 + p + 1];
                            if (ch == 0) break;
                            if (ch < 0x80)
                            {
                                dst[o++] = (char)ch;
                            }
                            else if (ch == 0x011E) { dst[o++] = 0xC4; dst[o++] = 0x9E; }
                            else if (ch == 0x011F) { dst[o++] = 0xC4; dst[o++] = 0x9F; }
                            else if (ch == 0x0130) { dst[o++] = 0xC4; dst[o++] = 0xB0; }
                            else if (ch == 0x0131) { dst[o++] = 0xC4; dst[o++] = 0xB1; }
                            else if (ch == 0x015E) { dst[o++] = 0xC5; dst[o++] = 0x9E; }
                            else if (ch == 0x015F) { dst[o++] = 0xC5; dst[o++] = 0x9F; }
                            else if (ch < 0x800)
                            {
                                dst[o++] = 0xC0 | (ch >> 6);
                                dst[o++] = 0x80 | (ch & 0x3F);
                            }
                            p += 2;
                        }
                        dst[o] = 0;
                    }
                    clean_trailing_spaces(dst);
                }
                pos += frame_sz;
            }
        }
        if (buf) free(buf);
    }

    if (title[0] == 0 || artist[0] == 0)
    {
        if (fseek(fp, -128, SEEK_END) == 0)
        {
            uint8_t v1[128];
            if (fread(v1, 1, 128, fp) == 128 && memcmp(v1, "TAG", 3) == 0)
            {
                if (title[0] == 0)
                {
                    char tmp[31];
                    memcpy(tmp, v1 + 3, 30);
                    tmp[30] = 0;
                    clean_trailing_spaces(tmp);
                    if (is_valid_utf8(tmp, strlen(tmp)))
                        snprintf(title, title_sz, "%s", tmp);
                    else
                        latin5_to_utf8(title, title_sz, tmp, strlen(tmp));
                }
                if (artist[0] == 0)
                {
                    char tmp[31];
                    memcpy(tmp, v1 + 33, 30);
                    tmp[30] = 0;
                    clean_trailing_spaces(tmp);
                    if (is_valid_utf8(tmp, strlen(tmp)))
                        snprintf(artist, artist_sz, "%s", tmp);
                    else
                        latin5_to_utf8(artist, artist_sz, tmp, strlen(tmp));
                }
                if (album[0] == 0)
                {
                    char tmp[31];
                    memcpy(tmp, v1 + 63, 30);
                    tmp[30] = 0;
                    clean_trailing_spaces(tmp);
                    if (is_valid_utf8(tmp, strlen(tmp)))
                        snprintf(album, album_sz, "%s", tmp);
                    else
                        latin5_to_utf8(album, album_sz, tmp, strlen(tmp));
                }
            }
        }
    }

    fclose(fp);
}

static int scan_callback(const rg_scandir_t *file, void *arg)
{
    if (playlist.count >= MAX_TRACKS)
        return RG_SCANDIR_STOP;

    if (rg_extension_match(file->basename, "mp3"))
        playlist.files[playlist.count++] = strdup(file->basename);

    return RG_SCANDIR_CONTINUE;
}

static int compare_names(const void *a, const void *b)
{
    return strcasecmp(*(char *const *)a, *(char *const *)b);
}

static void playlist_load(const char *folder)
{
    playlist.folder = strdup(folder);
    playlist.files = rg_alloc(MAX_TRACKS * sizeof(char *), MEM_SLOW);
    playlist.titles = rg_alloc(MAX_TRACKS * sizeof(char *), MEM_SLOW);
    playlist.artists = rg_alloc(MAX_TRACKS * sizeof(char *), MEM_SLOW);
    playlist.albums = rg_alloc(MAX_TRACKS * sizeof(char *), MEM_SLOW);
    playlist.labels = rg_alloc(MAX_TRACKS * sizeof(char *), MEM_SLOW);
    playlist.count = 0;

    rg_storage_scandir(folder, scan_callback, NULL, RG_SCANDIR_FILES);
    qsort(playlist.files, playlist.count, sizeof(char *), compare_names);

    for (int i = 0; i < playlist.count; ++i)
    {
        char full_path[RG_PATH_MAX + 1];
        snprintf(full_path, sizeof(full_path), "%s/%s", folder, playlist.files[i]);

        char title[MAX_LABEL] = {0};
        char artist[MAX_LABEL] = {0};
        char album[MAX_LABEL] = {0};

        parse_id3(full_path, title, sizeof(title), artist, sizeof(artist), album, sizeof(album));

        if (title[0] == 0)
        {
            char base[MAX_LABEL];
            snprintf(base, sizeof(base), "%s", playlist.files[i]);
            char *ext = strrchr(base, '.');
            if (ext) *ext = 0;

            char *dash = strstr(base, " - ");
            if (dash)
            {
                *dash = 0;
                snprintf(artist, sizeof(artist), "%s", base);
                snprintf(title, sizeof(title), "%s", dash + 3);
            }
            else
            {
                snprintf(title, sizeof(title), "%s", base);
                snprintf(artist, sizeof(artist), "%s", "Bilinmeyen");
            }
        }
        else if (artist[0] == 0)
        {
            snprintf(artist, sizeof(artist), "%s", "Bilinmeyen");
        }

        clean_trailing_spaces(title);
        clean_trailing_spaces(artist);
        clean_trailing_spaces(album);

        playlist.titles[i] = strdup(title);
        playlist.artists[i] = strdup(artist);
        playlist.albums[i] = strdup(album);

        char label[MAX_LABEL * 2];
        if (strcmp(artist, "Bilinmeyen") == 0)
            snprintf(label, sizeof(label), "%s", title);
        else
            snprintf(label, sizeof(label), "%s - %s", artist, title);
        playlist.labels[i] = strdup(label);
    }
    RG_LOGI("Found %d songs in '%s'", playlist.count, folder);
}

static int playlist_find(const char *path)
{
    const char *name = path ? rg_basename(path) : NULL;
    for (int i = 0; name && i < playlist.count; ++i)
    {
        if (strcmp(playlist.files[i], name) == 0)
            return i;
    }
    return 0;
}

static int next_track(int from)
{
    if (playlist.count < 2)
        return 0;

    if (player.shuffle)
    {
        int next = rand() % (playlist.count - 1);
        return next >= from ? next + 1 : next;
    }
    return (from + 1) % playlist.count;
}

static void skip_id3_tag(FILE *fp)
{
    uint8_t header[10];

    if (fread(header, 1, sizeof(header), fp) == sizeof(header) && memcmp(header, "ID3", 3) == 0)
    {
        long size = (header[6] << 21) | (header[7] << 14) | (header[8] << 7) | header[9];
        fseek(fp, size + sizeof(header), SEEK_SET);
    }
    else
    {
        fseek(fp, 0, SEEK_SET);
    }
}

static bool play_file(const char *path, uint8_t *input, mp3dec_t *decoder, int16_t *pcm)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
    {
        RG_LOGE("Failed to open '%s'", path);
        return false;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    skip_id3_tag(fp);

    const long data_start = ftell(fp);
    long consumed = 0;
    int64_t samples_played = 0;
    int64_t decode_time = 0;
    int sample_rate = 0;
    size_t available = 0;
    size_t position = 0;
    bool eof = false;
    bool any_frame = false;

    mp3dec_init(decoder);
    player.elapsed = player.duration = player.progress = 0;
    player.bitrate = 0;
    player.samplerate = 0;

    while (!player.quit && player.request < 0)
    {
        if (player.paused)
        {
            rg_task_delay(20);
            continue;
        }

        if (available < READ_THRESHOLD && !eof)
        {
            memmove(input, input + position, available);
            position = 0;
            size_t count = fread(input + available, 1, INPUT_BUFFER_SIZE - available, fp);
            eof = count == 0;
            available += count;
        }

        if (available == 0)
            break;

        mp3dec_frame_info_t info;
        int64_t start = rg_system_timer();
        int samples = mp3dec_decode_frame(decoder, input + position, available, pcm, &info);
        decode_time += rg_system_timer() - start;

        if (info.frame_bytes == 0)
        {
            if (eof)
                break;
            available = 0;
            continue;
        }

        position += info.frame_bytes;
        available -= info.frame_bytes;
        consumed += info.frame_bytes;

        if (samples == 0)
            continue;

        if (info.hz != sample_rate)
        {
            sample_rate = info.hz;
            rg_audio_set_sample_rate(sample_rate);
            RG_LOGI("Playing '%s': %d Hz, %d kbps, %d channels", rg_basename(path), info.hz, info.bitrate_kbps, info.channels);
        }

        player.samplerate = info.hz;
        player.bitrate = info.bitrate_kbps;

        if (info.channels == 1)
        {
            for (int i = samples - 1; i >= 0; --i)
                pcm[i * 2] = pcm[i * 2 + 1] = pcm[i];
        }

        rg_audio_submit((const rg_audio_frame_t *)pcm, samples);

        any_frame = true;
        samples_played += samples;
        player.elapsed = samples_played / sample_rate;
        player.progress = consumed * 1000 / RG_MAX(file_size - data_start, 1);
        if (player.elapsed >= 2)
            player.duration = (int)(samples_played * (file_size - data_start) / consumed / sample_rate);

        if ((samples_played / samples) % 500 == 0)
            RG_LOGI("Decoding took %d%% of realtime", (int)(decode_time * 100 * sample_rate / (samples_played * 1000000)));
    }

    fclose(fp);
    return any_frame;
}

static void audio_task(void *arg)
{
    uint8_t *input = rg_alloc(INPUT_BUFFER_SIZE, MEM_FAST);
    int16_t *pcm = rg_alloc(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(int16_t), MEM_FAST);
    mp3dec_t *decoder = rg_alloc(sizeof(mp3dec_t), MEM_FAST);
    char path[RG_PATH_MAX + 1];
    int failures = 0;

    while (!player.quit)
    {
        if (player.request < 0)
        {
            rg_task_delay(20);
            continue;
        }

        int track = player.request;
        player.request = -1;
        player.current = track;
        need_redraw = true;

        snprintf(path, sizeof(path), "%s/%s", playlist.folder, playlist.files[track]);
        failures = play_file(path, input, decoder, pcm) ? 0 : failures + 1;

        if (failures >= playlist.count)
        {
            player.failed = true;
            need_redraw = true;
        }
        else if (player.request < 0 && !player.quit)
        {
            player.request = next_track(track);
        }
    }

    free(input);
    free(pcm);
    free(decoder);
    player.stopped = true;
}

static void stop_audio(void)
{
    player.quit = true;
    player.paused = false;
    while (!player.stopped)
        rg_task_delay(10);
}

static void event_handler(int event, void *arg)
{
    if (event == RG_EVENT_REDRAW)
        need_redraw = true;
    else if (event == RG_EVENT_SHUTDOWN)
        stop_audio();
}

static void set_paused(bool paused)
{
    player.paused = paused;
    rg_audio_set_mute(paused);
}

static void draw_battery(int width)
{
    rg_battery_t bat = rg_input_read_battery();
    if (bat.present)
    {
        char bat_str[16];
        snprintf(bat_str, sizeof(bat_str), "%d%%", (int)bat.level);
        rg_gui_draw_text(width - 70, 2, 32, bat_str, C_WHITE, COLOR_BG, RG_TEXT_ALIGN_RIGHT);

        rg_gui_draw_rect(width - 34, 4, 18, 10, 1, C_SILVER, COLOR_BG);
        rg_gui_draw_rect(width - 16, 7, 2, 4, 1, C_SILVER, COLOR_BG);

        int fill_w = (14 * RG_MIN((int)bat.level, 100)) / 100;
        rg_color_t fill_col = bat.level > 40 ? C_FOREST_GREEN : (bat.level > 20 ? C_ORANGE : C_RED);
        rg_gui_draw_rect(width - 32, 6, 14, 6, 0, 0, COLOR_BG);
        if (fill_w > 0)
            rg_gui_draw_rect(width - 32, 6, fill_w, 6, 0, 0, fill_col);
    }
    else
    {
        rg_gui_draw_text(width - 50, 2, 42, "USB", C_SILVER, COLOR_BG, RG_TEXT_ALIGN_RIGHT);
    }
}

static void draw_status(void)
{
    const int width = rg_display_get_width();
    const int bar_x = 12;
    const int bar_y = 98;
    const int bar_w = width - 24;
    int duration = player.duration;

    draw_battery(width);

    char state_str[48];
    snprintf(state_str, sizeof(state_str), "%s%s  VOL %d%%",
             player.paused ? "[DURDU]" : "[CALIYOR]",
             player.shuffle ? " [K]" : "",
             rg_audio_get_volume());
    rg_gui_draw_text(130, 26, width - 142, state_str,
                     player.paused ? COLOR_ACCENT : C_SPRING_GREEN,
                     COLOR_BG, RG_TEXT_ALIGN_RIGHT);

    char fmt_str[48];
    if (player.bitrate > 0)
        snprintf(fmt_str, sizeof(fmt_str), "MP3  %d kbps  %d Hz", player.bitrate, player.samplerate);
    else
        snprintf(fmt_str, sizeof(fmt_str), "MP3  44100 Hz");
    rg_gui_draw_text(12, 82, width - 24, fmt_str, C_DIM_GRAY, COLOR_BG, 0);

    rg_gui_draw_rect(bar_x, bar_y, bar_w, 10, 1, C_SILVER, COLOR_BG);
    int p_fill = ((bar_w - 4) * RG_MIN(player.progress, 1000)) / 1000;
    if (p_fill > 0)
        rg_gui_draw_rect(bar_x + 2, bar_y + 2, p_fill, 6, 0, 0, COLOR_ACCENT);

    char t_el[16];
    snprintf(t_el, sizeof(t_el), "%02d:%02d", player.elapsed / 60, player.elapsed % 60);
    rg_gui_draw_text(12, 114, 60, t_el, COLOR_TEXT, COLOR_BG, 0);

    char t_dur[16];
    if (duration > 0)
        snprintf(t_dur, sizeof(t_dur), "%02d:%02d", duration / 60, duration % 60);
    else
        snprintf(t_dur, sizeof(t_dur), "--:--");
    rg_gui_draw_text(width - 72, 114, 60, t_dur, C_LIGHT_GRAY, COLOR_BG, RG_TEXT_ALIGN_RIGHT);

    char p_txt[16];
    snprintf(p_txt, sizeof(p_txt), "%d%%", player.progress / 10);
    rg_gui_draw_text(130, 114, 60, p_txt, COLOR_ACCENT, COLOR_BG, RG_TEXT_ALIGN_CENTER);
}

static void draw_screen(void)
{
    const int width = rg_display_get_width();
    const int height = rg_display_get_height();
    int track = player.current;

    rg_display_clear(COLOR_BG);

    rg_gui_draw_rect(0, 0, width, 18, 0, 0, COLOR_BG);
    rg_gui_draw_text(8, 2, 0, "RETRO-GO MUSIC", C_GOLD, COLOR_BG, 0);
    rg_gui_draw_rect(0, 18, width, 1, 0, 0, C_DARK_GRAY);

    if (playlist.count == 0 || player.failed)
    {
        rg_gui_draw_text(12, 44, width - 24, playlist.count ? "Sarkilar acilamadi." : "Sarki bulunamadi.", COLOR_TEXT, COLOR_BG, RG_TEXT_BIGGER);
        rg_gui_draw_text(12, 84, width - 24, "MP3 dosyalarini microSD kartta\nroms/music klasorune koy.", COLOR_DIM, COLOR_BG, RG_TEXT_MULTILINE);
        rg_gui_draw_text(12, height - 16, width - 24, "MENU: cikis", COLOR_DIM, COLOR_BG, 0);
        draw_battery(width);
        return;
    }

    rg_gui_draw_rect(6, 22, width - 12, 126, 1, C_STEEL_BLUE, COLOR_BG);

    char trk_str[32];
    snprintf(trk_str, sizeof(trk_str), "TRACK %02d/%02d", RG_MAX(track, 0) + 1, playlist.count);
    rg_gui_draw_text(12, 26, 0, trk_str, COLOR_ACCENT, COLOR_BG, 0);

    rg_gui_draw_rect(10, 40, width - 20, 1, 0, 0, C_DARK_SLATE_GRAY);

    if (track >= 0 && track < playlist.count)
    {
        rg_gui_draw_text(12, 45, width - 24, playlist.titles[track], COLOR_TEXT, COLOR_BG, RG_TEXT_BIGGER);

        char meta[128];
        if (playlist.albums[track][0])
            snprintf(meta, sizeof(meta), "%s | %s", playlist.artists[track], playlist.albums[track]);
        else
            snprintf(meta, sizeof(meta), "%s", playlist.artists[track]);
        rg_gui_draw_text(12, 67, width - 24, meta, C_LIGHT_GRAY, COLOR_BG, 0);
    }

    draw_status();

    rg_gui_draw_rect(0, height - 18, width, 1, 0, 0, C_DARK_GRAY);
    rg_gui_draw_text(8, height - 15, width - 16, "A: DURDUR  B: LISTE  START: KARISIK  YON: SARKI/SES", C_SILVER, COLOR_BG, 0);
}

static void show_song_list(void)
{
    rg_gui_option_t *options = rg_alloc((playlist.count + 1) * sizeof(rg_gui_option_t), MEM_SLOW);

    for (int i = 0; i < playlist.count; ++i)
        options[i] = (rg_gui_option_t){i, playlist.labels[i], NULL, RG_DIALOG_FLAG_NORMAL, NULL};
    options[playlist.count] = (rg_gui_option_t)RG_DIALOG_END;

    intptr_t selected = rg_gui_dialog("Sarkilar", options, RG_MAX(player.current, 0));
    free(options);

    if (selected >= 0 && selected < playlist.count)
    {
        set_paused(false);
        player.request = selected;
    }
    need_redraw = true;
}

static void show_exit_menu(void)
{
    const rg_gui_option_t options[] = {
        {0, "Devam et", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
        {1, "Menuye don", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
        RG_DIALOG_END,
    };

    if (rg_gui_dialog("Muzik", options, 0) == 1)
    {
        stop_audio();
        rg_system_exit();
    }
    need_redraw = true;
}

static void handle_keys(uint32_t pressed)
{
    if (pressed & RG_KEY_MENU)
        show_exit_menu();
    else if (pressed & RG_KEY_OPTION)
    {
        rg_gui_options_menu();
        need_redraw = true;
    }

    if (playlist.count == 0 || player.failed)
        return;

    if (pressed & RG_KEY_A)
        set_paused(!player.paused);
    else if (pressed & RG_KEY_B)
        show_song_list();
    else if (pressed & RG_KEY_START)
        player.shuffle = !player.shuffle;
    else if (pressed & RG_KEY_RIGHT)
    {
        set_paused(false);
        player.request = next_track(player.current);
    }
    else if (pressed & RG_KEY_LEFT)
    {
        int restart = player.elapsed >= RESTART_SECONDS || playlist.count < 2;
        set_paused(false);
        player.request = restart ? player.current : (player.current + playlist.count - 1) % playlist.count;
    }
    else if (pressed & RG_KEY_UP)
        rg_audio_set_volume(RG_MIN(rg_audio_get_volume() + VOLUME_STEP, 100));
    else if (pressed & RG_KEY_DOWN)
        rg_audio_set_volume(RG_MAX(rg_audio_get_volume() - VOLUME_STEP, 0));
}

void app_main(void)
{
    const rg_handlers_t handlers = {
        .event = &event_handler,
    };

    app = rg_system_init(AUDIO_SAMPLE_RATE, &handlers, NULL);
    rg_system_set_tick_rate(30);
    srand((unsigned)rg_system_timer());

    const char *rom = app->romPath && app->romPath[0] ? app->romPath : NULL;
    playlist_load(rom ? rg_dirname(rom) : DEFAULT_MUSIC_PATH);

    if (playlist.count > 0)
        player.request = playlist_find(rom);

    rg_task_create("muzik_audio", audio_task, NULL, 32 * 1024, RG_TASK_PRIORITY_6, 1);

    uint32_t previous = rg_input_read_gamepad();
    int64_t next_update = 0;
    int shown_track = -2;
    int saved_backlight = -1;

    while (true)
    {
        uint32_t keys = rg_input_read_gamepad();
        uint32_t pressed = keys & ~previous;
        previous = keys;

        if (pressed && saved_backlight >= 0)
        {
            rg_display_set_backlight(saved_backlight);
            saved_backlight = -1;
        }
        else if (pressed & RG_KEY_SELECT)
        {
            saved_backlight = rg_display_get_backlight();
            rg_display_set_backlight(0);
        }
        else if (pressed)
        {
            handle_keys(pressed);
            previous = rg_input_read_gamepad();
            next_update = 0;
        }

        if (saved_backlight < 0)
        {
            int64_t now = rg_system_timer();
            if (need_redraw || shown_track != player.current)
            {
                need_redraw = false;
                shown_track = player.current;
                draw_screen();
                next_update = now + 500000;
            }
            else if (now >= next_update && playlist.count > 0 && !player.failed)
            {
                draw_status();
                next_update = now + 500000;
            }
        }

        rg_system_tick(0);
        rg_task_delay(30);
    }
}
