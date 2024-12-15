#include <alsa/asoundlib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sndfile.h> // 添加对libsndfile的支持
#include <lame/lame.h> // 添加对LAME库的支持
#include <getopt.h>

#define SAMPLE_RATE 44100 // 采样率

typedef enum { SQUARE, SINE } WaveType;

typedef struct {
    unsigned int frequency;
    unsigned int duration_ms;
    WaveType wave_type; // 新增波形类型成员
} Note;

// 设置硬件参数函数
int set_hw_params(snd_pcm_t *handle, unsigned int rate);

void generate_wave(short *buffer, unsigned int frequency, unsigned int samples, WaveType type) {
    double two_pi_f = 2 * M_PI * frequency / SAMPLE_RATE;
    for (unsigned int i = 0; i < samples; ++i) {
        double t = i * two_pi_f;
        switch (type) {
            case SINE:
                buffer[i] = (short)(32760 * sin(t));
                break;
            case SQUARE:
                buffer[i] = (sin(t) >= 0) ? 32760 : -32760;
                break;
        }
    }
}

// 解析 .bmusic 文件
int parse_bmusic_file(const char *filename, Note **notes, size_t *num_notes, WaveType default_wave_type) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file for reading");
        return -1;
    }

    // 分配一个较大的临时数组用于存储音符，之后可以根据实际数量调整
    size_t temp_capacity = 300;
    *notes = malloc(temp_capacity * sizeof(Note));
    if (!*notes) {
        fprintf(stderr, "failed to allocate memory\n");
        fclose(file);
        return -1;
    }

    size_t count = 0;

    // 遍历文件，直接读取音符数据，忽略其他内容
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // 忽略空行和以#开头的注释行
        if (line[0] == '\n' || line[0] == '#') continue;

        unsigned int frequency, duration_ms;
        if (sscanf(line, "%u %u", &frequency, &duration_ms) == 2) {
            // 如果需要，扩展数组容量
            if (count >= temp_capacity) {
                temp_capacity *= 2;
                *notes = realloc(*notes, temp_capacity * sizeof(Note));
                if (!*notes) {
                    fprintf(stderr, "failed to reallocate memory\n");
                    fclose(file);
                    return -1;
                }
            }
            (*notes)[count].frequency = frequency;
            (*notes)[count].duration_ms = duration_ms;
            (*notes)[count].wave_type = default_wave_type; // 使用默认波形类型
            count++;
        }
        // 不再检查 in_note_section，允许音符部分内的任意格式
    }

    // 调整分配的内存大小以匹配实际音符数量
    *notes = realloc(*notes, count * sizeof(Note));
    if (!*notes && count > 0) { // 只有当有音符并且realloc失败时报告错误
        fprintf(stderr, "failed to reallocate memory\n");
        fclose(file);
        return -1;
    }

    *num_notes = count;

    fclose(file);
    return 0;
}

