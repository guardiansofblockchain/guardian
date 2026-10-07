#ifdef NERD_NOS
#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "mining.h"
#include "mining_guardian_max.h"
#include "stratum.h"
#include "monitor.h"
#include "drivers/displays/display.h"
#include "drivers/devices/device.h"
#include "drivers/guardian-max/nerdnos.h"
#include "drivers/guardian-max/adc.h"
#include "drivers/guardian-max/bm1397.h"

extern double best_diff;
extern unsigned long mLastTXtoPool;
extern uint32_t shares;
extern monitor_data mMonitor;

#define REQUEST_ASIC_HASHRATE
#define MAX_SAFE_TEMP 80.0f
#define TEMP_CHECK_INTERVAL 5000

static bm_job_t asic_jobs[ASIC_JOB_COUNT] = {0};
static SemaphoreHandle_t asic_job_mutexes[ASIC_JOB_COUNT];

#define ASIC_HISTORY_SIZE 128

typedef struct {
  uint32_t diffs[ASIC_HISTORY_SIZE];
  uint32_t timestamps[ASIC_HISTORY_SIZE];
  uint32_t newest;
  uint32_t oldest;
  uint64_t sum;
  double avg_gh;
  double duration;
  int share_count;
} history_t;

static volatile uint32_t version_mask_base = 0x20000000;
static history_t history = {0};
static uint32_t last_request_hashrate_ts = 0;
// Sum of pool difficulty of every accepted share since boot. Each unit of
// difficulty represents 2^32 hashes on average, so this estimates the total
// work the ASIC has done (the ESP itself does not hash on Guardian MAX).
static uint64_t accepted_diff_total = 0;

double nerdnos_get_avg_hashrate()
{
  return history.avg_gh;
}

double nerdnos_get_total_mhashes()
{
  return (double)accepted_diff_total * 4294.967296; // diff * 2^32 / 1e6
}

static void safe_free_job(bm_job_t *job)
{
  if (job && job->ntime)
  {
    nerdnos_free_bm_job(job);
    job->ntime = 0;
  }
}

static void calculate_hashrate(history_t *item, uint32_t diff)
{
  if (item->newest + 1 >= ASIC_HISTORY_SIZE)
  {
    item->sum -= item->diffs[item->oldest % ASIC_HISTORY_SIZE];
    item->oldest++;
  }
  item->sum += diff;
  item->diffs[item->newest % ASIC_HISTORY_SIZE] = diff;
  item->timestamps[item->newest % ASIC_HISTORY_SIZE] = millis();

  uint32_t oldest_timestamp = item->timestamps[item->oldest % ASIC_HISTORY_SIZE];
  uint32_t newest_timestamp = item->timestamps[item->newest % ASIC_HISTORY_SIZE];
  item->duration = (double)(newest_timestamp - oldest_timestamp) / 1.0e3;
  item->share_count = (int)item->newest - (int)item->oldest + 1;

  if (item->duration)
  {
    double avg = (double)(item->sum << 32llu) / item->duration;
    item->avg_gh = avg / 1.0e9;
  }
  item->newest++;
  shares++;
  accepted_diff_total += diff;
}

static void process_hashrate_response(task_result *result)
{
  static float hashrate_long = 0.0f;
  static float hashrate_short = 0.0f;
  float hr_gh = (float)((uint64_t)(result->data & 0x7fffffff) << 20llu) / 1.0e9f;
  if (result->data & 0x80000000)
    hashrate_long = hr_gh;
  else
    hashrate_short = hr_gh;
  Serial.printf("hashrate reported by asic: %.3fGH (short), %.3fGH (long)\n", hashrate_short, hashrate_long);
}

void runASIC_RX(void *task_id)
{
  (void)task_id;
  while (1)
  {
    task_result result = {0};
    if (!nerdnos_proccess_work(version_mask_base, 10000, &result))
      continue;

    if (result.is_reg_resp)
    {
      if (result.reg == 0x04)
        process_hashrate_response(&result);
      continue;
    }
    if (result.job_id >= ASIC_JOB_COUNT)
    {
      Serial.printf("Invalid job ID %02x\n", result.job_id);
      continue;
    }

    xSemaphoreTake(asic_job_mutexes[result.job_id], portMAX_DELAY);
    if (!asic_jobs[result.job_id].ntime)
    {
      Serial.printf("No job found for ID %02x\n", result.job_id);
      xSemaphoreGive(asic_job_mutexes[result.job_id]);
      continue;
    }

    uint8_t hash[32];
    double diff_hash = nerdnos_test_nonce_value(&asic_jobs[result.job_id], result.nonce, result.rolled_version, hash);
    if (diff_hash > best_diff)
      best_diff = diff_hash;

    if (diff_hash >= asic_jobs[result.job_id].pool_diff)
    {
      calculate_hashrate(&history, asic_jobs[result.job_id].pool_diff);
      Serial.printf("avg hashrate: %.2fGH/s (history spans %.2fs, %d shares)\n", history.avg_gh, history.duration, history.share_count);
    }

    AsicWorkSnapshot work;
    bool have_work = miningCopyAsicWork(work);
    if (have_work && diff_hash > work.difficulty)
    {
      mining_subscribe worker = init_mining_subscribe();
      strncpy(worker.wName, work.wName, sizeof(worker.wName) - 1);
      tx_mining_submit_asic(miningPoolClient(), worker, &asic_jobs[result.job_id], &result);
      Serial.println("valid share!");
      Serial.printf("   - Current diff share: %.3f\n", diff_hash);
      Serial.printf("   - Current pool diff : %lu\n", work.difficulty);
      mLastTXtoPool = millis();
    }
    xSemaphoreGive(asic_job_mutexes[result.job_id]);
  }
}

