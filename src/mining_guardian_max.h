#pragma once

#include <Arduino.h>

#define NERDNOS_JOB_INTERVAL_MS 30
#define ASIC_JOB_COUNT 32

struct AsicWorkSnapshot {
  String job_id;
  String prev_block_hash;
  String coinb1;
  String coinb2;
  String nbits;
  String version;
  String ntime;
  String extranonce1;
  int extranonce2_size;
  char wName[80];
  String merkle[32];
  size_t merkle_count;
  uint32_t difficulty;
};

void asicJobsInit();
void runASIC(void *task_id);
void runASIC_RX(void *task_id);
double nerdnos_get_avg_hashrate();
double nerdnos_get_total_mhashes();

void miningPublishAsicWork();
bool miningCopyAsicWork(AsicWorkSnapshot &out);
WiFiClient &miningPoolClient();

extern volatile uint32_t g_asicJobEpoch;