// 播放音符列表或保存为文件
void play_or_save_notes(Note *notes, size_t num_notes, const char *output_filename, const char *format, int compression) {
    int err;
    snd_pcm_t *handle = NULL;
    short *buffer = NULL;
    SNDFILE *sf = NULL;
    SF_INFO sfinfo = {0};
    lame_t lame = NULL;
    FILE *mp3_file = NULL;

    // 计算总的样本数并分配内存
    unsigned int total_samples = 0;
    for (size_t i = 0; i < num_notes; ++i) {
        total_samples += notes[i].duration_ms * SAMPLE_RATE / 1000;
    }
    buffer = malloc(total_samples * sizeof(short));
    if (!buffer) {
        fprintf(stderr, "failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    // 填充整个音频缓冲区
    unsigned int current_sample = 0;
    for (size_t note_index = 0; note_index < num_notes; ++note_index) {
        unsigned int frequency = notes[note_index].frequency;
        unsigned int samples = notes[note_index].duration_ms * SAMPLE_RATE / 1000;
        WaveType wave_type = notes[note_index].wave_type;

        generate_wave(buffer + current_sample, frequency, samples, wave_type);
        current_sample += samples;
    }

    if (output_filename != NULL) {
        if (strcmp(format, "wav") == 0) {
            // 设置文件信息
            sfinfo.samplerate = SAMPLE_RATE;
            sfinfo.frames = total_samples;
            sfinfo.channels = 1;
            sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

            // 打开文件准备写入
            sf = sf_open(output_filename, SFM_WRITE, &sfinfo);
            if (!sf) {
                fprintf(stderr, "Failed to open WAV file for writing: %s\n", sf_strerror(NULL));
                free(buffer);
                exit(EXIT_FAILURE);
            }

            // 写入文件
            sf_writef_short(sf, buffer, total_samples);
            sf_close(sf);
        } else if (strcmp(format, "mp3") == 0) {
            // 初始化 LAME 编码器
            lame = lame_init();
            lame_set_in_samplerate(lame, SAMPLE_RATE);
            lame_set_num_channels(lame, 1); // 单声道
            lame_set_quality(lame, compression); // 设置质量（0-9），数值越低质量越高
            lame_init_params(lame);

            // 打开 MP3 文件准备写入
            mp3_file = fopen(output_filename, "wb");
            if (!mp3_file) {
                fprintf(stderr, "Failed to open MP3 file for writing.\n");
                free(buffer);
                lame_close(lame);
                exit(EXIT_FAILURE);
            }

            // 编码并写入 MP3 文件
            unsigned char mp3_buffer[16384]; // 输出缓存
            for (unsigned int i = 0; i < total_samples; ) {
                unsigned int samples_to_encode = total_samples - i;
                if (samples_to_encode > 1152) samples_to_encode = 1152; // LAME 的最大输入块大小
                int written = lame_encode_buffer_interleaved(lame, buffer + i, samples_to_encode, mp3_buffer, sizeof(mp3_buffer));
                fwrite(mp3_buffer, 1, written, mp3_file);
                i += samples_to_encode;
            }

            // 写入剩余的 MP3 数据
            int flush_result = lame_encode_flush(lame, mp3_buffer, sizeof(mp3_buffer));
            fwrite(mp3_buffer, 1, flush_result, mp3_file);

            fclose(mp3_file);
            lame_close(lame);
        }
    } else {
        // 打开PCM设备用于回放
        if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
            fprintf(stderr, "cannot open audio device (%s)\n", snd_strerror(err));
            free(buffer);
            exit(EXIT_FAILURE);
        }

        // 设置硬件参数
        if ((err = set_hw_params(handle, SAMPLE_RATE)) < 0) {
            fprintf(stderr, "cannot set hardware parameters (%s)\n", snd_strerror(err));
            free(buffer);
            exit(EXIT_FAILURE);
        }

        // 写入音频数据
        snd_pcm_sframes_t frames = snd_pcm_writei(handle, buffer, total_samples);
        if (frames < 0) {
            frames = snd_pcm_recover(handle, frames, 0);
            if (frames < 0) {
                fprintf(stderr, "snd_pcm_writei failed: %s\n", snd_strerror(frames));
                free(buffer);
                exit(EXIT_FAILURE);
            }
        } else if (frames != (snd_pcm_sframes_t)total_samples) {
            fprintf(stderr, "short write (%li / %u frames)\n", frames, total_samples);
        }

        // 确保所有数据都被播放完毕
        if ((err = snd_pcm_drain(handle)) < 0) {
            fprintf(stderr, "drain failed: %s\n", snd_strerror(err));
            free(buffer);
            exit(EXIT_FAILURE);
        }

        // 关闭PCM设备
        snd_pcm_close(handle);
    }

    free(buffer);
}

// 设置硬件参数函数
int set_hw_params(snd_pcm_t *handle, unsigned int rate) {
    int err;
    unsigned int val;
    snd_pcm_hw_params_t *params;

    /* 分配硬件参数对象 */
    snd_pcm_hw_params_alloca(&params);

    /* 填充硬件参数对象 */
    snd_pcm_hw_params_any(handle, params);

    /* 设置为阻塞模式 */
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    /* 设置样本格式 */
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

    /* 设置声道数 */
    snd_pcm_hw_params_set_channels(handle, params, 1);

    /* 设置采样率 */
    val = rate;
    snd_pcm_hw_params_set_rate_near(handle, params, &val, 0);

    /* 设置周期大小 */
    snd_pcm_hw_params_set_period_size_near(handle, params, (snd_pcm_uframes_t *) &val, 0);

    /* 写入硬件参数 */
    if ((err = snd_pcm_hw_params(handle, params)) < 0) {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(err));
        return err;
    }

    return 0;
}

// 主函数
int main(int argc, char *argv[]) {
    const char *output_filename = NULL;
    const char *format = "wav"; // 默认为WAV格式
    int compression = 5; // 默认压缩级别
    WaveType default_wave_type = SINE; // 默认波形类型

    int opt;
    while ((opt = getopt(argc, argv, "o:f:c:w:")) != -1) {
        switch (opt) {
            case 'o':
                output_filename = optarg;
                break;
            case 'f':
                format = optarg;
                break;
            case 'c':
                compression = atoi(optarg);
                if (compression < 0 || compression > 9) {
                    fprintf(stderr, "Compression level must be between 0 and 9.\n");
                    return EXIT_FAILURE;
                }
                break;
            case 'w':
                default_wave_type = atoi(optarg) == 1 ? SINE : SQUARE;
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s [-o output-file] [-f format] [-c compression] [-w wave-type] bmusic-file\n", argv[0]);
                fprintf(stderr, "Format can be 'wav' or 'mp3'.\n");
                fprintf(stderr, "Compression is an integer between 0 and 9 for MP3.\n");
                fprintf(stderr, "Wave-type can be 0 for square wave or 1 for sine wave.\n");
                return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Expected argument after options.\n");
        return EXIT_FAILURE;
    }

    const char *bmusic_filename = argv[optind];

    Note *notes = NULL;
    size_t num_notes = 0;

    if (parse_bmusic_file(bmusic_filename, &notes, &num_notes, default_wave_type) != 0) {
        return EXIT_FAILURE;
    }

    if (num_notes > 0) {
        play_or_save_notes(notes, num_notes, output_filename, format, compression);
    } else {
        fprintf(stderr, "No notes found in the file.\n");
    }

    free(notes);

    return EXIT_SUCCESS;
}
