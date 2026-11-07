// MP3 player for Retro-Go. The launcher's "music" tab picks the first song, this app plays every
// song of that folder. The audio task owns the file and decoder, the main task owns the screen and
// the buttons, and they only talk through the `player` struct.

#include <rg_system.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#include "minimp3.h"

#define DEFAULT_MUSIC_PATH RG_BASE_PATH_ROMS "/music"
#define AUDIO_SAMPLE_RATE  44100
#define MAX_TRACKS         1000
#define MAX_LABEL          80
#define INPUT_BUFFER_SIZE  (16 * 1024)
#define READ_THRESHOLD     (8 * 1024)
#define VOLUME_STEP        5
#define RESTART_SECONDS    3 // Pressing "previous" later than this restarts the current song

#define COLOR_BG     C_BLACK
#define COLOR_TEXT   C_WHITE
#define COLOR_DIM    C_GRAY
#define COLOR_ACCENT C_DARK_ORANGE

static struct
{
    char *folder;
    char **files;
    char **labels;
    int count;
} playlist;

static struct
{
    volatile int request;  // Track the audio task must open next, -1 when there is none
    volatile int current;
    volatile bool paused;
    volatile bool shuffle;
    volatile bool quit;
    volatile bool stopped;
    volatile bool failed;  // No song in the folder could be opened
    volatile int elapsed;  // Seconds
    volatile int duration; // Seconds, 0 until it can be estimated
    volatile int progress; // Permille of the file
} player = {.request = -1, .current = -1};

static rg_app_t *app;
static volatile bool need_redraw = true;

// The built-in fonts have no Turkish letters, so names are shown in plain ASCII
static void to_ascii(char *out, size_t out_size, const char *in)
{
    static const struct { const char *from; char to; } table[] = {
        {"ç", 'c'}, {"Ç", 'C'}, {"ğ", 'g'}, {"Ğ", 'G'}, {"ı", 'i'}, {"İ", 'I'},
        {"ö", 'o'}, {"Ö", 'O'}, {"ş", 's'}, {"Ş", 'S'}, {"ü", 'u'}, {"Ü", 'U'},
    };
    size_t pos = 0;

    while (*in && pos < out_size - 1)
    {
        if ((unsigned char)*in < 0x80)
        {
            out[pos++] = *in++;
            continue;
        }

        bool found = false;
        for (size_t i = 0; i < RG_COUNT(table) && !found; ++i)
        {
            size_t len = strlen(table[i].from);
            if (strncmp(in, table[i].from, len) == 0)
            {
                out[pos++] = table[i].to;
                in += len;
                found = true;
            }
        }

        if (!found)
        {
            out[pos++] = '?';
            in++;
            while (((unsigned char)*in & 0xC0) == 0x80) // Skip the rest of the UTF-8 sequence
                in++;
        }
    }
    out[pos] = 0;
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
    playlist.labels = rg_alloc(MAX_TRACKS * sizeof(char *), MEM_SLOW);
    playlist.count = 0;

    rg_storage_scandir(folder, scan_callback, NULL, RG_SCANDIR_FILES);
    qsort(playlist.files, playlist.count, sizeof(char *), compare_names);

    for (int i = 0; i < playlist.count; ++i)
    {
        char label[MAX_LABEL];
        to_ascii(label, sizeof(label), playlist.files[i]);

        char *ext = strrchr(label, '.');
        if (ext)
            *ext = 0;
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

        if (info.frame_bytes == 0) // Not enough data for a whole frame
        {
            if (eof)
                break;
            available = 0;
            continue;
        }

        position += info.frame_bytes;
        available -= info.frame_bytes;
        consumed += info.frame_bytes;

        if (samples == 0) // Junk between frames, the decoder already skipped it
            continue;

        if (info.hz != sample_rate)
        {
            sample_rate = info.hz;
            rg_audio_set_sample_rate(sample_rate);
            RG_LOGI("Playing '%s': %d Hz, %d kbps, %d channels", rg_basename(path), info.hz, info.bitrate_kbps, info.channels);
        }

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
        if (player.elapsed >= 2) // Works for variable bitrates, unlike the header's bitrate
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
        else if (player.request < 0 && !player.quit) // The song ended by itself
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

static void draw_status(void)
{
    char line[48];
    const int width = rg_display_get_width();
    const int bar_width = width - 24;
    int duration = player.duration;

    rg_gui_draw_rect(12, 112, bar_width, 8, 1, COLOR_DIM, COLOR_BG);
    rg_gui_draw_rect(13, 113, (bar_width - 2) * RG_MIN(player.progress, 1000) / 1000, 6, 0, 0, COLOR_ACCENT);

    if (duration)
        snprintf(line, sizeof(line), "%02d:%02d / %02d:%02d", player.elapsed / 60, player.elapsed % 60, duration / 60, duration % 60);
    else
        snprintf(line, sizeof(line), "%02d:%02d", player.elapsed / 60, player.elapsed % 60);
    rg_gui_draw_text(12, 126, 130, line, COLOR_TEXT, COLOR_BG, 0);

    snprintf(line, sizeof(line), "Ses %d%%%s%s", rg_audio_get_volume(), player.shuffle ? "  Karisik" : "", player.paused ? "  Durdu" : "");
    rg_gui_draw_text(142, 126, width - 154, line, player.paused ? COLOR_ACCENT : COLOR_DIM, COLOR_BG, RG_TEXT_ALIGN_RIGHT);
}

static void draw_screen(void)
{
    char line[32];
    const int width = rg_display_get_width();
    const int height = rg_display_get_height();
    int track = player.current;

    rg_display_clear(COLOR_BG);

    if (playlist.count == 0 || player.failed)
    {
        rg_gui_draw_text(12, 44, width - 24, playlist.count ? "Sarkilar acilamadi." : "Sarki bulunamadi.", COLOR_TEXT, COLOR_BG, RG_TEXT_BIGGER);
        rg_gui_draw_text(12, 84, width - 24, "MP3 dosyalarini microSD kartta\nroms/music klasorune koy.", COLOR_DIM, COLOR_BG, RG_TEXT_MULTILINE);
        rg_gui_draw_text(12, height - 22, width - 24, "MENU: cikis", COLOR_DIM, COLOR_BG, 0);
        return;
    }

    snprintf(line, sizeof(line), "Sarki %d / %d", RG_MAX(track, 0) + 1, playlist.count);
    rg_gui_draw_text(12, 10, 0, line, COLOR_ACCENT, COLOR_BG, 0);
    if (track >= 0)
        rg_gui_draw_text(12, 34, width - 24, playlist.labels[track], COLOR_TEXT, COLOR_BG, RG_TEXT_BIGGER | RG_TEXT_MULTILINE);
    rg_gui_draw_text(12, height - 22, width - 24, "A durdur  Sol/Sag sarki  Yukari/Asagi ses  B liste", COLOR_DIM, COLOR_BG, 0);
    draw_status();
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

    // Decoding needs far more stack than usual, minimp3 keeps its scratch buffers there
    rg_task_create("muzik_audio", audio_task, NULL, 32 * 1024, RG_TASK_PRIORITY_6, 1);

    uint32_t previous = rg_input_read_gamepad();
    int64_t next_update = 0;
    int shown_track = -2;
    int saved_backlight = -1; // Set while the screen is switched off

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
            previous = rg_input_read_gamepad(); // A dialog may have swallowed the release
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
