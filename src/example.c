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
#include <ctype.h>
#include <libgen.h>

#include "../include/shm_shift.h"

#define TEST_DATA       	"./data/data.csv"
#define OUT_DIR       	    "./data/"
#define DEFAULT_DELIM   	';'
#define DEFAULT_INTERP_N 	10
#define DEFAULT_REF_COL     0

#define TICK clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start)
#define TOCK clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end)

static float mean(int* y, int len);
static struct timespec diff(struct timespec start, struct timespec end);


int main(int argc, char **argv)
{
    char* file_in = NULL;
    char* dir_out = NULL;
    int ch;
    char delim = DEFAULT_DELIM;
    int ni = DEFAULT_INTERP_N;
    int ref_col = DEFAULT_REF_COL;
    opterr = 0;

    while ((ch = getopt (argc, argv, "abc:")) != -1) // parse args
    {
        switch (ch)
        {
            case 'f':
                file_in = optarg;       /** input csv data file */
                break;
            case 'o':
                dir_out = optarg;       /** out dir */
                break;
            case 'd':
                delim = optarg[0];      /** csv delimiter */
                break;
            case 'i':
                ni = atoi(optarg);      /** ratio of num of interpolated points to one data x len*/
                break;
            case 'r':
                ref_col = atoi(optarg); /** reference signal column */
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

    if (file_in == NULL)
        file_in = TEST_DATA;

    char file_out[255];
    char* file_in_base = basename(file_in);
    sprintf(file_out, "%s%s", (dir_out == NULL ? OUT_DIR : dir_out), file_in_base);

    char *endch = file_out + strlen(file_out);
    while (endch > file_out && *endch != '.') --endch;
    if (endch > file_out)
        *endch = '\0';

    char cwd[100];
    getcwd(cwd, sizeof(cwd));

    printf("data: %s\n", file_in);
    printf("cwd: %s\n", cwd);

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
    TICK;
    int ret2 = shm_shift__analyse_csv(shm, PLI, ref_col, ni);      /* run 1.st algorithm */
    TOCK;

    int ret3 = shm_shift__write_csv(shm, file_out, delim);  /* write csv */
    shm_shift__dispose_calc(shm);

    df = diff(start, end);
    float pli_ms = ((float)df.tv_nsec / 1000000.0) / shm->cols;
    printf("PLI duration: %.3f ms\n\n", pli_ms);

    /** Cubic Spline Interpolation */
    printf("======== CSI ========\n\n");
    TICK;
    int ret4 = shm_shift__analyse_csv(shm, CSI, ref_col, ni);      /* run 2.nd algorithm */
    TOCK;

    int ret5 = shm_shift__write_csv(shm, file_out, delim);  /* write csv */
    shm_shift__dispose_calc(shm);

    df = diff(start, end);
    float csi_ms = ((float)df.tv_nsec / 1000000.0) / shm->cols;
    printf("CSI duration: %.3f ms\n\n", csi_ms);


    /** Extremum Center Inerpolation */
    printf("======== ECI ========\n\n");
    TICK;
    int ret6 = shm_shift__analyse_csv(shm, ECI, ref_col, ni);      /* run 3.rd algorithm */
    TOCK;

    int ret7 = shm_shift__write_csv(shm, file_out, delim);  /* write csv */

    df = diff(start, end);
    float eci_ms = ((float)df.tv_nsec / 1000000.0) / shm->cols;
    printf("ECI duration: %.3f ms\n", eci_ms);

    /** Calc mean and get final algorithm ratio speed */
    TICK;
    float ref_mean = mean(shm->signals[0].data, shm->signals[0].data_cnt);
    TOCK;
    df = diff(start, end);
    float mean_ms = (float)df.tv_nsec / 1000000.0;
    printf("\nref signal mean: %.3f, duration: %.5f us\n", ref_mean, mean_ms);

    float pli_ratio = pli_ms / mean_ms;
    float csi_ratio = csi_ms / mean_ms;
    float eci_ratio = eci_ms / mean_ms;

    printf("PLI speed ratio: %.3f\n", pli_ratio);
    printf("CSI speed ratio: %.3f\n", csi_ratio);
    printf("ECI speed ratio: %.3f\n", eci_ratio);

    /** Write durations to separate file */
    FILE * f_dur;
    char path_dur[256];
    sprintf(path_dur, "%s%s", file_out, "_durations.csv");/* write durations to csv */
    f_dur = fopen(path_dur,"w");
    fprintf (f_dur, "pli_ms;csi_ms;eci_ms;mean_ms;pli;csi;eci\n%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;",
        pli_ms, csi_ms, eci_ms, mean_ms, pli_ratio, csi_ratio, eci_ratio);
    fclose(f_dur);

    shm_shift__free(shm);                                   /* delete objects */
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
