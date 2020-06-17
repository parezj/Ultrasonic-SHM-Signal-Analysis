/*
 * CTU/DIT project - SHM signal analysis
 * Author: Jakub Parez <parez.jakub@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <libgen.h>

#include "../include/shm_shift.h"

#define TEST_DATA       	"./data/data.csv"
#define OUT_DIR       	    "./data/ni=10/"
#define DEFAULT_DELIM   	';'
#define DEFAULT_INTERP_N 	10
#define DEFAULT_REF_COL     0
#define CALC_ITERS          100

#define TIC clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start)
#define TOC clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end)

static float mean(int* y, int len);
static struct timespec diff(struct timespec start, struct timespec end);
static void fush_cpu_cache();


int main(int argc, char **argv)
{
    char file_in[255] = { '\0' };
    char dir_out[255] = { '\0' };
    int ch;
    char delim = DEFAULT_DELIM;
    int ni = DEFAULT_INTERP_N;
    int ref_col = DEFAULT_REF_COL;
    opterr = 0;

    /** Parse arguments */
    while ((ch = getopt (argc, argv, "abc:")) != -1) // parse args
    {
        switch (ch)
        {
            case 'f':
                strcpy(file_in, optarg);       /** input csv data file */
                break;
            case 'o':
                strcpy(dir_out, optarg);       /** out dir */
                break;
            case 'd':
                delim = optarg[0];             /** csv delimiter */
                break;
            case 'i':
                ni = atoi(optarg);             /** ratio of num of interpolated points to one data x len*/
                break;
            case 'r':
                ref_col = atoi(optarg);        /** reference signal column */
                break;
            case '?':
                if (optopt == 'f')
                    fprintf (stderr, "Option -f requires an argument filename\n");
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
                return -1;
            default:
                abort ();
        }
    }

    /** Parse input and output file names */
    if (file_in[0] == '\0')
        strcpy(file_in, TEST_DATA);
    char file_out[255] = { '\0' };
    char file_in_base[255] = { '\0' };
    strcpy(file_in_base, basename(file_in));

    /** Merge output dir and prefix */
    if (dir_out[0] == '\0')
        strcpy(dir_out, OUT_DIR);
    sprintf(file_out, "%s%s", dir_out, file_in_base);

    /** Remove extension and create it prefix for output */
    char *endch = file_out + strlen(file_out);
    while (endch > file_out && *endch != '.') --endch;
    if (endch > file_out)
        *endch = '\0';

    /** If output dir not exists, create it */
    DIR* dir = opendir(dir_out);
    if (!dir && mkdir(dir_out, 0777) != 0)
    {
        printf("cannot create output folder: %s", dir_out);
        return -5;
    }

    printf("data in: %s\n", file_in);
    struct timespec start, end, df;

    /** Create library object and parse csv data file */
    shm_shift_t* shm = shm_shift__create();                 /* create object */
    int ret1 = shm_shift__read_csv(shm, file_in, delim);    /* read csv */
    if (ret1 != 0)
    {
        shm_shift__free(shm);
        return(ret1);
    }

    /** Piecewise Linear Interpolation */
    printf("======== PLI ========\n\n");
    shm->interp_min = 1;  /* force to interpolate local minimus rather then default maximums */
    shm_shift__analyse_csv(shm, PLI, ref_col, ni);           /* first run for minimums */
    shm_shift__write_csv(shm, file_out, delim);              /* write csv results */
    shm_shift__dispose_calc(shm);                            /* dispose calculated values */
    shm->interp_min = 0;

    float pli_ms = 0;
    for (int i = 0; i < CALC_ITERS; i++)
    {
        printf("-- iteration %d/%d --\n", i + 1, CALC_ITERS);
        TIC;
        shm_shift__analyse_csv(shm, PLI, ref_col, ni);       /* then interpolate maximums */
        TOC;

        df = diff(start, end);
        pli_ms += ((float)df.tv_nsec / 1000.0) / shm->cols;
        fush_cpu_cache();
    }
    printf("\n");
    shm_shift__write_csv(shm, file_out, delim);              /* write csv results */
    shm_shift__dispose_calc(shm);                            /* dispose calculated values */
    pli_ms /= (float)CALC_ITERS;
    printf("PLI duration: %.3f us\n\n", pli_ms);


    /** Cubic Spline Interpolation */
    printf("======== CSI ========\n\n");
    shm->interp_min = 1; /* force to interpolate local minimus rather then default maximums */
    shm_shift__analyse_csv(shm, CSI, ref_col, ni);          /* first run for minimums */
    shm_shift__write_csv(shm, file_out, delim);             /* write csv results */
    shm_shift__dispose_calc(shm);                           /* dispose calculated values */
    shm->interp_min = 0;

    float csi_ms = 0;
    for (int i = 0; i < CALC_ITERS; i++)
    {
        printf("-- iteration %d/%d --\n", i + 1, CALC_ITERS);
        TIC;
        shm_shift__analyse_csv(shm, CSI, ref_col, ni);      /* then interpolate maximums  */
        TOC;

        df = diff(start, end);
        csi_ms += ((float)df.tv_nsec / 1000.0) / shm->cols;
        fush_cpu_cache();
    }
    printf("\n");
    shm_shift__write_csv(shm, file_out, delim);             /* write csv results */
    shm_shift__dispose_calc(shm);                           /* dispose calculated values */
    csi_ms /= (float)CALC_ITERS;
    printf("CSI duration: %.3f us\n\n", csi_ms);


    /** Extremum Center Inerpolation */
    printf("======== ECI ========\n\n");

    float eci_ms = 0;
    for (int i = 0; i < CALC_ITERS; i++)
    {
        printf("-- iteration %d/%d --\n", i + 1, CALC_ITERS);
        TIC;
        shm_shift__analyse_csv(shm, ECI, ref_col, ni);      /* run ECI algorithm */
        TOC;

        df = diff(start, end);
        eci_ms += ((float)df.tv_nsec / 1000.0) / shm->cols;
        fush_cpu_cache();
    }
    printf("\n");
    shm_shift__write_csv(shm, file_out, delim);             /* write csv */
    eci_ms /= (float)CALC_ITERS;
    printf("ECI duration: %.3f us\n", eci_ms);

    /** Calc mean and get final algorithm ratio speed */
    float mean_ms = 0;
    float ref_mean;
    for (int i = 0; i < CALC_ITERS; i++)
    {
        TIC;
        ref_mean = mean(shm->signals[0].data, shm->signals[0].data_cnt);
        TOC;
        df = diff(start, end);
        mean_ms += (float)df.tv_nsec / 1000.0;
    }
    mean_ms /= (float)CALC_ITERS;
    printf("\nref signal mean: %.3f, duration: %.3f us\n", ref_mean, mean_ms);

    /** Display measured values */
    float pli_ratio = pli_ms / mean_ms;
    float csi_ratio = csi_ms / mean_ms;
    float eci_ratio = eci_ms / mean_ms;

    printf("PLI duration: %.3f us, speed ratio: %.3f\n", pli_ms, pli_ratio);
    printf("CSI duration: %.3f us, speed ratio: %.3f\n", csi_ms, csi_ratio);
    printf("ECI duration: %.3f us, speed ratio: %.3f\n", eci_ms, eci_ratio);

    /** Write durations to separate file */
    FILE * f_dur;
    char path_dur[256];
    sprintf(path_dur, "%s%s", file_out, "_durations.csv");/* write durations to csv */
    f_dur = fopen(path_dur,"w");
    fprintf (f_dur, "pli_ms;csi_ms;eci_ms;mean_ms;pli;csi;eci\n%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;",
        pli_ms, csi_ms, eci_ms, mean_ms, pli_ratio, csi_ratio, eci_ratio);
    fclose(f_dur);

    shm_shift__free(shm);                                 /* delete all objects */
    return 0;
}

static float mean(int* y, int len)
{
    float sum = 0;;
    for(int i = 0; i < len; i++)
        sum += y[i];
    return sum / (float)len;
}

static struct timespec diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec-start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
}

static void fush_cpu_cache()
{
    const int size = 20*1024*50;
    char *c = (char *)malloc(size);
    for (int i = 0; i < 10; i++)
        for (int j = 0; j < size; j++)
            c[j] = i*j;
    free(c);
}