void asicJobsInit()
{
  for (int i = 0; i < ASIC_JOB_COUNT; i++)
  {
    if (asic_job_mutexes[i] == NULL)
      asic_job_mutexes[i] = xSemaphoreCreateMutex();
  }
}

void runASIC(void *task_id)
{
  (void)task_id;
  Serial.printf("[MINER] Started runASIC Task!\n");
  asicJobsInit();

  uint32_t extranonce_2 = 0;
  unsigned long lastTempCheck = 0;
  uint32_t seen_epoch = 0;

  while (1)
  {
    if (g_asicJobEpoch == seen_epoch)
    {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }
    seen_epoch = g_asicJobEpoch;

    AsicWorkSnapshot work;
    if (!miningCopyAsicWork(work))
      continue;

    version_mask_base = strtoul(work.version.c_str(), NULL, 16);
    mMonitor.NerdStatus = NM_hashing;
    Serial.println(">>> STARTING TO HASH NONCES");

    uint32_t current_difficulty = 0;
    while (g_asicJobEpoch == seen_epoch)
    {
      vTaskDelay(NERDNOS_JOB_INTERVAL_MS / portTICK_PERIOD_MS);
      if (g_asicJobEpoch != seen_epoch)
        break;

      unsigned long currentTime = millis();
      if (currentTime - lastTempCheck > TEMP_CHECK_INTERVAL)
      {
        float currentTemp = nerdnos_get_temperature();
        if (currentTemp > MAX_SAFE_TEMP)
        {
          gpio_set_level(NERD_NOS_GPIO_PEN, 0);
          Serial.println("ASIC temperature too high. Disabling power.");
          while (nerdnos_get_temperature() > (MAX_SAFE_TEMP - 10))
            vTaskDelay(2000 / portTICK_PERIOD_MS);
          gpio_set_level(NERD_NOS_GPIO_PEN, 1);
          Serial.println("Temperature safe. Re-enabling ASIC.");
          BM1397_init(210, 1);
        }
        lastTempCheck = currentTime;
      }

      extranonce_2++;
      uint8_t asic_job_id = (uint8_t)(extranonce_2 % ASIC_JOB_COUNT);

#ifdef REQUEST_ASIC_HASHRATE
      if (millis() - last_request_hashrate_ts > 2500)
      {
        BM1397_read_hashrate();
        last_request_hashrate_ts = millis();
      }
#endif

      if (!miningCopyAsicWork(work))
        break;

      xSemaphoreTake(asic_job_mutexes[asic_job_id], portMAX_DELAY);
      safe_free_job(&asic_jobs[asic_job_id]);
      nerdnos_create_job(work.extranonce1.c_str(), work.extranonce2_size, work.coinb1.c_str(), work.coinb2.c_str(),
                         work.job_id.c_str(), work.prev_block_hash.c_str(), work.nbits.c_str(), work.ntime.c_str(),
                         work.version.c_str(), work.merkle, work.merkle_count, extranonce_2, work.difficulty,
                         &asic_jobs[asic_job_id]);

      if (current_difficulty != asic_jobs[asic_job_id].pool_diff)
      {
        current_difficulty = asic_jobs[asic_job_id].pool_diff;
        nerdnos_set_asic_difficulty(current_difficulty);
        Serial.printf("Set difficulty to %lu\n", current_difficulty);
      }
      nerdnos_send_work(&asic_jobs[asic_job_id], asic_job_id);
      xSemaphoreGive(asic_job_mutexes[asic_job_id]);
    }

    Serial.println("MINER WORK ABORTED >> waiting new job");
    for (int i = 0; i < ASIC_JOB_COUNT; i++)
    {
      xSemaphoreTake(asic_job_mutexes[i], portMAX_DELAY);
      safe_free_job(&asic_jobs[i]);
      xSemaphoreGive(asic_job_mutexes[i]);
    }
  }
}

#endif
